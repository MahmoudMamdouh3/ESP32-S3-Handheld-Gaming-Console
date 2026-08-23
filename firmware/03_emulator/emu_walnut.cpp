#pragma GCC optimize ("O3")
#include "emu_walnut.h"
#include "buttons.h"
#include "display_emu.h"

// Define Walnut-CGB configurations
#define ENABLE_LCD 1
#define ENABLE_SOUND 0
#define WALNUT_FULL_GBC_SUPPORT 1
#define WALNUT_GB_RGB565_BIGENDIAN 1

#include <stdint.h>
#include <stdlib.h>

namespace WGB {
  #include "walnut_cgb.h"
}
using namespace WGB;

namespace {
  struct gb_s gb;
  const uint8_t* current_rom_data = nullptr;
  size_t current_rom_len = 0;
  
  static uint8_t cart_ram[32768];

  uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr >= current_rom_len) return 0xFF;
    return current_rom_data[addr];
  }

  uint16_t gb_rom_read16(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr + 1 >= current_rom_len) return 0xFFFF;
    // ESP32 supports unaligned reads, but we can do byte reconstruction for safety
    return current_rom_data[addr] | (current_rom_data[addr + 1] << 8);
  }

  uint32_t gb_rom_read32(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr + 3 >= current_rom_len) return 0xFFFFFFFF;
    return current_rom_data[addr] | 
           (current_rom_data[addr + 1] << 8) | 
           (current_rom_data[addr + 2] << 16) | 
           (current_rom_data[addr + 3] << 24);
  }
  
  uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr >= sizeof(cart_ram)) return 0xFF;
    return cart_ram[addr];
  }
  
  void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    if (addr >= sizeof(cart_ram)) return;
    cart_ram[addr] = val;
  }
  
  void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t val) {
    Serial.printf("Walnut-CGB Error: %d at PC 0x%04X\n", gb_err, val);
    Serial.flush();
    esp_restart();
  }

  void lcd_draw_line(struct gb_s *gb, const uint8_t *pixels, const uint_fast8_t line) {
    static uint16_t rowBuffer[480];
    static uint8_t scale_map[240];
    static bool map_init = false;
    
    if (!map_init) {
      for (int i = 0; i < 240; i++) scale_map[i] = (i * 2) / 3;
      map_init = true;
    }
    
    for (int x = 0; x < 240; x++) {
      int src_x = scale_map[x];
      uint16_t color;
      
      if (gb->cgb.cgbMode) {
        color = gb->cgb.fixPalette[pixels[src_x]];
      } else {
        // GBC automatically colorizes monochrome DMG games!
        // We define 3 distinct palettes (swapped BGR565 for SPI)
        static const uint16_t PAL_BG[4]   = { 0xFFFF, 0x8CF5, 0x0080, 0x0000 }; // White, Light Blue, Dark Blue, Black
        static const uint16_t PAL_OBJ0[4] = { 0xFFFF, 0x1F00, 0x1000, 0x0000 }; // White, Red, Dark Red, Black
        static const uint16_t PAL_OBJ1[4] = { 0xFFFF, 0x0700, 0x0300, 0x0000 }; // White, Green, Dark Green, Black
        
        uint8_t pixel_val = pixels[src_x];
        uint8_t color_idx = pixel_val & 0x03;
        uint8_t pal_type = (pixel_val >> 4) & 0x03; // 0=OBJ0, 1=OBJ1, 2=BG
        
        if (pal_type == 0) color = PAL_OBJ0[color_idx];
        else if (pal_type == 1) color = PAL_OBJ1[color_idx];
        else color = PAL_BG[color_idx];
      }
      
      rowBuffer[x] = color;
    }
    
    int out_y = (line * 3) / 2;
    int rows_to_draw = (line % 2 == 1) ? 2 : 1;
    
    if (rows_to_draw == 2) {
      memcpy(&rowBuffer[240], &rowBuffer[0], 240 * 2);
    }
    
    DisplayEmu::pushPixels(out_y, rowBuffer, rows_to_draw);
  }
}

bool WalnutEmu::begin(const uint8_t* rom_data, size_t rom_len) {
  current_rom_data = rom_data;
  current_rom_len = rom_len;
  
  enum gb_init_error_e ret = gb_init(&gb, gb_rom_read, gb_rom_read16, gb_rom_read32, gb_cart_ram_read, gb_cart_ram_write, gb_error, nullptr);
  if (ret != GB_INIT_NO_ERROR) {
    Serial.printf("Walnut-CGB init error: %d\n", ret);
    return false;
  }
  
  gb_init_lcd(&gb, lcd_draw_line);
  return true;
}

void WalnutEmu::updateJoypad() {
  // Assume Buttons::update() is called by the main loop
  bool up = Buttons::get(Buttons::UP).pressed;
  bool down = Buttons::get(Buttons::DOWN).pressed;
  bool left = Buttons::get(Buttons::LEFT).pressed;
  bool right = Buttons::get(Buttons::RIGHT).pressed;
  bool a = Buttons::get(Buttons::A).pressed;
  bool b = Buttons::get(Buttons::B).pressed;
  bool start = Buttons::get(Buttons::START).pressed;
  bool select = Buttons::get(Buttons::SELECT).pressed;
  
  gb.direct.joypad = 0xFF;
  if (up) gb.direct.joypad &= ~0x40;
  if (down) gb.direct.joypad &= ~0x80;
  if (left) gb.direct.joypad &= ~0x20;
  if (right) gb.direct.joypad &= ~0x10;
  if (a) gb.direct.joypad &= ~0x01;
  if (b) gb.direct.joypad &= ~0x02;
  if (start) gb.direct.joypad &= ~0x08;
  if (select) gb.direct.joypad &= ~0x04;
}

void WalnutEmu::runFrame() {
  gb_run_frame_dualfetch(&gb);
}
