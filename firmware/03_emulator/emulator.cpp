#include "emulator.h"
#include "buttons.h"
#include "display_emu.h"

#include "peanut_gb_config.h"

// Implement standard types for Peanut-GB
#include <stdint.h>
#include <stdlib.h>

// Include the library
#include "peanut_gb.h"

namespace {
  struct gb_s gb;
  
  const uint8_t* current_rom_data = nullptr;
  size_t current_rom_len = 0;
  
  // 32KB (4 banks) covers every real-world licensed Game Boy cartridge's MBC1 RAM configuration.
  // Peanut-GB's own num_ram_banks table technically permits RAM-size header code 0x04 -> 16 banks / 128KB,
  // but that code is not used by any known real cartridge; this buffer intentionally does not cover it.
  static uint8_t cart_ram[32768];

  uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr >= current_rom_len) return 0xFF; // Bounds check
    return current_rom_data[addr]; // Direct access, ESP32 doesn't need PROGMEM
  }
  
  uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr >= sizeof(cart_ram)) {
      static bool warned = false;
      if (!warned) { Serial.println("WARNING: Cart RAM read overflow!"); warned = true; }
      return 0xFF;
    }
    return cart_ram[addr];
  }
  
  void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
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
    // PGB_UNREACHABLE() requires this callback to never return for fatal errors.
    // We use esp_restart() for a deterministic, clean reset rather than entering undefined behavior.
    esp_restart();
  }
}

bool Emulator::begin(const uint8_t* rom_data, size_t rom_len) {
  current_rom_data = rom_data;
  current_rom_len = rom_len;
  
  Serial.println("Initializing Peanut-GB...");
  Serial.printf("ROM loaded: %u bytes\n", rom_len);
  
  // Initialize Peanut-GB
  enum gb_init_error_e ret = gb_init(&gb, &gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write,
                                     &gb_error, nullptr);
  
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
