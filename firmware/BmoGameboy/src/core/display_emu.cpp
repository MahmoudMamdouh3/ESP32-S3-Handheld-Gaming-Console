#pragma GCC optimize ("O3")
#include "display_emu.h"
#include "config.h"
#include "battery.h"
#include <SPI.h>
#include <Adafruit_ST7789.h>
#include <cstring>
#include <cstdio>
#include <new>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>

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
    // Menu text lives on menuCanvas, whose active font can differ from tft.
    menuCanvas->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
    int cx = (displayWidth - (int)w) / 2 - x1;
    return (cx < 0) ? 2 : cx;
  }

  // The display is intentionally in BGR mode for the emulators.  UI pixels
  // are stored in a little-endian GFXcanvas and sent with writeBytes(), so
  // they need BOTH conversions here: RGB -> BGR and host -> wire byte order.
  constexpr uint16_t swapBytes(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
  }

  constexpr uint16_t uiColor(uint8_t r, uint8_t g, uint8_t b) {
    const uint16_t bgr565 = ((uint16_t)(b & 0xF8) << 8)
                          | ((uint16_t)(g & 0xFC) << 3)
                          | ((uint16_t)r >> 3);
    return swapBytes(bgr565);
  }

  constexpr uint16_t panelColor(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(b & 0xF8) << 8)
         | ((uint16_t)(g & 0xFC) << 3)
         | ((uint16_t)r >> 3);
  }

  constexpr uint16_t UI_TEAL = uiColor(83, 198, 181);
  constexpr uint16_t UI_DEEP_TEAL = uiColor(22, 72, 72);
  constexpr uint16_t UI_YELLOW = uiColor(255, 210, 66);
  constexpr uint16_t UI_WHITE = uiColor(245, 248, 245);
  constexpr uint16_t UI_MUTED = uiColor(163, 202, 195);
  constexpr uint16_t UI_BLACK = uiColor(5, 12, 14);

  const char* consoleName(RomType type) {
    switch (type) {
      case ROM_GB:      return "GAME BOY";
      case ROM_GBC:     return "GAME BOY COLOR";
      case ROM_NES:     return "NES";
      case ROM_WAD:     return "DOOM";
      case ROM_SMS:     return "SEGA MASTER SYSTEM";
      case ROM_GG:      return "GAME GEAR";
      case ROM_PCE:     return "PC ENGINE";
      case ROM_ATARI:   return "ATARI 2600";
      case ROM_PICO8:   return "PICO-8";
      case ROM_GENESIS: return "SEGA GENESIS";
      case ROM_SNES:    return "SUPER NINTENDO";
      case ROM_WSWAN:   return "WONDERSWAN";
      case ROM_NGP:     return "NEO GEO POCKET";
      case ROM_LYNX:    return "ATARI LYNX";
      case ROM_COLEM:   return "COLECOVISION";
      default:          return "GAMES";
    }
  }

  const char* consoleYear(RomType type) {
    switch (type) {
      case ROM_GB:      return "1989";
      case ROM_GBC:     return "1998";
      case ROM_NES:     return "1983";
      case ROM_WAD:     return "1993";
      case ROM_SMS:     return "1986";
      case ROM_GG:      return "1990";
      case ROM_PCE:     return "1987";
      case ROM_ATARI:   return "1977";
      case ROM_PICO8:   return "2015";
      case ROM_GENESIS: return "1988";
      case ROM_SNES:    return "1990";
      case ROM_WSWAN:   return "1999";
      case ROM_NGP:     return "1998";
      case ROM_LYNX:    return "1989";
      case ROM_COLEM:   return "1982";
      default:          return "";
    }
  }

  const char* consoleBadge(RomType type) {
    switch (type) {
      case ROM_GB:      return "GB";
      case ROM_GBC:     return "GBC";
      case ROM_NES:     return "NES";
      case ROM_WAD:     return "DOOM";
      case ROM_SMS:     return "SMS";
      case ROM_GG:      return "GG";
      case ROM_PCE:     return "PCE";
      case ROM_ATARI:   return "A26";
      case ROM_PICO8:   return "P8";
      case ROM_GENESIS: return "MD";
      case ROM_SNES:    return "SNES";
      case ROM_WSWAN:   return "WS";
      case ROM_NGP:     return "NGP";
      case ROM_LYNX:    return "LNX";
      case ROM_COLEM:   return "COL";
      default:          return "GAME";
    }
  }

  void drawCentered(const char* text, int y, uint16_t color) {
    menuCanvas->setCursor(centeredX(text, 320), y);
    menuCanvas->setTextColor(color);
    menuCanvas->print(text);
  }

  void drawFittedCentered(const char* source, int y, int maxWidth, uint16_t color) {
    char fitted[64];
    snprintf(fitted, sizeof(fitted), "%s", source ? source : "Unknown game");
    int16_t x1, y1;
    uint16_t width, height;
    menuCanvas->getTextBounds(fitted, 0, 0, &x1, &y1, &width, &height);
    while (width > maxWidth && strlen(fitted) > 4) {
      size_t length = strlen(fitted);
      fitted[length - 4] = '.';
      fitted[length - 3] = '.';
      fitted[length - 2] = '.';
      fitted[length - 1] = '\0';
      menuCanvas->getTextBounds(fitted, 0, 0, &x1, &y1, &width, &height);
    }
    drawCentered(fitted, y, color);
  }

  void gameTitle(const char* filename, char* output, size_t outputSize) {
    snprintf(output, outputSize, "%s", filename ? filename : "Unknown game");
    char* extension = strrchr(output, '.');
    if (extension) *extension = '\0';
    const char* baked = strstr(output, " (Baked)");
    if (baked) output[baked - output] = '\0';
  }

  void drawFooter(const char* text) {
    menuCanvas->fillRect(0, 212, 320, 28, UI_BLACK);
    menuCanvas->setFont();
    drawFittedCentered(text, 230, 300, UI_MUTED);
  }

  void writeMenuCanvas() {
    tft.startWrite();
    tft.setAddrWindow(0, 0, 320, 240);
    SPI.writeBytes((const uint8_t*)menuCanvas->getBuffer(), 320 * 240 * 2);
    tft.endWrite();
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

  // FIX: Some ST7789 panels are configured for BGR instead of RGB by default,
  // causing colors to look swapped (e.g., Mario looks blue, sky looks orange).
  // We manually flip the RGB/BGR bit (0x08) in the MADCTL register (0x36)
  // to correct the color order across the entire system.
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TFT_DC, LOW);
  digitalWrite(TFT_CS, LOW);
  SPI.transfer(0x36); // MADCTL command
  digitalWrite(TFT_DC, HIGH);
  SPI.transfer(0xA0 | 0x08); // Rotation 3 (0xA0) | BGR bit (0x08) = 0xA8
  digitalWrite(TFT_CS, HIGH);
  SPI.endTransaction();

  tft.fillScreen(ST77XX_BLACK);
  initMenuUI();
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
// NES Rendering (PERF-07: Static aligned buffer + 32-bit store coalescing)
// ---------------------------------------------------------------------------
static uint16_t __attribute__((aligned(4))) s_nesRowBuf[256];

