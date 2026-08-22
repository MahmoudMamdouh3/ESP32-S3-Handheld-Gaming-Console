#pragma once
#include <stdint.h>

struct gb_s; // Forward declaration

namespace DisplayEmu {
  // Initialize the display. MUST be called after SPI.begin().
  void begin();

  // Draw the game selection menu
  void drawMenu(const char** titles, int count, int selectedIndex);

  // Peanut-GB callback: called once per scanline (144 times per frame).
  void drawScanline(struct gb_s *gb, const uint8_t pixels[160], const uint_fast8_t line);
}
