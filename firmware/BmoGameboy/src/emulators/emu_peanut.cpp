#pragma GCC optimize("O3,unroll-loops")
#include "emu_peanut.h"
#include "buttons.h"
#include "display_emu.h"
#include "bmo_face.h"
#include <string.h>
#include <Arduino.h>
#include <esp_heap_caps.h>

#include "../vendor/peanut_gb/peanut_gb_config.h"

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

namespace PGB {
  #include "../vendor/peanut_gb/peanut_gb.h"
}
using namespace PGB;

namespace {
  // Stub audio callbacks moved above
  
  // E3: Align the emulator state struct to the ESP32-S3 D-cache line
  // size (32 bytes). Prevents cache thrashing on hot registers.
  static struct gb_s __attribute__((aligned(32))) gb;
  
  const uint8_t* current_rom_data = nullptr;
  size_t current_rom_len = 0;
  
  // Keep cartridge save RAM out of scarce internal DRAM.  128 KB covers the
  // larger MBC5/CGB save configurations; PSRAM latency is acceptable here
  // because cartridge-RAM access is far less frequent than CPU registers.
  static constexpr size_t CART_RAM_SIZE = 128 * 1024;
  static uint8_t* cart_ram = nullptr;

  // N5: Module-level, 4-byte aligned — avoids BSS contention with gb_s data.
  static uint16_t __attribute__((aligned(4))) rowBuffer[480];

  static uint16_t PAL_256[256];

  // N1: IRAM_ATTR — these callbacks are invoked ~280K times per frame by the
  // emulator core; zero-wait-state IRAM placement eliminates I-cache stalls.

  IRAM_ATTR uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr >= current_rom_len) return 0xFF;
    return current_rom_data[addr];
  }
  
  IRAM_ATTR uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
    if (!cart_ram || addr >= CART_RAM_SIZE) {
      static bool warned = false;
      if (!warned) { Serial.println("WARNING: Cart RAM read overflow!"); warned = true; }
      return 0xFF;
    }
    return cart_ram[addr];
  }
  
  IRAM_ATTR void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    if (!cart_ram || addr >= CART_RAM_SIZE) {
      static bool warned = false;
      if (!warned) { Serial.println("WARNING: Cart RAM write overflow!"); warned = true; }
      return;
    }
    cart_ram[addr] = val;
  }
  
  void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t val) {
    Serial.printf("Peanut-GB Error: %d at PC 0x%04X\n", gb_err, val);
    Serial.flush();
    BmoFace::setExpression(BmoFace::ERROR);
    BmoFace::draw(); // Force draw immediately before restart
    esp_restart();
  }

  // N1: lcd_draw_line in IRAM — called 144×/frame.
  IRAM_ATTR void lcd_draw_line(struct gb_s *gb, const uint8_t pixels[160], const uint_fast8_t line) {
    // E1: Process 4 source pixels per iteration → 6 output pixels.
    // This perfectly aligns to three 32-bit memory stores (12 bytes),
    // eliminating unaligned store penalties on the Xtensa LX7 CPU.
    uint32_t* out32 = (uint32_t*)rowBuffer;
    for (int g = 0; g < 40; g++) {
      uint16_t pA = PAL_256[pixels[g * 4]];
      uint16_t pB = PAL_256[pixels[g * 4 + 1]];
      uint16_t pC = PAL_256[pixels[g * 4 + 2]];
      uint16_t pD = PAL_256[pixels[g * 4 + 3]];
      
      out32[0] = (uint32_t)pA | ((uint32_t)pA << 16);
      out32[1] = (uint32_t)pB | ((uint32_t)pC << 16);
      out32[2] = (uint32_t)pC | ((uint32_t)pD << 16);
      
      out32 += 3;
    }

    // N3: No setAddrWindow per line — startFrame() set it once for 240×216.
    // Exact 3:2 nearest-neighbour scale. Horizontally, A B C D becomes
    // A A B C C D; duplicating the even source rows gives the identical
    // top-left anchored mapping vertically (0,0,1,2,2,3,...).
    const int rows_to_draw = (line & 1u) ? 1 : 2;
    if (rows_to_draw == 2) {
      memcpy(&rowBuffer[240], &rowBuffer[0], 240 * 2);
    }
    DisplayEmu::streamPixelRow(rowBuffer, 240 * rows_to_draw);
  }
}

bool PeanutEmu::begin(const uint8_t* rom_data, size_t rom_len) {
  current_rom_data = rom_data;
  current_rom_len = rom_len;

  // Precompute O(1) 256-entry palette lookup table to save bitwise ops in hot loop
  for (int i = 0; i < 256; i++) {
    PAL_256[i] = DisplayEmu::CLASSIC_PALETTE[i & 0x03];
  }

  if (!cart_ram) {
    cart_ram = (uint8_t*)heap_caps_malloc(CART_RAM_SIZE, MALLOC_CAP_SPIRAM);
    if (!cart_ram) {
      Serial.println("Peanut-GB: unable to allocate PSRAM cartridge RAM.");
      return false;
    }
  }

  // Clear cart RAM to prevent save-data bleed between games.
  memset(cart_ram, 0, CART_RAM_SIZE);

  Serial.println("Initializing Peanut-GB...");
  Serial.printf("ROM loaded: %u bytes\n", rom_len);
  
  enum gb_init_error_e ret = gb_init(&gb, &gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write,
                                     &gb_error, nullptr);
  if (ret != GB_INIT_NO_ERROR) {
    Serial.printf("gb_init() failed: %d\n", ret);
    return false;
  }
  
  gb_init_lcd(&gb, &lcd_draw_line);

  // Reset joypad so no button fires on the first emulator frame.
  gb.direct.joypad = 0xFF;
  
  Serial.println("Peanut-GB initialized.");
  return true;
}

void PeanutEmu::updateJoypad() {
  // NB2: Single branchless bitmask direct copy - zero function call overhead.
  gb.direct.joypad = Buttons::gb_joypad_state;
}

void PeanutEmu::runFrame() {
  // startFrame holds SPI bus open and sets address window once for the
  // entire 240×216 frame (N1 + N3).
  DisplayEmu::startFrame();
  gb_run_frame(&gb);
  DisplayEmu::endFrame();
}

void PeanutEmu::destroy() {
  if (cart_ram) {
    free(cart_ram);
    cart_ram = nullptr;
  }
}
