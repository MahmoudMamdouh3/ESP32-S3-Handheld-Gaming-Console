#pragma once
#include <stdint.h>
#include <stddef.h>
#include <Adafruit_GFX.h>

// BoxArt: 64x64 Retro Box Art & Thumbnail Engine
// Supports 64x64 uncompressed raw 16-bit (RGB565/BGR565) and 16/24-bit BMP images
// from /boxart/ or /covers/ on MicroSD card.
// All buffer allocations strictly reside in Octal PSRAM (MALLOC_CAP_SPIRAM).

class BoxArt {
public:
  static const int WIDTH = 64;
  static const int HEIGHT = 64;
  static const size_t PIXEL_COUNT = WIDTH * HEIGHT;
  static const size_t BUFFER_SIZE = PIXEL_COUNT * sizeof(uint16_t); // 8,192 bytes

  static bool init();
  static bool load(const char* romFilename);
  static void unload();
  static bool hasArt();
  static const uint16_t* getBuffer();

  // Renders the box art onto the canvas with an optional border
  static void draw(GFXcanvas16* canvas, int x, int y, bool drawBorder = true, uint16_t borderColor = 0xFFFF);

  // Checks if a box art file exists on the SD card for a given ROM
  static bool existsForRom(const char* romFilename);

  // Helper to extract the basename without file extension into outBaseName
  static void getBaseName(const char* romFilename, char* outBaseName, size_t maxLen);

private:
  static bool loadRaw(const char* path);
  static bool loadBmp(const char* path);
};