void DisplayEmu::streamNESFrame(const uint8_t* nes_framebuffer) {
  tft.startWrite();
  tft.setAddrWindow(32, 0, 256, 240); // Center 256x240 on 320x240 display
  
  uint32_t* out32 = (uint32_t*)s_nesRowBuf;
  for (int y = 0; y < 240; y++) {
    const uint8_t* inRow = &nes_framebuffer[y * 256];
    for (int x = 0; x < 256; x += 4) {
      uint16_t p0 = NES_PALETTE[inRow[x]     & 0x3F];
      uint16_t p1 = NES_PALETTE[inRow[x + 1] & 0x3F];
      uint16_t p2 = NES_PALETTE[inRow[x + 2] & 0x3F];
      uint16_t p3 = NES_PALETTE[inRow[x + 3] & 0x3F];
      out32[(x >> 1)]     = (uint32_t)p0 | ((uint32_t)p1 << 16);
      out32[(x >> 1) + 1] = (uint32_t)p2 | ((uint32_t)p3 << 16);
    }
    SPI.writeBytes((const uint8_t*)s_nesRowBuf, 256 * 2);
  }
  
  tft.endWrite();
}

// ---------------------------------------------------------------------------
// DOOM Rendering (PERF-07/14: Static aligned buffer + 32-bit coalescing)
// ---------------------------------------------------------------------------
struct color {
    uint32_t b:8;
    uint32_t g:8;
    uint32_t r:8;
    uint32_t a:8;
};
extern "C" struct color colors[256];

