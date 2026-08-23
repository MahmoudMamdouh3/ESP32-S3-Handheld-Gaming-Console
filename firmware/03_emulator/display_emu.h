#pragma once
#include <stdint.h>

namespace DisplayEmu {
  // Initialize the display. MUST be called after SPI.begin().
  void begin();

  // Clear the screen to black before launching a game.
  void clearScreen();

  // Menu drawing functions
  void drawEmulatorSelectMenu(int selectedIndex);
  void showSDCardWarning();
  void drawMenu(const char** titles, int count, int selectedIndex,
                const uint16_t* selectedCover, bool useColorEmulator);

  // ---------------------------------------------------------------------------
  // Pixel push API
  // ---------------------------------------------------------------------------

  // pushPixels: full self-contained transaction (startWrite + setAddrWindow +
  // writeBytes + endWrite). Use for cover art or any one-off blit.
  void pushPixels(int yOffset, const uint16_t* rowBuffer, int rowsToDraw);

  // pushPixelsRaw: caller owns the SPI bus (must be inside startWrite/endWrite).
  // Issues setAddrWindow then writeBytes.
  void pushPixelsRaw(int yOffset, const uint16_t* rowBuffer, int rowsToDraw);

  // ---------------------------------------------------------------------------
  // High-performance frame rendering (N3)
  // ---------------------------------------------------------------------------
  // Protocol:
  //   DisplayEmu::startFrame();       // assert CS, setAddrWindow(240x216) ONCE
  //   gb_run_frame(...);              // lcd_draw_line calls streamPixelRow()
  //   DisplayEmu::endFrame();         // deassert CS
  //
  // startFrame() sets the address window once for the entire 240×216 viewport.
  // streamPixelRow() then just writes raw bytes — no DC toggle, no commands,
  // no CS transitions between scanlines.
  // This reduces setAddrWindow calls from 144/frame to 1/frame.
  void startFrame();
  void endFrame();

  // streamPixelRow: bare pixel data push. MUST be called inside startFrame/endFrame.
  // pixelCount = number of uint16_t pixels (240 for 1 row, 480 for a doubled row).
  void streamPixelRow(const uint16_t* buf, int pixelCount);

  // Classic Game Boy "Pea-Soup Green" palette in BGR565 (byte-swapped for SPI)
  extern const uint16_t CLASSIC_PALETTE[4];
}
