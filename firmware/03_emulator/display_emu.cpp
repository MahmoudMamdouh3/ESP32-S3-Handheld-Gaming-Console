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

  // Helper: measure the pixel width of a string with the currently active font
  // using Adafruit GFX's getTextBounds(), then return the X cursor that
  // horizontally centers it within a display of 'displayWidth' pixels.
  int centeredX(const char* str, int displayWidth) {
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
    int cx = (displayWidth - (int)w) / 2 - x1;
    return (cx < 0) ? 2 : cx;
  }
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

// ---------------------------------------------------------------------------
// N3 — Single setAddrWindow per frame
// ---------------------------------------------------------------------------
// startFrame() asserts the SPI CS and issues ONE setAddrWindow for the entire
// 240×216 Game Boy viewport. All subsequent streamPixelRow() calls just send
// raw pixel bytes — no DC toggle, no command bytes, no CS glitch.
//
// This reduces setAddrWindow calls from 144/frame → 1/frame.
//
// Protocol:
//   DisplayEmu::startFrame();
//   // lcd_draw_line fires 144×, each calling streamPixelRow()
//   gb_run_frame(...);
//   DisplayEmu::endFrame();
// ---------------------------------------------------------------------------

void DisplayEmu::startFrame() {
  tft.startWrite();
  // Set the address window to exactly the Game Boy's scaled output region.
  // 240 pixels wide × 216 pixels tall, positioned at (OFFSET_X, OFFSET_Y).
  // The ST7789 will auto-advance its write pointer after each pixel pair,
  // so all 144 scanlines can stream contiguously.
  tft.setAddrWindow(OFFSET_X, OFFSET_Y, 240, 216);
}

void DisplayEmu::endFrame() {
  tft.endWrite();
}

// streamPixelRow: bare pixel data push — MUST be called inside startFrame/endFrame.
// pixelCount is the number of uint16_t pixels (240 for a single row, 480 for a
// doubled row). No setAddrWindow, no CS assert/deassert.
void DisplayEmu::streamPixelRow(const uint16_t* buf, int pixelCount) {
  SPI.writeBytes((const uint8_t*)buf, pixelCount * 2);
}

// ---------------------------------------------------------------------------
// Legacy per-scanline push — used for occasional cover-art or menu blits
// where no startFrame/endFrame context exists.
// ---------------------------------------------------------------------------
void DisplayEmu::pushPixels(int yOffset, const uint16_t* rowBuffer, int rowsToDraw) {
  tft.startWrite();
  tft.setAddrWindow(OFFSET_X, OFFSET_Y + yOffset, 240, rowsToDraw);
  SPI.writeBytes((const uint8_t*)rowBuffer, 240 * rowsToDraw * 2);
  tft.endWrite();
}

// pushPixelsRaw: used inside a caller-managed startWrite/endWrite block.
void DisplayEmu::pushPixelsRaw(int yOffset, const uint16_t* rowBuffer, int rowsToDraw) {
  tft.setAddrWindow(OFFSET_X, OFFSET_Y + yOffset, 240, rowsToDraw);
  SPI.writeBytes((const uint8_t*)rowBuffer, 240 * rowsToDraw * 2);
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
    tft.fillRect(110, 60, 100, 100, 0xFFFF);
    tft.fillRect(120, 70, 80, 20, 0xF800);
    tft.fillRect(120, 90, 80, 20, 0x07E0);
    tft.fillRect(120, 110, 80, 20, 0x001F);
  } else if (selectedIndex == 1) { // GB
    tft.fillRect(110, 60, 100, 100, 0xCE79);
    tft.fillRect(120, 70, 80, 20, 0x4208);
    tft.fillRect(120, 90, 80, 20, 0x8410);
    tft.fillRect(120, 110, 80, 20, 0xC618);
  } else if (selectedIndex == 2) { // NES
    tft.fillRect(110, 60, 100, 100, 0x8410);
    tft.fillRect(120, 130, 80, 15, 0xF800);
    tft.fillRect(140, 75, 40, 40, 0x0000);
  } else if (selectedIndex == 3) { // SNES
    tft.fillRect(110, 60, 100, 100, 0xAD75);
    tft.fillRect(110, 60, 100, 20, 0x4208);
    tft.fillCircle(160, 110, 25, 0x61B7);
  } else if (selectedIndex == 4) { // GBA
    tft.fillRect(110, 70, 100, 80, 0x301A);
    tft.fillRoundRect(120, 80, 80, 40, 5, 0xFFFF);
  } else { // Sega Genesis
    tft.fillRect(110, 60, 100, 100, 0x0000);
    tft.fillRect(120, 90, 80, 40, 0x0000);
    tft.drawRect(120, 90, 80, 40, 0xF800);
  }
  tft.drawRect(108, 58, 104, 104, 0x11E9);
  tft.drawRect(109, 59, 102, 102, 0x11E9);

  // Arrows
  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(0xFD84);
  tft.setCursor(50, 115);
  tft.print("<");
  tft.setCursor(250, 115);
  tft.print(">");

  // Console Name — accurate centering via getTextBounds()
  tft.setFont(&FreeSans9pt7b);
  tft.setTextSize(1);
  tft.setTextColor(0x11E9);
  
  const char* consoleNames[] = {
    "Game Boy Color",
    "Game Boy",
    "Nintendo (NES)",
    "Super Nintendo",
    "Game Boy Advance",
    "Sega Genesis"
  };
  
  const char* name = consoleNames[selectedIndex];
  tft.setCursor(centeredX(name, 320), 190);
  tft.print(name);

  // READY / COMING SOON badge (indices 0 & 1 are fully implemented)
  bool isReady = (selectedIndex == 0 || selectedIndex == 1);
  tft.setFont();
  tft.setTextSize(1);
  tft.setTextColor(isReady ? 0x07E0 : 0xF800);
  const char* badge = isReady ? "[ READY ]" : "[ COMING SOON ]";
  int16_t bx1, by1; uint16_t bw, bh;
  tft.getTextBounds(badge, 0, 0, &bx1, &by1, &bw, &bh);
  tft.setCursor((320 - bw) / 2, 208);
  tft.print(badge);
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
  tft.fillScreen(0x5E36);
  tft.fillRect(0, 0, 320, 30, 0x11E9);
  
  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(0xFD84);
  tft.setCursor(80, 22);
  tft.print("BMO GAMEBOY");

  if (selectedCover) {
    tft.drawRGBBitmap(110, 50, selectedCover, 100, 100);
    tft.drawRect(108, 48, 104, 104, 0x11E9);
    tft.drawRect(109, 49, 102, 102, 0x11E9);
  }

  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(0xFD84);
  tft.setCursor(50, 105);
  tft.print("<");
  tft.setCursor(250, 105);
  tft.print(">");

  tft.setFont(&FreeSans9pt7b);
  tft.setTextSize(1);
  tft.setTextColor(0x11E9);
  tft.setCursor(centeredX(titles[selectedIndex], 320), 185);
  tft.print(titles[selectedIndex]);

  tft.setFont();
  tft.setTextSize(1);
  tft.setTextColor(0x11E9);
  
  char progress[16];
  sprintf(progress, "Game %d of %d", selectedIndex + 1, count);
  tft.setCursor(10, 225);
  tft.print(progress);

  tft.setTextColor(useColorEmulator ? 0xFD84 : 0x11E9);
  tft.setCursor(240, 225);
  tft.print(useColorEmulator ? "Walnut-CGB" : "Peanut-GB");
}