static uint16_t __attribute__((aligned(4))) s_doomLineBuf[320];

void DisplayEmu::streamDoomFrame(const uint8_t* cmap) {
  tft.startWrite();
  tft.setAddrWindow(0, 20, 320, 200);

  static uint16_t doomPalette[256];
  static struct color lastColors[256];
  if (memcmp(lastColors, colors, sizeof(lastColors)) != 0) {
    memcpy(lastColors, colors, sizeof(lastColors));
    for (int i = 0; i < 256; ++i) {
      const struct color c = colors[i];
      const uint16_t p = ((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3);
      doomPalette[i] = (p >> 8) | (p << 8);
    }
  }

  uint32_t* out32 = (uint32_t*)s_doomLineBuf;
  for (int y = 0; y < 200; y++) {
    const uint8_t* inRow = &cmap[y * 320];
    for (int x = 0; x < 320; x += 4) {
      uint16_t p0 = doomPalette[inRow[x]];
      uint16_t p1 = doomPalette[inRow[x + 1]];
      uint16_t p2 = doomPalette[inRow[x + 2]];
      uint16_t p3 = doomPalette[inRow[x + 3]];
      out32[(x >> 1)]     = (uint32_t)p0 | ((uint32_t)p1 << 16);
      out32[(x >> 1) + 1] = (uint32_t)p2 | ((uint32_t)p3 << 16);
    }
    SPI.writeBytes((const uint8_t*)s_doomLineBuf, 320 * 2);
  }
  
  tft.endWrite();
}

void DisplayEmu::streamSMSFrame(const uint16_t* sms_framebuffer, bool isGameGear) {
  tft.startWrite();
  if (isGameGear) {
    tft.setAddrWindow(80, 48, 160, 144);
    SPI.writeBytes((const uint8_t*)sms_framebuffer, 160 * 144 * 2);
  } else {
    tft.setAddrWindow(32, 24, 256, 192);
    SPI.writeBytes((const uint8_t*)sms_framebuffer, 256 * 192 * 2);
  }
  tft.endWrite();
}

void DisplayEmu::streamPCEFrame(const uint16_t* pce_framebuffer) {
  tft.startWrite();
  tft.setAddrWindow(32, 0, 256, 240);
  SPI.writeBytes((const uint8_t*)pce_framebuffer, 256 * 240 * 2);
  tft.endWrite();
}

void DisplayEmu::streamAtariFrame(const uint16_t* atari_framebuffer) {
  tft.startWrite();
  tft.setAddrWindow(80, 24, 160, 192);
  SPI.writeBytes((const uint8_t*)atari_framebuffer, 160 * 192 * 2);
  tft.endWrite();
}

void DisplayEmu::streamPicoFrame(const uint16_t* pico_framebuffer) {
  tft.startWrite();
  tft.setAddrWindow(96, 56, 128, 128);
  SPI.writeBytes((const uint8_t*)pico_framebuffer, 128 * 128 * 2);
  tft.endWrite();
}

void DisplayEmu::streamGenesisFrame(const uint16_t* genesis_framebuffer, int width, int height) {
  tft.startWrite();
  // 320x224 centered vertically on 320x240 screen (yOffset = 8)
  tft.setAddrWindow(0, 8, 320, 224);
  SPI.writeBytes((const uint8_t*)genesis_framebuffer, 320 * 224 * 2);
  tft.endWrite();
}

void DisplayEmu::streamSNESFrame(const uint16_t* snes_framebuffer, int width, int height) {
  tft.startWrite();
  // 256x224 centered on 320x240 screen (xOffset = 32, yOffset = 8)
  tft.setAddrWindow(32, 8, 256, 224);
  SPI.writeBytes((const uint8_t*)snes_framebuffer, 256 * 224 * 2);
  tft.endWrite();
}

void DisplayEmu::streamWSwanFrame(const uint16_t* wswan_framebuffer, int width, int height) {
  tft.startWrite();
  // 224x144 centered on 320x240 screen (xOffset = 48, yOffset = 48)
  tft.setAddrWindow(48, 48, 224, 144);
  SPI.writeBytes((const uint8_t*)wswan_framebuffer, 224 * 144 * 2);
  tft.endWrite();
}

void DisplayEmu::streamNGPFrame(const uint16_t* ngp_framebuffer, int width, int height) {
  tft.startWrite();
  // 160x152 centered on 320x240 screen (xOffset = 80, yOffset = 44)
  tft.setAddrWindow(80, 44, 160, 152);
  SPI.writeBytes((const uint8_t*)ngp_framebuffer, 160 * 152 * 2);
  tft.endWrite();
}

void DisplayEmu::streamLynxFrame(const uint16_t* lynx_framebuffer, int width, int height) {
  tft.startWrite();
  // 160x102 centered on 320x240 screen (xOffset = 80, yOffset = 69)
  tft.setAddrWindow(80, 69, 160, 102);
  SPI.writeBytes((const uint8_t*)lynx_framebuffer, 160 * 102 * 2);
  tft.endWrite();
}

void DisplayEmu::streamColemFrame(const uint16_t* colem_framebuffer, int width, int height) {
  tft.startWrite();
  // 256x192 centered on 320x240 screen (xOffset = 32, yOffset = 24)
  tft.setAddrWindow(32, 24, 256, 192);
  SPI.writeBytes((const uint8_t*)colem_framebuffer, 256 * 192 * 2);
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

void DisplayEmu::pushPixelsFullScreen(const uint16_t* buffer) {
  tft.startWrite();
  tft.setAddrWindow(0, 0, 320, 240);
  SPI.writeBytes((const uint8_t*)buffer, 320 * 240 * 2);
  tft.endWrite();
}

// pushPixelsAt: unrestricted single-region blit at any (x, y) on the full
// 320×240 display.  Self-contained SPI transaction (startWrite/endWrite).
void DisplayEmu::pushPixelsAt(int x, int y, int w, int h, const uint16_t* buf) {
  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
  SPI.writeBytes((const uint8_t*)buf, (uint32_t)w * h * 2);
  tft.endWrite();
}

void DisplayEmu::startDirectWindow(int x, int y, int w, int h) {
  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
}

void DisplayEmu::writeWindowBytes(const uint8_t* data, size_t len) {
  SPI.writeBytes(data, len);
}

void DisplayEmu::endDirectWindow() {
  tft.endWrite();
}

void DisplayEmu::initMenuUI() {
  if (!menuCanvas) {
    PSRAMCanvas* canvas = new (std::nothrow) PSRAMCanvas(320, 240);
    // GFXcanvas16 does not throw on an allocation failure.  Keep the menu in
    // a safe no-op state instead of dereferencing a null framebuffer.
    if (!canvas || !canvas->getBuffer()) {
      delete canvas;
      return;
    }
    menuCanvas = canvas;
  }
}

void DisplayEmu::cleanupMenuUI() {
  // PERF-04: Preserve menuCanvas in PSRAM across game launches to prevent fragmentation
}

void DisplayEmu::drawConsoleSelectMenu(int selectedIndex, const int* gameCounts, int consoleCount, bool sdMounted) {
  if (!menuCanvas || consoleCount <= 0) return;
  const RomType consoles[15] = {
    ROM_GB, ROM_GBC, ROM_NES, ROM_WAD,
    ROM_SMS, ROM_GG, ROM_PCE, ROM_ATARI, ROM_PICO8,
    ROM_GENESIS, ROM_SNES, ROM_WSWAN, ROM_NGP, ROM_LYNX, ROM_COLEM
  };
  if (selectedIndex < 0) selectedIndex = 0;
  if (selectedIndex >= consoleCount) selectedIndex = consoleCount - 1;

  menuCanvas->fillScreen(UI_TEAL);
  menuCanvas->fillRect(0, 0, 320, 42, UI_BLACK);
  menuCanvas->setFont(&FreeSans12pt7b);
  drawCentered("BMO CONSOLE SELECT", 29, UI_YELLOW);
  menuCanvas->setFont(&FreeSans9pt7b);

  // Scroll window: 4 items visible at a time
  int topIndex = 0;
  if (selectedIndex >= 3) topIndex = selectedIndex - 2;
  if (topIndex + 4 > consoleCount) topIndex = consoleCount - 4;
  if (topIndex < 0) topIndex = 0;

  for (int row = 0; row < 4 && (topIndex + row) < consoleCount; ++row) {
    int i = topIndex + row;
    const int y = 53 + row * 39;
    const bool selected = (i == selectedIndex);
    const bool available = (gameCounts[i] > 0);
    if (selected) menuCanvas->fillRoundRect(14, y, 292, 33, 7, UI_DEEP_TEAL);
    menuCanvas->drawRoundRect(14, y, 292, 33, 7, selected ? UI_YELLOW : UI_MUTED);
    menuCanvas->setCursor(27, y + 22);
    menuCanvas->setTextColor(available ? (selected ? UI_YELLOW : UI_WHITE) : UI_MUTED);
    menuCanvas->print(consoleName(consoles[i]));
    menuCanvas->setFont();
    char detail[28];
    snprintf(detail, sizeof(detail), "%s  |  %d game%s", consoleYear(consoles[i]),
             gameCounts[i], gameCounts[i] == 1 ? "" : "s");
    menuCanvas->setCursor(160, y + 20);
    menuCanvas->print(detail);
    menuCanvas->setFont(&FreeSans9pt7b);
  }
    drawFooter(sdMounted ? "LEFT / RIGHT: CONSOLE    A: GAMES    SELECT: SPECS" :
                           "BUILT-IN GAMES ONLY - SD CARD NOT FOUND");
    writeMenuCanvas();
}

void DisplayEmu::drawConsoleMuseumModal(RomType console) {
  if (!menuCanvas) return;
  menuCanvas->fillScreen(UI_BLACK);
  menuCanvas->drawRoundRect(8, 8, 304, 224, 8, UI_YELLOW);
  menuCanvas->fillRect(0, 0, 320, 36, UI_DEEP_TEAL);
  
  menuCanvas->setFont(&FreeSans9pt7b);
  char title[48];
  snprintf(title, sizeof(title), "SPECS: %s", consoleName(console));
  drawCentered(title, 24, UI_YELLOW);
  
  menuCanvas->setFont();
  menuCanvas->setTextColor(UI_WHITE);
  
  menuCanvas->setCursor(18, 48);
  menuCanvas->printf("Released: %s  |  Badge: %s", consoleYear(console), consoleBadge(console));
  
  menuCanvas->setCursor(18, 66);
  switch (console) {
    case ROM_GB:
      menuCanvas->print("CPU: Sharp LR35902 @ 4.19MHz | 8KB RAM");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 160x144, 4-Shade Olive Green STN LCD");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Gunpei Yokoi's lateral thinking;");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("30h battery life, Pokemon & Tetris phenomenon.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Tetris, Pokemon R/B, Zelda Link's Awk");
      break;
    case ROM_GBC:
      menuCanvas->print("CPU: Sharp LR35902 Dual @ 8.39MHz | 32KB RAM");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 160x144, 32,768 Color TFT LCD");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Full color portable revolution; double");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("CPU speed, backwards compatible palette engine.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Zelda Oracle Ages, Pokemon Crystal");
      break;
    case ROM_NES:
      menuCanvas->print("CPU: Ricoh 2A03 @ 1.79MHz | 2KB RAM + Mappers");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 256x240, 25 Simultaneous Colors");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Revived gaming after 1983 crash;");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("Miyamoto created modern game design grammars.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Super Mario, Zelda, Metroid, Mega Man");
      break;
    case ROM_SNES:
      menuCanvas->print("CPU: 65C816 @ 3.58MHz | 128KB RAM + SPC700 DSP");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 256x224, Mode 7 Scaling, 32,768 Colors");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Golden era of 2D pixel art & sampled");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("audio; multi-jointed sprites, 16-bit mastery.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Chrono Trigger, Super Metroid, Mario");
      break;
    case ROM_GENESIS:
      menuCanvas->print("CPU: Motorola 68000 @ 7.67MHz + Z80 Sound Co-CPU");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 320x224, 64 Colors, YM2612 6-Ch FM Audio");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Blast processing speed & gritty FM synth;");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("Sonic momentum physics, electronic club tracks.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Sonic 2, Streets of Rage 2, Gunstar");
      break;
    case ROM_SMS:
      menuCanvas->print("CPU: Zilog Z80 @ 3.58MHz | 8KB RAM + 16KB VRAM");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 256x192, 32 Simultaneous Colors");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Superior 8-bit color depth and memory;");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("pioneered 3D dungeon rendering in Phantasy Star.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Phantasy Star, Alex Kidd, Wonder Boy");
      break;
    case ROM_GG:
      menuCanvas->print("CPU: Zilog Z80 @ 3.58MHz | 8KB RAM + 16KB VRAM");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 160x144, 4096-Color Master Palette");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Full-color landscape handheld with");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("backlit screen and arcade porting excellence.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Sonic Triple Trouble, Shinobi II");
      break;
    case ROM_PCE:
      menuCanvas->print("CPU: HuC6280 @ 7.16MHz | 16-bit HuC6270 GPU");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 256x240, 482 Simultaneous Colors");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Tiny credit-card HuCards with giant");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("color palettes; the ultimate home arcade shmup.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Castlevania Rondo, Soldier Blade");
      break;
    case ROM_ATARI:
      menuCanvas->print("CPU: MOS 6507 @ 1.19MHz | 128 BYTES RAM");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 160x192, Racing the beam with 0 VRAM");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Invented home cartridge gaming; programmers");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("calculated scanlines cycle-by-cycle in real time.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Pitfall!, River Raid, Space Invaders");
      break;
    case ROM_COLEM:
      menuCanvas->print("CPU: Z80A @ 3.58MHz | 1KB RAM + 16KB TMS9918A VDP");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 256x192, 16 Colors, 32 Hardware Sprites");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: First home console with true arcade ports");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("and dedicated hardware video memory.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Donkey Kong, Zaxxon, Venture");
      break;
    case ROM_NGP:
      menuCanvas->print("CPU: Toshiba TLCS-900H 16/32-bit RISC @ 6.14MHz");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 160x152, 4096 Colors, Microswitch Stick");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Legendary arcade fighting precision and");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("ultra-fluid chibi animations on a clicky stick.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Card Fighters Clash, Match Millennium");
      break;
    case ROM_LYNX:
      menuCanvas->print("CPU: MOS 65SC02 @ 4MHz + Suzy 16MHz Co-Processor");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 160x102, 4096 Colors, Hardware 3D Scaler");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: World's first color handheld with real-time");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("sprite zooming, scaling, and 3D pseudo-space.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Blue Lightning, California Games");
      break;
    case ROM_WSWAN:
      menuCanvas->print("CPU: 16-bit NEC V30 MZ @ 3.07MHz | 64KB RAM");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 224x144 Widescreen, Dual D-Pad TATE Mode");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Gunpei Yokoi's final design; 30+ hours on");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("ONE AA battery, playable vertical or horizontal.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Klonoa, Final Fantasy IV, Silversword");
      break;
    case ROM_PICO8:
      menuCanvas->print("Engine: Lua Virtual Computer @ 8MHz virtual clock");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 128x128, 16 Curated Colors, 32KB Carts");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Ignited the modern fantasy console movement;");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("deliberate constraints spark unbounded creativity.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: Celeste Classic, Slipways, High Stakes");
      break;
    case ROM_WAD:
      menuCanvas->print("Engine: id Tech 1 / DOOM Engine (BSP Raycasting)");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Display: 320x200 @ 35 FPS, 256 VGA Indexed Colors");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("Legacy: Carmack & Romero transformed gaming;");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("bioneered 3D FPS, multiplayer deathmatch, mods.");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Landmarks: DOOM, DOOM II, Freedoom, FreeDM");
      break;
    default:
      menuCanvas->print("Historic gaming hardware architecture.");
      break;
  }
  
  drawFooter("PRESS B OR SELECT TO RETURN");
  writeMenuCanvas();
}

