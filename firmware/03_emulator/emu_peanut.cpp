#include "emu_peanut.h"
#include "buttons.h"
#include "display_emu.h"

#include "peanut_gb_config.h"

// Implement standard types for Peanut-GB
#include <stdint.h>
#include <stdlib.h>

// Include the library inside a namespace to avoid ODR violations with Walnut-CGB
namespace PGB {
  #include "peanut_gb.h"
}
using namespace PGB;

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
    esp_restart();
  }

  void lcd_draw_line(struct gb_s *gb, const uint8_t pixels[160], const uint_fast8_t line) {
    static uint16_t rowBuffer[480];
    
    for (int x = 0; x < 240; x++) {
      int src_x = (x * 2) / 3;
      uint8_t color_idx = pixels[src_x] & 0x03; 
      uint16_t color = DisplayEmu::CLASSIC_PALETTE[color_idx];
      uint16_t native = (color >> 8) | (color << 8); // Un-swap SPI endianness
      uint16_t r = (native >> 11) & 0x1F;
      uint16_t g = (native >> 5) & 0x3F;
      uint16_t b = native & 0x1F;
      uint16_t bgr = (b << 11) | (g << 5) | r;
      rowBuffer[x] = (bgr >> 8) | (bgr << 8); // Re-swap for SPI
    }

    int out_y = (line * 3) / 2;
    int rows_to_draw = (line % 2 == 1) ? 2 : 1;

    if (rows_to_draw == 2) {
      memcpy(&rowBuffer[240], &rowBuffer[0], 240 * 2);
    }

    DisplayEmu::pushPixels(out_y, rowBuffer, rows_to_draw);
  }
}

bool PeanutEmu::begin(const uint8_t* rom_data, size_t rom_len) {
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
  gb_init_lcd(&gb, &lcd_draw_line);
  
  Serial.println("Peanut-GB initialized.");
  return true;
}

void PeanutEmu::updateJoypad() {
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

void PeanutEmu::runFrame() {
  gb_run_frame(&gb);
}
