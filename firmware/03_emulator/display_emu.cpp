#pragma GCC optimize ("O3")
#include "display_emu.h"
#include "config.h"
#include <SPI.h>
#include <Adafruit_ST7789.h>
#include <cstring>
#include <cstdio>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <math.h>

// No emulator headers should be included here to prevent ODR violations
#include <esp_heap_caps.h>

class PSRAMCanvas : public GFXcanvas16 {
public:
  PSRAMCanvas(uint16_t w, uint16_t h) : GFXcanvas16(w, h, false) {
    buffer = (uint16_t*)heap_caps_malloc(w * h * 2, MALLOC_CAP_SPIRAM);
  }
  ~PSRAMCanvas() {
    if (buffer) {
      heap_caps_free(buffer);
      buffer = nullptr;
    }
  }
};

static PSRAMCanvas* menuCanvas = nullptr;
static float currentScrollPos = 0.0f;
static uint32_t lastFrameTime = 0;


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
// Pre-swapping means we can use SPI.writeBytes() instead of slower SPI.write16().
const uint16_t DisplayEmu::CLASSIC_PALETTE[4] = {
  0xF30D, 
  0x710D, 
  0x0633, 
  0xC109  
};

const uint16_t DisplayEmu::NES_PALETTE[64] = {
  0x2C63, 0x5101, 0x9410, 0x1438, 0x0F58, 0x0868, 0x2068, 0xE050,
  0xA031, 0x400A, 0x8002, 0x6102, 0x0902, 0x0000, 0x0000, 0x0000,
  0x75AD, 0xFB12, 0x1F42, 0x3F71, 0xD9A0, 0xEFB0, 0x84B1, 0x609A,
  0x606B, 0x203C, 0x800C, 0x6604, 0xF103, 0x0000, 0x0000, 0x0000,
  0xFFFF, 0x9F65, 0x9F94, 0xBFC3, 0x5FF3, 0x79FB, 0x0EFC, 0xE4EC,
  0xE0BD, 0xC08E, 0x265F, 0x1047, 0x7B4E, 0x694A, 0x0000, 0x0000,
  0xFFFF, 0x3FA7, 0xDFBD, 0xDFDD, 0xDFFD, 0x38FD, 0x96F6, 0x15FF,
  0xCFFE, 0xCFDF, 0xD7BF, 0xDBBF, 0xFF07, 0xDFFE, 0x0000, 0x0000,
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
// NES Rendering
// ---------------------------------------------------------------------------
void DisplayEmu::streamNESFrame(const uint8_t* nes_framebuffer) {
  tft.startWrite();
  tft.setAddrWindow(32, 0, 256, 240); // Center 256x240 on 320x240 display
  
  uint16_t row_buf[256];
  for (int y = 0; y < 240; y++) {
    for (int x = 0; x < 256; x++) {
      uint8_t color_ix = nes_framebuffer[(y * 256) + x];
      row_buf[x] = NES_PALETTE[color_ix & 0x3f];
    }
    SPI.writeBytes((const uint8_t*)row_buf, 256 * 2);
  }
  
  tft.endWrite();
}

// ---------------------------------------------------------------------------
// DOOM Rendering
// ---------------------------------------------------------------------------
struct color {
    uint32_t b:8;
    uint32_t g:8;
    uint32_t r:8;
    uint32_t a:8;
};
extern "C" struct color colors[256];

void DisplayEmu::streamDoomFrame(const uint8_t* cmap) {
  // Screen is 320x240, DOOM is 320x200
  // So we center DOOM on Y (offset = 20)
  tft.startWrite();
  tft.setAddrWindow(0, 20, 320, 200);

  // We have enough RAM to do 1 line at a time
  uint16_t lineBuf[320];

  for (int y = 0; y < 200; y++) {
    for (int x = 0; x < 320; x++) {
      int idx = cmap[y * 320 + x];
      struct color c = colors[idx];
      uint16_t p = ((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3);
      // Byte swap for SPI DMA
      lineBuf[x] = (p >> 8) | (p << 8);
    }
    SPI.writeBytes((const uint8_t*)lineBuf, 320 * 2);
  }
  
  tft.endWrite();
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

void DisplayEmu::initMenuUI() {
  if (!menuCanvas) {
    menuCanvas = new PSRAMCanvas(320, 240);
  }
}

void DisplayEmu::cleanupMenuUI() {
  if (menuCanvas) {
    delete menuCanvas;
    menuCanvas = nullptr;
  }
}

void DisplayEmu::drawMenuFrame(const char** titles, int count, int selectedIndex, bool useColorEmulator) {
  if (!menuCanvas) return;
  
  uint32_t now = millis();
  float dt = (now - lastFrameTime) / 1000.0f;
  if (dt > 0.1f) dt = 0.1f;
  lastFrameTime = now;

  // Smooth lerp scrolling
  float targetScroll = (float)selectedIndex;
  currentScrollPos += (targetScroll - currentScrollPos) * 10.0f * dt;

  // Animated background (moving grid)
  menuCanvas->fillScreen(0x18C3); // Dark background
  int gridOffset = (now / 20) % 20;
  for (int x = gridOffset; x < 320; x += 20) {
    menuCanvas->drawFastVLine(x, 0, 240, 0x2104);
  }
  for (int y = gridOffset; y < 240; y += 20) {
    menuCanvas->drawFastHLine(0, y, 320, 0x2104);
  }

  // Draw header
  menuCanvas->fillRect(0, 0, 320, 35, 0x0000);
  menuCanvas->setFont(&FreeSans12pt7b);
  menuCanvas->setTextSize(1);
  menuCanvas->setTextColor(0xFD84); // Yellow text
  menuCanvas->setCursor(80, 25);
  menuCanvas->print("BMO GAMEBOY");

  // Pulsing selector
  int pulse = (sin(now / 150.0f) + 1.0f) * 15.0f;
  uint16_t pulseColor = tft.color565(100 + pulse*5, 200 + pulse, 100 + pulse*5);

  // Draw items
  menuCanvas->setFont(&FreeSans9pt7b);
  
  for (int i = 0; i < count; i++) {
    float yPos = 120 + (i - currentScrollPos) * 40;
    
    // Only draw if on screen
    if (yPos > -20 && yPos < 260) {
      bool isSelected = (i == selectedIndex);
      
      if (isSelected) {
        menuCanvas->fillRoundRect(20, yPos - 15, 280, 30, 8, pulseColor);
        menuCanvas->drawRoundRect(20, yPos - 15, 280, 30, 8, 0xFFFF);
        menuCanvas->setTextColor(0x0000);
      } else {
        menuCanvas->setTextColor(0xFFFF);
      }
      
      menuCanvas->setCursor(35, yPos + 6);
      menuCanvas->print(titles[i]);
    }
  }

  // Draw footer
  menuCanvas->fillRect(0, 210, 320, 30, 0x0000);
  menuCanvas->setTextColor(0xFFFF);
  menuCanvas->setFont();
  char progress[32];
  sprintf(progress, "Item %d / %d", selectedIndex + 1, count);
  menuCanvas->setCursor(10, 220);
  menuCanvas->print(progress);

  // Push to screen via SPI
  tft.startWrite();
  tft.setAddrWindow(0, 0, 320, 240);
  SPI.writeBytes((const uint8_t*)menuCanvas->getBuffer(), 320 * 240 * 2);
  tft.endWrite();
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
