#pragma once
#include <stdint.h>

namespace DisplayEmu {
  // Initialize the display. MUST be called after SPI.begin().
  void begin();

  // Clear the screen to black before launching a game
  void clearScreen();

  // Draw the initial emulator selection menu
  void drawEmulatorSelectMenu(int selectedIndex);
  void showSDCardWarning();
  void drawMenu(const char** titles, int count, int selectedIndex, const uint16_t* selectedCover, bool useColorEmulator);

  // Draw an array of RGB565 pixels directly to the TFT.
  void pushPixels(int yOffset, const uint16_t* rowBuffer, int rowsToDraw);



  // Classic Game Boy "Pea-Soup Green" palette in RGB565 (Little-Endian swapped)
  extern const uint16_t CLASSIC_PALETTE[4];
}
