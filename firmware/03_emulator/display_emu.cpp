#pragma GCC optimize ("O3")
#include "display_emu.h"
#include "config.h"
#include <SPI.h>
#include <Adafruit_ST7789.h>
#include <cstring>

#include "peanut_gb_config.h"
#define PEANUT_GB_HEADER_ONLY
#include "peanut_gb.h"

namespace {
  Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
  
  // Game Boy native is 160x144. Scaled 1.5x it becomes 240x216.
  // Center vertically on the 320px tall display.
  const int OFFSET_Y = (TFT_HEIGHT - 216) / 2;

  // Classic Game Boy "Pea-Soup Green" palette in RGB565.
  // WARNING: These values are PRE-SWAPPED for ESP32 Little-Endian SPI!
  // If you use Adafruit_ST7789 drawing functions (drawPixel, fillRect, etc.),
  // do NOT use this palette directly, as Adafruit_GFX handles endianness itself
  // and the colors will appear corrupted. This is only for raw SPI.writeBytes().
  const uint16_t PALETTE[4] = {
    0xE19D, // Swapped 0x9DE1
    0x618D, // Swapped 0x8D61
    0x0633, // Swapped 0x3306
    0xC109  // Swapped 0x09C1
  };
}

void DisplayEmu::begin() {
  tft.init(TFT_WIDTH, TFT_HEIGHT);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
}

void DisplayEmu::drawScanline(struct gb_s *gb, const uint8_t pixels[160], const uint_fast8_t line) {
  static uint16_t rowBuffer[480]; // up to 2 rows of 240 pixels, static to save stack space
  
  // 1.5x Horizontal scaling: map 160 pixels to 240
  for (int x = 0; x < 240; x++) {
    int src_x = (x * 2) / 3;
    uint8_t color_idx = pixels[src_x] & 0x03; 
    rowBuffer[x] = PALETTE[color_idx];
  }
  
  // 1.5x Vertical scaling
  int out_y = (line * 3) / 2;
  int rows_to_draw = (line % 2 == 1) ? 2 : 1;
  
  if (rows_to_draw == 2) {
    memcpy(&rowBuffer[240], &rowBuffer[0], 240 * 2); // clone row
  }
  
  // Optimized raw SPI transaction
  tft.startWrite();
  tft.setAddrWindow(0, OFFSET_Y + out_y, 240, rows_to_draw);
  SPI.writeBytes((const uint8_t*)rowBuffer, 240 * rows_to_draw * 2);
  tft.endWrite();
}

void DisplayEmu::drawMenu(const char** titles, int count, int selectedIndex) {
  tft.fillScreen(ST77XX_BLACK);
  
  // Title
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 10);
  tft.println("BMO Gameboy");
  tft.drawFastHLine(10, 30, 220, ST77XX_GREEN);

  // List of games
  tft.setTextSize(2);
  for (int i = 0; i < count; i++) {
    int y = 50 + (i * 30);
    
    if (i == selectedIndex) {
      tft.setTextColor(ST77XX_YELLOW);
      tft.setCursor(10, y);
      tft.print("> ");
    } else {
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(10, y);
      tft.print("  ");
    }
    tft.println(titles[i]);
  }
}