void DisplayEmu::drawGameSelectMenu(const RomFile* const* games, int count, int selectedIndex, RomType console, bool sdMounted) {
  if (!menuCanvas) return;
  if (count < 0) count = 0;
  if (count > 0 && (selectedIndex < 0 || selectedIndex >= count)) selectedIndex = 0;

  menuCanvas->fillScreen(UI_TEAL);
  menuCanvas->fillRect(0, 0, 320, 42, UI_BLACK);
  
  // Battery status block (Top right header)
#if FEATURE_BATTERY_MONITOR
  menuCanvas->fillRect(275, 13, 36, 16, UI_BLACK);
  int pct = Battery::getPercentage();
  menuCanvas->drawRect(275, 16, 30, 10, UI_WHITE);
  menuCanvas->fillRect(305, 19, 2, 4, UI_WHITE);
  if (pct > 0) {
    uint16_t color = (pct > 20) ? uiColor(0, 255, 0) : uiColor(255, 0, 0);
    int fillW = (int)((pct / 100.0f) * 26);
    menuCanvas->fillRect(277, 18, fillW, 6, color);
  }
#endif

  menuCanvas->setFont(&FreeSans9pt7b);
  char heading[40];
  snprintf(heading, sizeof(heading), "%s LIBRARY", consoleName(console));
  drawCentered(heading, 28, UI_YELLOW);

  if (count == 0) {
    menuCanvas->setFont(&FreeSans12pt7b);
    drawCentered("NO GAMES", 112, UI_WHITE);
    menuCanvas->setFont(&FreeSans9pt7b);
    drawCentered(sdMounted ? "Add compatible ROMs to the SD card." :
                            "Insert an SD card to add more games.", 145, UI_DEEP_TEAL);
    drawFooter("B: BACK");
    writeMenuCanvas();
    return;
  }

  const RomFile* game = games[selectedIndex];
  char title[64];
  gameTitle(game ? game->filename : nullptr, title, sizeof(title));

  // A deliberately generic cover card: it gives every ROM a polished, safe
  // presentation without pretending that arbitrary files have cover art.
  menuCanvas->fillRoundRect(81, 54, 158, 112, 12, UI_DEEP_TEAL);
  menuCanvas->drawRoundRect(81, 54, 158, 112, 12, UI_YELLOW);
  menuCanvas->fillRect(102, 73, 116, 51, UI_BLACK);
  menuCanvas->drawRoundRect(112, 137, 96, 17, 8, UI_BLACK);
  menuCanvas->setFont(&FreeSans12pt7b);
  drawCentered(consoleBadge(console), 106, UI_YELLOW);
  menuCanvas->setFont(&FreeSans9pt7b);
  drawFittedCentered(title, 185, 282, UI_WHITE);
  menuCanvas->setFont();
  char position[28];
  snprintf(position, sizeof(position), "%d / %d", selectedIndex + 1, count);
  drawCentered(position, 204, UI_DEEP_TEAL);
  drawFooter("LEFT / RIGHT: BROWSE     A: PLAY     B: BACK");
  writeMenuCanvas();
}

void DisplayEmu::showSDCardWarning() {
  tft.fillRect(40, 80, 240, 80, panelColor(22, 72, 72));
  tft.drawRect(42, 82, 236, 76, panelColor(255, 210, 66));
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(panelColor(255, 210, 66));
  tft.setCursor(55, 115);
  tft.print("SD CARD REQUIRED");
  tft.setCursor(55, 135);
    tft.print("FOR THIS CONSOLE");
}


