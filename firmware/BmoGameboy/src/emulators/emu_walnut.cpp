#pragma GCC optimize("O3,unroll-loops")
#include "emu_walnut.h"
#include "../core/buttons.h"
#include "../core/display_emu.h"
#include "../core/bmo_face.h"
#include <string.h>
#include <Arduino.h>
#include <esp_heap_caps.h>

// Define Walnut-CGB configurations
#define ENABLE_LCD 1
#define ENABLE_SOUND 1
#define WALNUT_FULL_GBC_SUPPORT 1
#define WALNUT_GB_RGB565_BIGENDIAN 1

#include <stdint.h>
#include <stdlib.h>

#if FEATURE_AUDIO
  extern "C" __attribute__((weak)) uint8_t audio_read(const uint16_t addr) {
    return 0; // Replace with actual APU read if minigb_apu is integrated
  }
  extern "C" __attribute__((weak)) void audio_write(const uint16_t addr, const uint8_t val) {
    // Replace with actual APU write / I2S push
  }
#else
  extern "C" __attribute__((weak)) uint8_t audio_read(const uint16_t addr) { return 0; }
  extern "C" __attribute__((weak)) void audio_write(const uint16_t addr, const uint8_t val) {}
#endif

namespace WGB {
  #include "../engine/walnut_cgb/walnut_cgb.h"
}
using namespace WGB;

namespace {
  // Stub audio callbacks moved above
  // E3: Align the massive emulator state struct to the ESP32-S3 D-cache line
  // size (32 bytes). Prevents the hot cpu_reg struct from straddling two
  // cache lines, which causes a 2x penalty on every register access.
  static struct gb_s __attribute__((aligned(32))) gb;
  
  const uint8_t* current_rom_data = nullptr;
  size_t current_rom_len = 0;
  
  // Place the larger CGB-capable save RAM in PSRAM so both compiled emulator
  // cores do not consume internal DRAM while inactive.
  static constexpr size_t CART_RAM_SIZE = 128 * 1024;
  static uint8_t* cart_ram = nullptr;

  // N5: Module-level, 4-byte aligned, outside the function so it doesn't
  // compete in the BSS region with the hot gb_s struct (~50KB).
  static uint16_t __attribute__((aligned(4))) rowBuffer[480];

  // NB3: Combined 12-entry DMG-on-GBC colorization lookup.
  // Indexed by (pal_type << 2) | color_idx where:
  //   pal_type = (pixel_val >> 4) & 0x03  — 0=OBJ0, 1=OBJ1, 2=BG
  //   color_idx = pixel_val & 0x03
  // Avoids 2 conditional branches per pixel (34,560 branches saved per frame).
  // Values are byte-swapped BGR565 for the physical ST7789 display.
  // Padded to 16 entries to prevent OOB access if pal_type hits 3.
  static const uint16_t DMG_ON_GBC_PAL[16] = {
    // OBJ0 (pal_type 0): White, Red, Dark Red, Black
    0xFFFF, 0x1F00, 0x1000, 0x0000,
    // OBJ1 (pal_type 1): White, Green, Dark Green, Black
    0xFFFF, 0xE007, 0xE003, 0x0000,
    // BG   (pal_type 2): White, Light Blue, Dark Blue, Black
    0xFFFF, 0x8CF5, 0x0080, 0x0000,
    // UNUSED (pal_type 3): Fallback to black
    0x0000, 0x0000, 0x0000, 0x0000,
  };

  static uint16_t DMG_ON_GBC_PAL_256[256];

  // N1: IRAM_ATTR keeps these callback functions in zero-wait-state internal
  // SRAM. They are called ~280,000 times per frame by the emulator core and
  // must never stall on an I-cache miss.

