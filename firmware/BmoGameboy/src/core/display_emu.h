#pragma once
#include <stdint.h>
#include "sd_card.h"

namespace DisplayEmu {
  // Initialize the display. MUST be called after SPI.begin().
  void begin();

  // Clear the screen to black before launching a game.
  void clearScreen();

  // Menu drawing functions.  The menu has its own pixel-format conversion so
  // it remains correct without changing any emulator palette or frame stream.
  void showSDCardWarning();
  void initMenuUI();
  void drawConsoleSelectMenu(int selectedIndex, const int gameCounts[4], bool sdMounted);
  void drawGameSelectMenu(const RomFile* const* games, int count, int selectedIndex,
                          RomType console, bool sdMounted);
  void cleanupMenuUI();

  // ---------------------------------------------------------------------------
  // Pixel push API
  // ---------------------------------------------------------------------------

  // NES 64-color palette in BGR565 (byte-swapped for SPI)
  extern const uint16_t NES_PALETTE[64];

  // Starts an SPI transaction and streams exactly 144 lines of Game Boy video
  // scaled 1.5x via nearest-neighbor to the 240x216 centered display window.
  void streamFrame(const uint8_t* gb_framebuffer, uint16_t* palette);

  // Starts an SPI transaction and streams exactly 240 lines of NES video (256x240)
  // scaled/cropped to the 240x216 centered display window.
  void streamNESFrame(const uint8_t* nes_framebuffer);

  // Starts an SPI transaction and streams 320x200 DOOM pixels.
  void streamDoomFrame(const uint8_t* cmap);

  // pushPixels: full self-contained transaction (startWrite + setAddrWindow +
  // writeBytes + endWrite). Use for cover art or any one-off blit.
  void pushPixels(int yOffset, const uint16_t* rowBuffer, int rowsToDraw);

  // pushPixelsRaw: caller owns the SPI bus (must be inside startWrite/endWrite).
  // Issues setAddrWindow then writeBytes.
  void pushPixelsRaw(int yOffset, const uint16_t* rowBuffer, int rowsToDraw);

  // pushPixelsFullScreen: blits a full 320x240 frame (e.g. for BMO face)
  void pushPixelsFullScreen(const uint16_t* buffer);

  // pushPixelsAt: self-contained transaction that blits a w×h region from
  // 'buf' at display position (x, y).  Same semantics as pushPixels but
  // unrestricted — not tied to the Game Boy OFFSET_X/Y viewport window.
  // Used by BmoFace to place the mascot face anywhere on the 320×240 display.
  void pushPixelsAt(int x, int y, int w, int h, const uint16_t* buf);


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
