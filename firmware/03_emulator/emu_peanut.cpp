#pragma GCC optimize("O3,unroll-loops")
#include "emu_peanut.h"
#include "buttons.h"
#include "display_emu.h"
#include <string.h>
#include <Arduino.h>

#include "peanut_gb_config.h"

#include <stdint.h>
#include <stdlib.h>

namespace PGB {
  #include "peanut_gb.h"
}
using namespace PGB;

namespace {
  // E3: Align the emulator state struct to the ESP32-S3 D-cache line
  // size (32 bytes). Prevents cache thrashing on hot registers.
  static struct gb_s __attribute__((aligned(32))) gb;
  
  const uint8_t* current_rom_data = nullptr;
  size_t current_rom_len = 0;
  
  // 32KB covers every real-world licensed Game Boy cartridge MBC1 RAM.
  static uint8_t cart_ram[32768];

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
    if (addr >= sizeof(cart_ram)) {
      static bool warned = false;
      if (!warned) { Serial.println("WARNING: Cart RAM read overflow!"); warned = true; }
      return 0xFF;
    }
    return cart_ram[addr];
  }
  
  IRAM_ATTR void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    if (addr >= sizeof(cart_ram)) {
      static bool warned = false;
      if (!warned) { Serial.println("WARNING: Cart RAM write overflow!"); warned = true; }
      return;
    }
    cart_ram[addr] = val;
  }
  
  void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t val) {
    Serial.printf("Peanut-GB Error: %d at PC 0x%04X\n", gb_err, val);
    Serial.flush();
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
    int rows_to_draw = (line % 2 == 1) ? 2 : 1;
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

  // Clear cart RAM to prevent save-data bleed between games.
  memset(cart_ram, 0, sizeof(cart_ram));

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