  IRAM_ATTR uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr >= current_rom_len) return 0xFF;
    return current_rom_data[addr];
  }

  IRAM_ATTR uint16_t gb_rom_read16(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr + 2 > current_rom_len) return 0xFFFF;
    return (uint16_t)current_rom_data[addr] | 
          ((uint16_t)current_rom_data[addr + 1] << 8);
  }

  IRAM_ATTR uint32_t gb_rom_read32(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr + 4 > current_rom_len) return 0xFFFFFFFF;
    return (uint32_t)current_rom_data[addr] | 
          ((uint32_t)current_rom_data[addr + 1] << 8) | 
          ((uint32_t)current_rom_data[addr + 2] << 16) | 
          ((uint32_t)current_rom_data[addr + 3] << 24);
  }
  
  IRAM_ATTR uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
    if (!cart_ram || addr >= CART_RAM_SIZE) return 0xFF;
    return cart_ram[addr];
  }
  
  IRAM_ATTR void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    if (!cart_ram || addr >= CART_RAM_SIZE) return;
    cart_ram[addr] = val;
  }
  
  void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t val) {
    Serial.printf("Walnut-CGB Error: %d at PC 0x%04X\n", gb_err, val);
    Serial.flush();
    BmoFace::setExpression(BmoFace::ERROR);
    BmoFace::draw(); // Force draw immediately before restart
    esp_restart();
  }

  // N1: lcd_draw_line is called 144×/frame; IRAM placement eliminates
  // I-cache misses in the pixel-processing inner loop.
  IRAM_ATTR void lcd_draw_line(struct gb_s *gb, const uint8_t *pixels, const uint_fast8_t line) {
    // NB1: Hoist cgbMode out of the 240-pixel inner loop. It is constant for
    // the entire scanline — the compiler cannot prove this without the hoist.
    const uint8_t cgbMode = gb->cgb.cgbMode;

    if (cgbMode) {
      // ---------------------------------------------------------------
      // GBC mode: pixel byte indexes fixPalette[0..63] directly.
      // E1: Process 4 input pixels per iteration → 6 output pixels.
      // This allows exactly three 32-bit memory stores per iteration,
      // which means every store is perfectly 4-byte aligned. The previous
      // 2-pixel version performed unaligned 32-bit stores on odd iterations,
      // incurring Xtensa LX7 load/store stall penalties.
      // ---------------------------------------------------------------
      const uint16_t* pal = gb->cgb.fixPalette;
      uint32_t* out32 = (uint32_t*)rowBuffer;

      for (int g = 0; g < 40; g++) {
        // Read 4 inputs: A, B, C, D
        uint16_t pA = pal[pixels[g * 4]     & 0x3F];
        uint16_t pB = pal[pixels[g * 4 + 1] & 0x3F];
        uint16_t pC = pal[pixels[g * 4 + 2] & 0x3F];
        uint16_t pD = pal[pixels[g * 4 + 3] & 0x3F];
        
        // Output 6 outputs: A, A, B, C, C, D
        // Store 1: [A] [A]
        // Store 2: [C] [B] (Little-endian layout: B goes into low 16 bits, C into high)
        // Store 3: [D] [C]
        out32[0] = (uint32_t)pA | ((uint32_t)pA << 16);
        out32[1] = (uint32_t)pB | ((uint32_t)pC << 16);
        out32[2] = (uint32_t)pC | ((uint32_t)pD << 16);
        
        out32 += 3;
      }
    } else {
      // ---------------------------------------------------------------
      // DMG-on-GBC colorisation: use combined 12-entry lookup to avoid
      // 2 branches per pixel (NB3). Same E1 4-pixel perfectly aligned stores.
      // ---------------------------------------------------------------
      uint32_t* out32 = (uint32_t*)rowBuffer;
      
      for (int g = 0; g < 40; g++) {
        // DMG-on-GBC colorisation: use 256-entry lookup to completely avoid
        // shift and mask operations per pixel.
        uint16_t pA = DMG_ON_GBC_PAL_256[pixels[g * 4]];
        uint16_t pB = DMG_ON_GBC_PAL_256[pixels[g * 4 + 1]];
        uint16_t pC = DMG_ON_GBC_PAL_256[pixels[g * 4 + 2]];
        uint16_t pD = DMG_ON_GBC_PAL_256[pixels[g * 4 + 3]];
        
        out32[0] = (uint32_t)pA | ((uint32_t)pA << 16);
        out32[1] = (uint32_t)pB | ((uint32_t)pC << 16);
        out32[2] = (uint32_t)pC | ((uint32_t)pD << 16);
        
        out32 += 3;
      }
    }
    
    // N3: stream directly — address window is set once per frame in startFrame().
    // Exact 3:2 nearest-neighbour scale.  Match the horizontal phase
    // (A B C D -> A A B C C D) by duplicating even source rows.
    const int rows_to_draw = (line & 1u) ? 1 : 2;
    if (rows_to_draw == 2) {
      memcpy(&rowBuffer[240], &rowBuffer[0], 240 * 2);
    }
    DisplayEmu::streamPixelRow(rowBuffer, 240 * rows_to_draw);
  }
}

bool WalnutEmu::begin(const uint8_t* rom_data, size_t rom_len) {
  current_rom_data = rom_data;
  current_rom_len = rom_len;

  // Populate the 256-entry DMG-on-GBC palette lookup table
  for (int i = 0; i < 256; i++) {
    DMG_ON_GBC_PAL_256[i] = DMG_ON_GBC_PAL[(((i >> 4) & 0x03) << 2) | (i & 0x03)];
  }

  if (!cart_ram) {
    cart_ram = (uint8_t*)heap_caps_malloc(CART_RAM_SIZE, MALLOC_CAP_SPIRAM);
    if (!cart_ram) {
      Serial.println("Walnut-CGB: unable to allocate PSRAM cartridge RAM.");
      return false;
    }
  }

  // Clear cart RAM so previous game's save data cannot bleed into the next.
  memset(cart_ram, 0, CART_RAM_SIZE);
  
  enum gb_init_error_e ret = gb_init(&gb,
    gb_rom_read, gb_rom_read16, gb_rom_read32,
    gb_cart_ram_read, gb_cart_ram_write,
    gb_error, nullptr);
  if (ret != GB_INIT_NO_ERROR) {
    Serial.printf("Walnut-CGB init error: %d\n", ret);
    return false;
  }
  
  gb_init_lcd(&gb, lcd_draw_line);

  // Reset joypad so no button press fires on the very first emulator frame.
  gb.direct.joypad = 0xFF;

  return true;
}

void WalnutEmu::updateJoypad() {
  // NB2: Single branchless bitmask direct copy - zero function call overhead.
  gb.direct.joypad = Buttons::gb_joypad_state;
}

void WalnutEmu::runFrame() {
  // startFrame holds the SPI bus open and sets the address window once for
  // the entire 240×216 frame (N1 + N3).
  DisplayEmu::startFrame();
  gb_run_frame_dualfetch(&gb);
  DisplayEmu::endFrame();
}

void WalnutEmu::destroy() {
  if (cart_ram) {
    free(cart_ram);
    cart_ram = nullptr;
  }
}
