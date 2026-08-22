#include "emulator.h"
#include "buttons.h"
#include "config.h"
#if USE_TEST_ROM
#include "rom_data_test.h"
#else
#include "rom_data.h"
#endif
#include "display_emu.h"

// Peanut-GB configuration
#define ENABLE_SOUND 0
#define ENABLE_LCD 1

// Implement standard types for Peanut-GB
#include <stdint.h>
#include <stdlib.h>

// Include the library
#include "peanut_gb.h"

namespace {
  struct gb_s gb;
  uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr >= rom_data_len) return 0xFF; // Bounds check
    return rom_data[addr]; // Direct access, ESP32 doesn't need PROGMEM
  }
  
  uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
    // TODO: Cart RAM is fully stubbed. Progress/Saves will NOT persist
    // between reboots until SD card support is implemented.
    return 0xFF;
  }
  
  void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    // TODO: Cart RAM is fully stubbed. 
  }
  
  void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t val) {
    Serial.printf("Peanut-GB Error: %d at PC 0x%04X\n", gb_err, val);
  }
}

bool Emulator::begin() {
  Serial.println("Initializing Peanut-GB...");
  Serial.printf("ROM loaded: %u bytes\n", rom_data_len);
  
  // Init context
  enum gb_init_error_e ret = gb_init(&gb, &gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write, &gb_error, nullptr);
  
  if (ret != GB_INIT_NO_ERROR) {
    Serial.printf("gb_init() failed: %d\n", ret);
    return false;
  }
  
  // Connect the LCD callback
  gb_init_lcd(&gb, &DisplayEmu::drawScanline);
  
  Serial.println("Peanut-GB initialized.");
  return true;
}

void Emulator::updateJoypad() {
  // Peanut-GB bits: 0 = pressed, 1 = released
  // Our button state: pressed = true, released = false
  // Since we want 0 for pressed, we invert the pressed state using !
  
  uint8_t joypad = 0xFF; // All released
  
  if (Buttons::get(Buttons::UP).pressed) joypad &= ~JOYPAD_UP;
  if (Buttons::get(Buttons::DOWN).pressed) joypad &= ~JOYPAD_DOWN;
  if (Buttons::get(Buttons::LEFT).pressed) joypad &= ~JOYPAD_LEFT;
  if (Buttons::get(Buttons::RIGHT).pressed) joypad &= ~JOYPAD_RIGHT;
  
  if (Buttons::get(Buttons::A).pressed) joypad &= ~JOYPAD_A;
  if (Buttons::get(Buttons::B).pressed) joypad &= ~JOYPAD_B;
  if (Buttons::get(Buttons::START).pressed) joypad &= ~JOYPAD_START;
  if (Buttons::get(Buttons::SELECT).pressed) joypad &= ~JOYPAD_SELECT;

  gb.direct.joypad = joypad;
}

void Emulator::runFrame() {
  gb_run_frame(&gb);
}
