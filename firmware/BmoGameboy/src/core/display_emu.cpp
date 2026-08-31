#pragma GCC optimize ("O3")
#include "display_emu.h"
#include "config.h"
#include "battery.h"
#include "theme.h"
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

  constexpr uint16_t UI_TEAL        = Theme::BMO_BODY_TEAL;
  constexpr uint16_t UI_MINT        = Theme::BMO_SCREEN_MINT;
  constexpr uint16_t UI_DEEP_TEAL   = Theme::BMO_DEEP_TEAL;
  constexpr uint16_t UI_YELLOW      = Theme::BMO_DPAD_YELLOW;
  constexpr uint16_t UI_CORAL       = Theme::BMO_CORAL_RED;
  constexpr uint16_t UI_BLUE        = Theme::BMO_BLUE_BUTTON;
  constexpr uint16_t UI_WHITE       = Theme::BMO_WHITE;
  constexpr uint16_t UI_MUTED       = Theme::COLOR_TEXT_MUTED;
  constexpr uint16_t UI_BLACK       = Theme::BMO_DARK_FOREST;

  const char* consoleName(RomType type) {
    switch (type) {
      case ROM_FAVORITES: return "FAVORITES";
      case ROM_GB:        return "GAME BOY";
      case ROM_GBC:       return "GAME BOY COLOR";
      case ROM_NES:       return "NES";
      case ROM_WAD:       return "DOOM";
      case ROM_SMS:       return "SEGA MASTER SYSTEM";
      case ROM_GG:        return "GAME GEAR";
      case ROM_PCE:       return "PC ENGINE";
      case ROM_ATARI:     return "ATARI 2600";
      case ROM_PICO8:     return "PICO-8";
      case ROM_GENESIS:   return "SEGA GENESIS";
      case ROM_SNES:      return "SUPER NINTENDO";
      case ROM_WSWAN:     return "WONDERSWAN";
      case ROM_NGP:       return "NEO GEO POCKET";
      case ROM_LYNX:      return "ATARI LYNX";
      case ROM_COLEM:     return "COLECOVISION";
      default:            return "GAMES";
    }
  }

  const char* consoleYear(RomType type) {
    switch (type) {
      case ROM_FAVORITES: return "STARRED";
      case ROM_GB:        return "1989";
      case ROM_GBC:       return "1998";
      case ROM_NES:       return "1983";
      case ROM_WAD:       return "1993";
      case ROM_SMS:       return "1986";
      case ROM_GG:        return "1990";
      case ROM_PCE:       return "1987";
      case ROM_ATARI:     return "1977";
      case ROM_PICO8:     return "2015";
      case ROM_GENESIS:   return "1988";
      case ROM_SNES:      return "1990";
      case ROM_WSWAN:     return "1999";
      case ROM_NGP:       return "1998";
      case ROM_LYNX:      return "1989";
      case ROM_COLEM:     return "1982";
      default:            return "";
    }
  }

  const char* consoleBadge(RomType type) {
    switch (type) {
      case ROM_FAVORITES: return "FAV";
      case ROM_GB:        return "GB";
      case ROM_GBC:       return "GBC";
      case ROM_NES:       return "NES";
      case ROM_WAD:       return "DOOM";
      case ROM_SMS:       return "SMS";
      case ROM_GG:        return "GG";
      case ROM_PCE:       return "PCE";
      case ROM_ATARI:     return "A26";
      case ROM_PICO8:     return "P8";
      case ROM_GENESIS:   return "MD";
      case ROM_SNES:      return "SNES";
      case ROM_WSWAN:     return "WS";
      case ROM_NGP:       return "NGP";
      case ROM_LYNX:      return "LNX";
      case ROM_COLEM:     return "COL";
      default:            return "GAME";
    }
  }

  const char* consoleExt(RomType type) {
    switch (type) {
      case ROM_FAVORITES: return "*";
      case ROM_GB:        return ".gb";
      case ROM_GBC:       return ".gbc";
      case ROM_NES:       return ".nes";
      case ROM_WAD:       return ".wad";
      case ROM_SMS:       return ".sms";
      case ROM_GG:        return ".gg";
      case ROM_PCE:       return ".pce";
      case ROM_ATARI:     return ".a26";
      case ROM_PICO8:     return ".p8";
      case ROM_GENESIS:   return ".md";
      case ROM_SNES:      return ".sfc";
      case ROM_WSWAN:     return ".ws";
      case ROM_NGP:       return ".ngc";
      case ROM_LYNX:      return ".lnx";
      case ROM_COLEM:     return ".col";
      default:            return ".rom";
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
    DisplayEmu::sanitizeRomTitle(filename, output, outputSize);
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

void DisplayEmu::drawBootSplash(bool pressAnyButtonBlink) {
  if (!menuCanvas) return;
  menuCanvas->fillScreen(UI_BLACK);

  // Large Centered BMO Mascot Face (w=120, h=80 centered on 320x240)
  const int cx = 160;
  const int cy = 76;
  
  // Body Card (BMO Teal #5FB49C)
  menuCanvas->fillRoundRect(cx - 60, cy - 40, 120, 80, 16, UI_TEAL);
  menuCanvas->drawRoundRect(cx - 60, cy - 40, 120, 80, 16, UI_DEEP_TEAL);

  // Black Dot Eyes
  menuCanvas->fillCircle(cx - 25, cy - 8, 7, UI_BLACK);
  menuCanvas->fillCircle(cx + 25, cy - 8, 7, UI_BLACK);

  // Cheerful Smile
  menuCanvas->drawCircle(cx, cy + 8, 10, UI_BLACK);
  menuCanvas->drawCircle(cx, cy + 9, 10, UI_BLACK);
  menuCanvas->fillRect(cx - 12, cy - 2, 24, 10, UI_TEAL);

  // Rosy Cheeks (#E8175D / #F48FB1)
  menuCanvas->fillCircle(cx - 40, cy + 2, 6, UI_CORAL);
  menuCanvas->fillCircle(cx + 40, cy + 2, 6, UI_CORAL);

  // Title: "BMO GAMEBOY"
  menuCanvas->setFont(&FreeSans12pt7b);
  drawCentered("BMO GAMEBOY", 152, UI_WHITE);

  // Subtitle: "15 Retro Consoles - Thousands of Adventures"
  menuCanvas->setFont();
  menuCanvas->setCursor(34, 178);
  menuCanvas->setTextColor(UI_MINT);
  menuCanvas->print("15 Retro Consoles - Thousands of Adventures");

  // Blinking Prompt: "PRESS ANY BUTTON"
  if (pressAnyButtonBlink) {
    menuCanvas->setFont(&FreeSans9pt7b);
    drawCentered("PRESS ANY BUTTON", 216, UI_YELLOW);
  }

  writeMenuCanvas();
}

void DisplayEmu::drawConsoleSelectMenu(int selectedIndex, const int* gameCounts, int consoleCount, bool sdMounted) {
  if (!menuCanvas || consoleCount <= 0) return;
  const RomType consoles[16] = {
    ROM_FAVORITES,
    ROM_GB, ROM_GBC, ROM_NES, ROM_WAD,
    ROM_SMS, ROM_GG, ROM_PCE, ROM_ATARI, ROM_PICO8,
    ROM_GENESIS, ROM_SNES, ROM_WSWAN, ROM_NGP, ROM_LYNX, ROM_COLEM
  };
  if (selectedIndex < 0) selectedIndex = 0;
  if (selectedIndex >= consoleCount) selectedIndex = consoleCount - 1;

  const RomType currentConsole = consoles[selectedIndex];
  const bool isFav = (currentConsole == ROM_FAVORITES);

  // Screen background: Dark Navy / Midnight Black (Exact 1:1 match to simulator!)
  menuCanvas->fillScreen(UI_BLACK);

  // Top Header Bar: Dark Forest Teal (0, 0, 320, 42)
  menuCanvas->fillRect(0, 0, 320, 42, UI_DEEP_TEAL);

  // Mini BMO Mascot Face in top-left (8, 5, 48, 30)
  menuCanvas->fillRoundRect(8, 5, 48, 30, 6, UI_TEAL);
  menuCanvas->fillCircle(20, 15, 2, UI_BLACK);
  menuCanvas->fillCircle(36, 15, 2, UI_BLACK);
  menuCanvas->drawCircle(28, 20, 4, UI_BLACK);
  menuCanvas->fillRect(24, 15, 8, 4, UI_TEAL);
  menuCanvas->fillCircle(14, 20, 2, UI_CORAL);
  menuCanvas->fillCircle(42, 20, 2, UI_CORAL);

  // Top Right System Counter ("SYSTEM 1/16")
  menuCanvas->setFont();
  char counterStr[32];
  snprintf(counterStr, sizeof(counterStr), "SYSTEM %d/%d", selectedIndex + 1, consoleCount);
  menuCanvas->setCursor(220, 20);
  menuCanvas->setTextColor(UI_MINT);
  menuCanvas->print(counterStr);

  // Center Carousel Card (35, 54, 250, 148) — Exact 1:1 match to simulator!
  const int cardX = 35;
  const int cardY = 54;
  const int cardW = 250;
  const int cardH = 148;

  menuCanvas->fillRoundRect(cardX, cardY, cardW, cardH, 12, UI_DEEP_TEAL);
  menuCanvas->drawRoundRect(cardX, cardY, cardW, cardH, 12, UI_YELLOW);
  menuCanvas->drawRoundRect(cardX + 1, cardY + 1, cardW - 2, cardH - 2, 11, UI_YELLOW);

  // Console Badge [ FAV ] / [ GB ] (cardX + 16, cardY + 16, 54, 24)
  menuCanvas->fillRoundRect(cardX + 16, cardY + 16, 54, 24, 6, UI_YELLOW);
  menuCanvas->setFont();
  menuCanvas->setCursor(cardX + 24, cardY + 24);
  menuCanvas->setTextColor(UI_BLACK);
  menuCanvas->print(consoleBadge(currentConsole));

  // Year / "Starred" in top-right of card
  menuCanvas->setCursor(cardX + cardW - 65, cardY + 24);
  menuCanvas->setTextColor(UI_YELLOW);
  if (isFav) {
    menuCanvas->print("Starred");
  } else {
    menuCanvas->print(consoleYear(currentConsole));
  }

  // Full Console Name
  menuCanvas->setFont(&FreeSans9pt7b);
  menuCanvas->setCursor(cardX + 16, cardY + 74);
  menuCanvas->setTextColor(UI_WHITE);
  if (isFav) {
    menuCanvas->print("* Favorites");
  } else {
    menuCanvas->print(consoleName(currentConsole));
  }

  // Format Tag
  menuCanvas->setFont();
  menuCanvas->setCursor(cardX + 16, cardY + 95);
  menuCanvas->setTextColor(UI_MINT);
  char fmtBuf[32];
  snprintf(fmtBuf, sizeof(fmtBuf), "Format: %s", isFav ? "*" : consoleExt(currentConsole));
  menuCanvas->print(fmtBuf);

  // Game Count Tag
  menuCanvas->setCursor(cardX + 16, cardY + 124);
  menuCanvas->setTextColor(UI_YELLOW);
  char countBuf[32];
  snprintf(countBuf, sizeof(countBuf), "* %d Games Ready", gameCounts[selectedIndex]);
  menuCanvas->print(countBuf);

  // Large Pixel-Art Silhouette Icon under the Date on Right of Card!
  drawConsoleIcon(currentConsole, cardX + 175, cardY + 50, UI_YELLOW);

  // Left & Right Carousel Arrows
  menuCanvas->setFont(&FreeSans12pt7b);
  menuCanvas->setCursor(14, 134);
  menuCanvas->setTextColor(UI_TEAL);
  menuCanvas->print("<");
  menuCanvas->setCursor(298, 134);
  menuCanvas->print(">");

  // Footer Instruction Bar (0, 214, 320, 26)
  menuCanvas->fillRect(0, 214, 320, 26, UI_DEEP_TEAL);
  menuCanvas->setFont();
  menuCanvas->setCursor(20, 224);
  menuCanvas->setTextColor(UI_MINT);
  if (!sdMounted) {
    menuCanvas->print("BUILT-IN GAMES ONLY - SD CARD NOT FOUND");
  } else {
    menuCanvas->print("A: Browse Games  |  SELECT: Specs  |  < / >: Console");
  }

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
    case ROM_FAVORITES:
      menuCanvas->print("Category: Universal Starred Games Collection");
      menuCanvas->setCursor(18, 80);
      menuCanvas->print("Consoles: Multi-System Auto-Dispatch Launcher");
      menuCanvas->setCursor(18, 100);
      menuCanvas->print("BMO Tip: Press SELECT on ANY game in ANY console");
      menuCanvas->setCursor(18, 114);
      menuCanvas->print("library to star it and launch directly from here!");
      menuCanvas->setCursor(18, 134);
      menuCanvas->print("Auto-saves favorites list to SD: /favorites.txt");
      break;
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

  menuCanvas->fillScreen(UI_BLACK);
  menuCanvas->fillRect(0, 0, 320, 38, UI_DEEP_TEAL);

  // Mini BMO Mascot Face in top-left (8, 4, 40, 28)
  menuCanvas->fillRoundRect(8, 4, 40, 28, 6, UI_TEAL);
  menuCanvas->fillCircle(18, 14, 2, UI_BLACK);
  menuCanvas->fillCircle(30, 14, 2, UI_BLACK);
  menuCanvas->drawCircle(24, 18, 4, UI_BLACK);
  menuCanvas->fillRect(20, 14, 8, 4, UI_TEAL);
  menuCanvas->fillCircle(12, 19, 2, UI_CORAL);
  menuCanvas->fillCircle(36, 19, 2, UI_CORAL);
  
  // Battery status block (Top right header)
#if FEATURE_BATTERY_MONITOR
  int pct = Battery::getPercentage();
  uint16_t battColor = (pct > 50) ? Theme::COLOR_SUCCESS : ((pct > 20) ? UI_YELLOW : UI_CORAL);
  menuCanvas->drawRoundRect(280, 13, 24, 12, 2, UI_WHITE);
  menuCanvas->fillRect(304, 16, 2, 6, UI_WHITE); // Battery positive cap
  int fillW = (int)((pct / 100.0f) * 20);
  if (fillW > 0) {
    menuCanvas->fillRect(282, 15, fillW, 8, battColor);
  }
#endif

  menuCanvas->setFont(&FreeSans9pt7b);
  char heading[40];
  if (console == ROM_FAVORITES) {
    snprintf(heading, sizeof(heading), "★ FAVORITE GAMES");
  } else {
    snprintf(heading, sizeof(heading), "%s LIBRARY", consoleName(console));
  }
  drawCentered(heading, 25, UI_YELLOW);

  if (count == 0) {
    menuCanvas->setFont(&FreeSans12pt7b);
    if (console == ROM_FAVORITES) {
      drawCentered("NO FAVORITES YET!", 100, UI_BLACK);
      menuCanvas->setFont(&FreeSans9pt7b);
      drawCentered("Press SELECT on any game in any", 128, UI_DEEP_TEAL);
      drawCentered("console library to star it ★", 148, UI_DEEP_TEAL);
    } else {
      drawCentered("NO GAMES", 100, UI_BLACK);
      menuCanvas->setFont(&FreeSans9pt7b);
      drawCentered(sdMounted ? "Add compatible ROMs to the SD card." :
                              "Insert an SD card to add more games.", 138, UI_DEEP_TEAL);
    }
    drawFooter("B: BACK");
    writeMenuCanvas();
    return;
  }

  const RomFile* game = games[selectedIndex];
  const bool isFav = game ? game->isFavorite : false;
  const RomType actualType = game ? game->type : console;

  char title[64];
  gameTitle(game ? game->filename : nullptr, title, sizeof(title));

  // Authentic BMO Mint & Teal Cover Card
  menuCanvas->fillRoundRect(81, 46, 158, 114, 12, UI_DEEP_TEAL);
  menuCanvas->drawRoundRect(81, 46, 158, 114, 12, isFav ? UI_YELLOW : UI_TEAL);
  menuCanvas->fillRect(102, 62, 116, 52, UI_BLACK);
  menuCanvas->drawRoundRect(112, 126, 96, 18, 8, isFav ? UI_YELLOW : UI_TEAL);
  
  menuCanvas->setFont(&FreeSans12pt7b);
  drawCentered(consoleBadge(actualType), 98, UI_YELLOW);
  
  menuCanvas->setFont();
  if (isFav) {
    menuCanvas->setTextColor(UI_YELLOW);
    menuCanvas->setCursor(120, 139);
    menuCanvas->print("★ FAVORITE");
  } else {
    menuCanvas->setTextColor(UI_WHITE);
    menuCanvas->setCursor(126, 139);
    menuCanvas->print(consoleBadge(actualType));
  }

  menuCanvas->setFont(&FreeSans9pt7b);
  drawFittedCentered(title, 180, 290, UI_BLACK);
  
  menuCanvas->setFont();
  char position[36];
  snprintf(position, sizeof(position), "%d / %d  [%s]", selectedIndex + 1, count, consoleBadge(actualType));
  drawCentered(position, 198, UI_DEEP_TEAL);
  
  drawFooter("◄/►: JUMP A-Z    A: PLAY    B: BACK    SELECT: ★ FAV");
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

void DisplayEmu::sanitizeRomTitle(const char* src, char* dst, size_t maxLen) {
  if (!src || !dst || maxLen == 0) return;
  size_t si = 0, di = 0;
  bool inParen = false, inBracket = false;
  
  while (src[si] != '\0' && di + 1 < maxLen) {
    char c = src[si++];
    if (c == '(') { inParen = true; continue; }
    if (c == ')') { inParen = false; continue; }
    if (c == '[') { inBracket = true; continue; }
    if (c == ']') { inBracket = false; continue; }
    if (inParen || inBracket) continue;
    
    if (c == '_') c = ' ';
    if (c == ' ' && di > 0 && dst[di - 1] == ' ') continue;
    
    dst[di++] = c;
  }
  dst[di] = '\0';
  
  char* ext = strrchr(dst, '.');
  if (ext) *ext = '\0';
  
  while (di > 0 && (dst[di - 1] == ' ' || dst[di - 1] == '\t')) {
    dst[--di] = '\0';
  }
  
  if (di == 0) {
    snprintf(dst, maxLen, "%s", src);
  }
}

static int activeDmgPaletteIdx = 1; // Default to authentic BMO Teal (Theme::PALETTE_BMO)
static const uint16_t* const DMG_PALETTES[Theme::PALETTE_COUNT] = {
  DisplayEmu::CLASSIC_PALETTE,
  Theme::PALETTE_BMO,
  Theme::PALETTE_POCKET,
  Theme::PALETTE_LIGHT,
  Theme::PALETTE_AMBER
};

void DisplayEmu::setDmgPalette(int paletteIndex) {
  if (paletteIndex >= 0 && paletteIndex < Theme::PALETTE_COUNT) {
    activeDmgPaletteIdx = paletteIndex;
  }
}

int DisplayEmu::getDmgPaletteIndex() {
  return activeDmgPaletteIdx;
}

void DisplayEmu::cycleDmgPalette() {
  activeDmgPaletteIdx = (activeDmgPaletteIdx + 1) % Theme::PALETTE_COUNT;
}

const uint16_t* DisplayEmu::getActiveDmgPalette() {
  return DMG_PALETTES[activeDmgPaletteIdx];
}

void DisplayEmu::drawDiagnosticsDashboard(unsigned long uptimeMs, uint32_t freeDram,
                                         uint32_t freePsram, uint32_t freeIram,
                                         uint8_t buttonMask) {
  if (!menuCanvas) return;
  menuCanvas->fillScreen(UI_BLACK);
  menuCanvas->drawRoundRect(8, 8, 304, 224, 8, UI_TEAL);
  menuCanvas->fillRect(0, 0, 320, 36, UI_DEEP_TEAL);
  
  menuCanvas->setFont(&FreeSans9pt7b);
  drawCentered("HARDWARE DIAGNOSTICS", 24, UI_YELLOW);
  
  menuCanvas->setFont();
  menuCanvas->setTextColor(UI_WHITE);
  
  // System Telemetry
  menuCanvas->setCursor(18, 46);
  menuCanvas->printf("MCU: ESP32-S3 Dual LX7 @ 240MHz");
  menuCanvas->setCursor(18, 58);
  unsigned long sec = uptimeMs / 1000;
  menuCanvas->printf("Uptime: %lum %lus  |  Flash: 16MB OPI 80MHz", sec / 60, sec % 60);

  // Memory Telemetry
  menuCanvas->setCursor(18, 74);
  menuCanvas->setTextColor(UI_YELLOW);
  menuCanvas->print("--- MEMORY TELEMETRY ---");
  menuCanvas->setTextColor(UI_WHITE);
  
  menuCanvas->setCursor(18, 88);
  menuCanvas->printf("Internal DRAM Free : %6u / 327,680 B (%u%%)", 
                     (unsigned int)freeDram, (unsigned int)(freeDram * 100 / 327680));
  menuCanvas->setCursor(18, 100);
  menuCanvas->printf("Octal PSRAM Free   : %6u / 8,388,608 B", (unsigned int)freePsram);
  menuCanvas->setCursor(18, 112);
  menuCanvas->printf("IRAM Execution Free: %6u B", (unsigned int)freeIram);

  // Live Button Tester
  menuCanvas->setCursor(18, 130);
  menuCanvas->setTextColor(UI_YELLOW);
  menuCanvas->print("--- LIVE BUTTON MATRIX TESTER ---");
  menuCanvas->setTextColor(UI_WHITE);

  const bool upPressed    = (buttonMask & (1 << 0));
  const bool downPressed  = (buttonMask & (1 << 1));
  const bool leftPressed  = (buttonMask & (1 << 2));
  const bool rightPressed = (buttonMask & (1 << 3));
  const bool aPressed     = (buttonMask & (1 << 4));
  const bool bPressed     = (buttonMask & (1 << 5));
  const bool selPressed   = (buttonMask & (1 << 6));
  const bool startPressed = (buttonMask & (1 << 7));

  // D-Pad
  menuCanvas->fillRect(42, 146, 16, 14, upPressed    ? UI_YELLOW : UI_DEEP_TEAL);
  menuCanvas->drawRect(42, 146, 16, 14, UI_WHITE);
  menuCanvas->setCursor(47, 157);
  menuCanvas->setTextColor(upPressed ? UI_BLACK : UI_WHITE);
  menuCanvas->print("U");

  menuCanvas->fillRect(24, 162, 16, 14, leftPressed  ? UI_YELLOW : UI_DEEP_TEAL);
  menuCanvas->drawRect(24, 162, 16, 14, UI_WHITE);
  menuCanvas->setCursor(29, 173);
  menuCanvas->setTextColor(leftPressed ? UI_BLACK : UI_WHITE);
  menuCanvas->print("L");

  menuCanvas->fillRect(60, 162, 16, 14, rightPressed ? UI_YELLOW : UI_DEEP_TEAL);
  menuCanvas->drawRect(60, 162, 16, 14, UI_WHITE);
  menuCanvas->setCursor(65, 173);
  menuCanvas->setTextColor(rightPressed ? UI_BLACK : UI_WHITE);
  menuCanvas->print("R");

  menuCanvas->fillRect(42, 178, 16, 14, downPressed  ? UI_YELLOW : UI_DEEP_TEAL);
  menuCanvas->drawRect(42, 178, 16, 14, UI_WHITE);
  menuCanvas->setCursor(47, 189);
  menuCanvas->setTextColor(downPressed ? UI_BLACK : UI_WHITE);
  menuCanvas->print("D");

  // Select / Start
  menuCanvas->fillRect(104, 168, 36, 16, selPressed   ? UI_YELLOW : UI_DEEP_TEAL);
  menuCanvas->drawRect(104, 168, 36, 16, UI_WHITE);
  menuCanvas->setCursor(110, 180);
  menuCanvas->setTextColor(selPressed ? UI_BLACK : UI_WHITE);
  menuCanvas->print("SEL");

  menuCanvas->fillRect(148, 168, 36, 16, startPressed ? UI_YELLOW : UI_DEEP_TEAL);
  menuCanvas->drawRect(148, 168, 36, 16, UI_WHITE);
  menuCanvas->setCursor(154, 180);
  menuCanvas->setTextColor(startPressed ? UI_BLACK : UI_WHITE);
  menuCanvas->print("STA");

  // Action Buttons B & A
  menuCanvas->fillRect(212, 166, 22, 20, bPressed     ? UI_YELLOW : UI_DEEP_TEAL);
  menuCanvas->drawRect(212, 166, 22, 20, UI_WHITE);
  menuCanvas->setCursor(219, 180);
  menuCanvas->setTextColor(bPressed ? UI_BLACK : UI_WHITE);
  menuCanvas->print("B");

  menuCanvas->fillRect(244, 154, 22, 20, aPressed     ? UI_YELLOW : UI_DEEP_TEAL);
  menuCanvas->drawRect(244, 154, 22, 20, UI_WHITE);
  menuCanvas->setCursor(251, 168);
  menuCanvas->setTextColor(aPressed ? UI_BLACK : UI_WHITE);
  menuCanvas->print("A");

  drawFooter("PRESS B TO EXIT DIAGNOSTICS");
  writeMenuCanvas();
}

void DisplayEmu::drawConsoleIcon(RomType type, int x, int y, uint16_t primaryColor) {
  if (!menuCanvas) return;
  if (primaryColor == 0) primaryColor = UI_YELLOW;

  // Dark backdrop pill (w=58, h=74)
  menuCanvas->fillRoundRect(x, y, 58, 74, 8, UI_BLACK);
  menuCanvas->drawRoundRect(x, y, 58, 74, 8, UI_TEAL);

  const int cx = x + 29;
  const int cy = y + 37;

  switch (type) {
    case ROM_FAVORITES:
      menuCanvas->fillTriangle(cx, cy - 22, cx - 18, cy + 18, cx + 18, cy - 6, UI_YELLOW);
      menuCanvas->fillTriangle(cx, cy - 22, cx + 18, cy + 18, cx - 18, cy - 6, UI_YELLOW);
      menuCanvas->fillTriangle(cx - 22, cy - 6, cx + 22, cy - 6, cx, cy + 22, UI_YELLOW);
      menuCanvas->fillCircle(cx - 7, cy - 2, 2, UI_BLACK);
      menuCanvas->fillCircle(cx + 7, cy - 2, 2, UI_BLACK);
      menuCanvas->drawCircle(cx, cy + 2, 4, UI_BLACK);
      menuCanvas->fillRect(cx - 5, cy - 2, 10, 4, UI_YELLOW);
      menuCanvas->fillCircle(cx - 11, cy + 3, 2, UI_CORAL);
      menuCanvas->fillCircle(cx + 11, cy + 3, 2, UI_CORAL);
      break;

    case ROM_GB:
    case ROM_GBC:
      menuCanvas->fillRoundRect(cx - 16, cy - 26, 32, 54, 4, (type == ROM_GB) ? UI_TEAL : UI_BLUE);
      menuCanvas->fillRect(cx - 12, cy - 22, 24, 20, UI_DEEP_TEAL);
      menuCanvas->fillRect(cx - 9, cy - 19, 18, 14, UI_MINT);
      menuCanvas->drawPixel(cx - 4, cy - 14, UI_BLACK);
      menuCanvas->drawPixel(cx + 4, cy - 14, UI_BLACK);
      menuCanvas->drawFastHLine(cx - 2, cy - 10, 4, UI_BLACK);
      menuCanvas->fillRect(cx - 12, cy + 6, 8, 3, UI_YELLOW);
      menuCanvas->fillRect(cx - 10, cy + 4, 4, 7, UI_YELLOW);
      menuCanvas->fillCircle(cx + 8, cy + 7, 2, UI_CORAL);
      menuCanvas->fillCircle(cx + 3, cy + 11, 2, UI_CORAL);
      break;

    case ROM_NES:
      menuCanvas->fillRoundRect(cx - 24, cy - 14, 48, 28, 3, UI_WHITE);
      menuCanvas->fillRect(cx - 21, cy - 11, 42, 22, UI_BLACK);
      menuCanvas->fillRect(cx - 18, cy - 2, 10, 4, UI_WHITE);
      menuCanvas->fillRect(cx - 15, cy - 5, 4, 10, UI_WHITE);
      menuCanvas->fillCircle(cx + 10, cy + 1, 3, UI_CORAL);
      menuCanvas->fillCircle(cx + 17, cy + 1, 3, UI_CORAL);
      break;

    case ROM_SNES:
      menuCanvas->fillRoundRect(cx - 26, cy - 14, 52, 28, 12, UI_WHITE);
      menuCanvas->fillRoundRect(cx - 22, cy - 11, 44, 22, 9, UI_MUTED);
      menuCanvas->fillRect(cx - 18, cy - 2, 8, 3, UI_BLACK);
      menuCanvas->fillRect(cx - 16, cy - 4, 3, 7, UI_BLACK);
      menuCanvas->fillCircle(cx + 14, cy - 5, 2, UI_YELLOW);
      menuCanvas->fillCircle(cx + 9, cy, 2, Theme::COLOR_SUCCESS);
      menuCanvas->fillCircle(cx + 19, cy, 2, UI_BLUE);
      menuCanvas->fillCircle(cx + 14, cy + 5, 2, UI_CORAL);
      break;

    case ROM_GENESIS:
      menuCanvas->fillRoundRect(cx - 24, cy - 14, 48, 28, 10, UI_BLACK);
      menuCanvas->drawRoundRect(cx - 24, cy - 14, 48, 28, 10, UI_MUTED);
      menuCanvas->fillRect(cx - 3, cy - 8, 6, 2, UI_CORAL);
      menuCanvas->fillCircle(cx - 13, cy + 2, 6, UI_MUTED);
      menuCanvas->fillCircle(cx + 6, cy + 5, 2, UI_WHITE);
      menuCanvas->fillCircle(cx + 12, cy + 2, 2, UI_WHITE);
      menuCanvas->fillCircle(cx + 18, cy - 1, 2, UI_WHITE);
      break;

    case ROM_SMS:
    case ROM_GG:
      menuCanvas->fillRoundRect(cx - 25, cy - 16, 50, 32, 6, UI_BLACK);
      menuCanvas->drawRoundRect(cx - 25, cy - 16, 50, 32, 6, UI_TEAL);
      menuCanvas->fillRect(cx - 14, cy - 10, 28, 20, UI_BLUE);
      menuCanvas->fillCircle(cx + 18, cy - 2, 2, UI_CORAL);
      menuCanvas->fillCircle(cx + 18, cy + 5, 2, UI_YELLOW);
      break;

    case ROM_ATARI:
      menuCanvas->fillRoundRect(cx - 16, cy - 4, 32, 26, 4, UI_BLACK);
      menuCanvas->drawRoundRect(cx - 16, cy - 4, 32, 26, 4, UI_YELLOW);
      menuCanvas->drawCircle(cx, cy + 9, 7, UI_CORAL);
      menuCanvas->fillRect(cx - 3, cy - 22, 6, 20, UI_BLACK);
      menuCanvas->fillCircle(cx, cy - 22, 5, UI_CORAL);
      menuCanvas->fillCircle(cx - 10, cy + 2, 3, UI_CORAL);
      break;

    case ROM_PICO8:
      menuCanvas->fillRoundRect(cx - 16, cy - 24, 32, 48, 4, UI_DEEP_TEAL);
      menuCanvas->fillRect(cx - 12, cy - 18, 24, 26, UI_YELLOW);
      menuCanvas->fillRect(cx - 12, cy + 10, 5, 4, UI_CORAL);
      menuCanvas->fillRect(cx - 7, cy + 10, 5, 4, UI_YELLOW);
      menuCanvas->fillRect(cx - 2, cy + 10, 5, 4, Theme::COLOR_SUCCESS);
      menuCanvas->fillRect(cx + 3, cy + 10, 5, 4, UI_BLUE);
      menuCanvas->fillRect(cx + 8, cy + 10, 4, 4, UI_WHITE);
      break;

    case ROM_WAD:
      menuCanvas->fillRoundRect(cx - 16, cy - 18, 32, 36, 6, UI_BLACK);
      menuCanvas->drawRoundRect(cx - 16, cy - 18, 32, 36, 6, UI_CORAL);
      menuCanvas->fillRect(cx - 11, cy - 8, 22, 8, Theme::COLOR_SUCCESS);
      break;

    default:
      menuCanvas->fillRoundRect(cx - 16, cy - 22, 32, 44, 4, UI_BLACK);
      menuCanvas->drawRoundRect(cx - 16, cy - 22, 32, 44, 4, UI_YELLOW);
      menuCanvas->fillRect(cx - 11, cy - 15, 22, 18, UI_TEAL);
      menuCanvas->setFont();
      menuCanvas->setCursor(cx - 8, cy - 4);
      menuCanvas->setTextColor(UI_BLACK);
      menuCanvas->print(consoleBadge(type));
      break;
  }
}

void DisplayEmu::drawIdleMascotScreen(unsigned long idleSeconds, const char* stateMessage) {
  if (!menuCanvas) return;
  menuCanvas->fillScreen(UI_BLACK);
  
  // Header bar
  menuCanvas->fillRect(0, 0, 320, 32, UI_DEEP_TEAL);
  menuCanvas->setFont(&FreeSans9pt7b);
  drawCentered("BMO IS DREAMING...", 22, UI_YELLOW);

  // Large living BMO face
  const int cx = 160;
  const int cy = 110;
  
  // BMO Body Card
  menuCanvas->fillRoundRect(cx - 70, cy - 50, 140, 100, 16, UI_TEAL);
  menuCanvas->drawRoundRect(cx - 70, cy - 50, 140, 100, 16, UI_DEEP_TEAL);
  
  // Eyes
  const unsigned long phase = (idleSeconds / 3) % 4;
  if (phase == 0) {
    menuCanvas->fillCircle(cx - 30, cy - 10, 7, UI_BLACK);
    menuCanvas->fillCircle(cx + 30, cy - 10, 7, UI_BLACK);
    menuCanvas->drawCircle(cx, cy + 12, 14, UI_BLACK);
    menuCanvas->fillRect(cx - 16, cy - 2, 32, 14, UI_TEAL);
  } else if (phase == 1) {
    menuCanvas->fillRect(cx - 36, cy - 12, 14, 5, UI_BLACK);
    menuCanvas->fillRect(cx + 22, cy - 12, 14, 5, UI_BLACK);
    menuCanvas->drawFastHLine(cx - 10, cy + 15, 20, UI_BLACK);
  } else if (phase == 2) {
    menuCanvas->drawLine(cx - 36, cy - 6, cx - 29, cy - 14, UI_BLACK);
    menuCanvas->drawLine(cx - 29, cy - 14, cx - 22, cy - 6, UI_BLACK);
    menuCanvas->drawLine(cx + 22, cy - 6, cx + 29, cy - 14, UI_BLACK);
    menuCanvas->drawLine(cx + 29, cy - 14, cx + 36, cy - 6, UI_BLACK);
    menuCanvas->drawCircle(cx, cy + 10, 8, UI_BLACK);
    menuCanvas->fillRect(cx - 10, cy + 2, 20, 8, UI_TEAL);
  } else {
    menuCanvas->fillCircle(cx - 30, cy - 10, 7, UI_BLACK);
    menuCanvas->fillCircle(cx + 30, cy - 10, 7, UI_BLACK);
    menuCanvas->fillCircle(cx, cy + 12, 12, UI_BLACK);
    menuCanvas->fillRect(cx - 14, cy, 28, 12, UI_TEAL);
  }

  // Rosy cheeks
  menuCanvas->fillCircle(cx - 48, cy + 2, 6, UI_CORAL);
  menuCanvas->fillCircle(cx + 48, cy + 2, 6, UI_CORAL);

  // Floating Z z z
  menuCanvas->setFont();
  menuCanvas->setTextColor(UI_DEEP_TEAL);
  menuCanvas->setCursor(cx + 80, cy - 35);
  menuCanvas->print("Z");
  menuCanvas->setCursor(cx + 92, cy - 48);
  menuCanvas->print("z");
  menuCanvas->setCursor(cx + 102, cy - 60);
  menuCanvas->print("z");

  // Bottom prompt
  drawFooter(stateMessage ? stateMessage : "PRESS ANY BUTTON TO WAKE UP BMO!");
  writeMenuCanvas();
}


