#pragma GCC optimize ("O3")
#include "display_emu.h"
#include "config.h"
#include <SPI.h>
#include <Adafruit_ST7789.h>
#include <cstring>
#include <cstdio>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>

// No emulator headers should be included here to prevent ODR violations


namespace {
  Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
  
  // Game Boy native is 160x144. Scaled 1.5x it becomes 240x216.
  // In Landscape mode, the display is 320x240.
  // Center it both horizontally and vertically.
  const int OFFSET_X = (320 - 240) / 2; // 40
  const int OFFSET_Y = (240 - 216) / 2; // 12
}

// Classic Game Boy "Pea-Soup Green" palette in BGR565 (Little-Endian swapped)
// Pre-swapped for BGR physical displays
const uint16_t DisplayEmu::CLASSIC_PALETTE[4] = {
  0xF30D, 
  0x710D, 
  0x0633, 
  0xC109  
};

void DisplayEmu::begin() {
  tft.init(TFT_WIDTH, TFT_HEIGHT);
  tft.setSPISpeed(80000000); // 80 MHz SPI clock for max framerate
  tft.setRotation(3); // Flipped Landscape mode (270 degrees)
  tft.fillScreen(ST77XX_BLACK);
}

void DisplayEmu::clearScreen() {
  tft.fillScreen(ST77XX_BLACK);
}

void DisplayEmu::pushPixels(int yOffset, const uint16_t* rowBuffer, int rowsToDraw) {
  tft.startWrite();
  tft.setAddrWindow(OFFSET_X, OFFSET_Y + yOffset, 240, rowsToDraw);
  SPI.writeBytes((const uint8_t*)rowBuffer, 240 * rowsToDraw * 2);
  tft.endWrite();
}



void DisplayEmu::drawEmulatorSelectMenu(int selectedIndex) {
  tft.fillScreen(0x5E36); // BMO Teal background
  tft.fillRect(0, 0, 320, 30, 0x11E9); // Dark blue top bar
  
  // Title
  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(0xFD84); // Yellow text on dark blue
  
  tft.setCursor(65, 22);
  tft.print("SELECT CONSOLE");

  // Draw Console Graphics
  if (selectedIndex == 0) { // GBC
    tft.fillRect(110, 60, 100, 100, 0xFFFF); // White
    tft.fillRect(120, 70, 80, 20, 0xF800); // Red
    tft.fillRect(120, 90, 80, 20, 0x07E0); // Green
    tft.fillRect(120, 110, 80, 20, 0x001F); // Blue
  } else if (selectedIndex == 1) { // GB
    tft.fillRect(110, 60, 100, 100, 0xCE79); // Grey
    tft.fillRect(120, 70, 80, 20, 0x4208); // Dark Grey
    tft.fillRect(120, 90, 80, 20, 0x8410); // Mid Grey
    tft.fillRect(120, 110, 80, 20, 0xC618); // Light Grey
  } else if (selectedIndex == 2) { // NES
    tft.fillRect(110, 60, 100, 100, 0x8410); // Darker Grey
    tft.fillRect(120, 130, 80, 15, 0xF800); // Red stripe at bottom
    tft.fillRect(140, 75, 40, 40, 0x0000); // Black cartridge label
  } else if (selectedIndex == 3) { // SNES
    tft.fillRect(110, 60, 100, 100, 0xAD75); // Light Grey
    tft.fillRect(110, 60, 100, 20, 0x4208); // Dark grey top
    tft.fillCircle(160, 110, 25, 0x61B7); // Purple circle
  } else if (selectedIndex == 4) { // GBA
    tft.fillRect(110, 70, 100, 80, 0x301A); // Dark Purple
    tft.fillRoundRect(120, 80, 80, 40, 5, 0xFFFF); // White label
  } else { // Sega Genesis
    tft.fillRect(110, 60, 100, 100, 0x0000); // Black
    tft.fillRect(120, 90, 80, 40, 0x0000);
    tft.drawRect(120, 90, 80, 40, 0xF800); // Red outline label
  }
  tft.drawRect(108, 58, 104, 104, 0x11E9); // Border
  tft.drawRect(109, 59, 102, 102, 0x11E9); // Thicker Border

  // Arrows
  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(0xFD84); // Yellow arrows
  tft.setCursor(50, 115);
  tft.print("<");
  tft.setCursor(250, 115);
  tft.print(">");

  // Console Name
  tft.setFont(&FreeSans9pt7b);
  tft.setTextSize(1);
  tft.setTextColor(0x11E9); // Dark blue text
  
  const char* consoleNames[] = {
    "Game Boy Color",
    "Game Boy",
    "Nintendo (NES)",
    "Super Nintendo",
    "Game Boy Advance",
    "Sega Genesis"
  };
  
  const char* name = consoleNames[selectedIndex];
  int textWidth = strlen(name) * 11;
  int curX = (320 - textWidth) / 2;
  if (curX < 0) curX = 0;
  tft.setCursor(curX, 190);
  tft.print(name);
}

void DisplayEmu::showSDCardWarning() {
  tft.fillRect(40, 80, 240, 80, 0x11E9);
  tft.drawRect(42, 82, 236, 76, 0xFD84);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(0xFD84);
  tft.setCursor(55, 115);
  tft.print("SD CARD REQUIRED");
  tft.setCursor(55, 135);
  tft.print("FOR THIS CONSOLE");
}

void DisplayEmu::drawMenu(const char** titles, int count, int selectedIndex, const uint16_t* selectedCover, bool useColorEmulator) {
  tft.fillScreen(0x5E36); // BMO Teal background
  tft.fillRect(0, 0, 320, 30, 0x11E9); // Dark blue top bar
  
  // Title
  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(0xFD84); // Yellow text on dark blue
  tft.setCursor(80, 22);
  tft.print("BMO GAMEBOY");

  // Cover Art centered
  if (selectedCover) {
    tft.drawRGBBitmap(110, 50, selectedCover, 100, 100);
    tft.drawRect(108, 48, 104, 104, 0x11E9); // Dark Blue border
    tft.drawRect(109, 49, 102, 102, 0x11E9); // Thicker border
  }

  // Arrows
  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(0xFD84); // Yellow arrows
  tft.setCursor(50, 105);
  tft.print("<");
  tft.setCursor(250, 105);
  tft.print(">");

  // Game Title
  tft.setFont(&FreeSans9pt7b);
  tft.setTextSize(1);
  tft.setTextColor(0x11E9);
  int titleWidth = strlen(titles[selectedIndex]) * 10;
  // If title is too long, we just do our best
  int cursorX = (320 - titleWidth) / 2;
  if (cursorX < 0) cursorX = 5;
  tft.setCursor(cursorX, 185);
  tft.print(titles[selectedIndex]);

  // Progress indicator and core
  tft.setFont(); // Reset to default 5x7 font for small footer text
  tft.setTextSize(1);
  tft.setTextColor(0x11E9);
  
  char progress[16];
  sprintf(progress, "Game %d of %d", selectedIndex + 1, count);
  tft.setCursor(10, 225);
  tft.print(progress);

  tft.setTextColor(useColorEmulator ? 0xFD84 : 0x11E9); // Highlight if Color is active
  tft.setCursor(240, 225);
  tft.print(useColorEmulator ? "Walnut-CGB" : "Peanut-GB");
}
