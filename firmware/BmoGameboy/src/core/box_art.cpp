#include "box_art.h"
#include "sd_card.h"
#include "config.h"
#include "spi_arbiter.h"
#include "theme.h"
#include <esp_heap_caps.h>
#include <string.h>
#if FEATURE_SD_CARD
#include <SD.h>
#endif

namespace {
  static uint16_t* s_boxArtBuf = nullptr;
  static char s_cachedRomName[64] = "";
  static bool s_hasArt = false;

  uint16_t rgbToDisplayColor(uint8_t r, uint8_t g, uint8_t b) {
    return Theme::makeUiColor(r, g, b);
  }
}

bool BoxArt::init() {
  if (!s_boxArtBuf) {
    s_boxArtBuf = (uint16_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_boxArtBuf) {
      return false;
    }
  }
  unload();
  return true;
}

void BoxArt::unload() {
  s_cachedRomName[0] = '\0';
  s_hasArt = false;
  if (s_boxArtBuf) {
    memset(s_boxArtBuf, 0, BUFFER_SIZE);
  }
}

bool BoxArt::hasArt() {
  return s_hasArt && (s_boxArtBuf != nullptr);
}

const uint16_t* BoxArt::getBuffer() {
  return s_boxArtBuf;
}

void BoxArt::getBaseName(const char* romFilename, char* outBaseName, size_t maxLen) {
  if (!romFilename || !outBaseName || maxLen == 0) return;
  outBaseName[0] = '\0';
  
  // Strip path if present
  const char* base = strrchr(romFilename, '/');
  if (!base) base = strrchr(romFilename, '\\');
  base = base ? base + 1 : romFilename;
  
  strncpy(outBaseName, base, maxLen - 1);
  outBaseName[maxLen - 1] = '\0';
  
  // Strip file extension
  char* dot = strrchr(outBaseName, '.');
  if (dot) {
    *dot = '\0';
  }
}

bool BoxArt::existsForRom(const char* romFilename) {
#if FEATURE_SD_CARD
  if (!romFilename || !SDCard::isMounted()) return false;
  
  char baseName[64];
  getBaseName(romFilename, baseName, sizeof(baseName));
  if (baseName[0] == '\0') return false;

  char path[128];
  SpiArbiter::lock();
  
  snprintf(path, sizeof(path), "/boxart/%s.raw", baseName);
  if (SD.exists(path)) { SpiArbiter::unlock(); return true; }

  snprintf(path, sizeof(path), "/boxart/%s.bmp", baseName);
  if (SD.exists(path)) { SpiArbiter::unlock(); return true; }

  snprintf(path, sizeof(path), "/covers/%s.raw", baseName);
  if (SD.exists(path)) { SpiArbiter::unlock(); return true; }

  snprintf(path, sizeof(path), "/covers/%s.bmp", baseName);
  if (SD.exists(path)) { SpiArbiter::unlock(); return true; }

  SpiArbiter::unlock();
#endif
  return false;
}

bool BoxArt::load(const char* romFilename) {
#if FEATURE_SD_CARD
  if (!romFilename) {
    unload();
    return false;
  }

  // Check if this ROM's art is already loaded in cache
  if (s_hasArt && strncmp(s_cachedRomName, romFilename, sizeof(s_cachedRomName)) == 0) {
    return true;
  }

  if (!init()) {
    return false;
  }

  unload();
  strncpy(s_cachedRomName, romFilename, sizeof(s_cachedRomName) - 1);
  s_cachedRomName[sizeof(s_cachedRomName) - 1] = '\0';

  char baseName[64];
  getBaseName(romFilename, baseName, sizeof(baseName));
  if (baseName[0] == '\0') return false;

  char path[128];
  SpiArbiter::lock();

  // 1. Try /boxart/<base>.raw
  snprintf(path, sizeof(path), "/boxart/%s.raw", baseName);
  if (loadRaw(path)) {
    s_hasArt = true;
    SpiArbiter::unlock();
    return true;
  }

  // 2. Try /boxart/<base>.bmp
  snprintf(path, sizeof(path), "/boxart/%s.bmp", baseName);
  if (loadBmp(path)) {
    s_hasArt = true;
    SpiArbiter::unlock();
    return true;
  }

  // 3. Try /covers/<base>.raw
  snprintf(path, sizeof(path), "/covers/%s.raw", baseName);
  if (loadRaw(path)) {
    s_hasArt = true;
    SpiArbiter::unlock();
    return true;
  }

  // 4. Try /covers/<base>.bmp
  snprintf(path, sizeof(path), "/covers/%s.bmp", baseName);
  if (loadBmp(path)) {
    s_hasArt = true;
    SpiArbiter::unlock();
    return true;
  }

  SpiArbiter::unlock();
#endif
  return false;
}

bool BoxArt::loadRaw(const char* path) {
#if FEATURE_SD_CARD
  if (!SD.exists(path)) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  if (f.size() < BUFFER_SIZE) {
    f.close();
    return false;
  }

  size_t bytesRead = f.read((uint8_t*)s_boxArtBuf, BUFFER_SIZE);
  f.close();
  return (bytesRead == BUFFER_SIZE);
#else
  return false;
#endif
}

bool BoxArt::loadBmp(const char* path) {
#if FEATURE_SD_CARD
  if (!SD.exists(path)) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  uint8_t header[54];
  if (f.read(header, 54) != 54) {
    f.close();
    return false;
  }

  // Validate BM magic
  if (header[0] != 'B' || header[1] != 'M') {
    f.close();
    return false;
  }

  uint32_t dataOffset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
  int32_t width  = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
  int32_t height = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
  uint16_t bpp   = header[28] | (header[29] << 8);

  bool bottomUp = (height > 0);
  int absHeight = bottomUp ? height : -height;

  // We only support 64x64 BMPs (or close approximations)
  if (width != 64 || absHeight != 64 || (bpp != 24 && bpp != 16)) {
    f.close();
    return false;
  }

  if (!f.seek(dataOffset)) {
    f.close();
    return false;
  }

  if (bpp == 24) {
    // 24-bit BGR: 64 pixels * 3 bytes = 192 bytes per row (already 4-byte aligned)
    uint8_t rowBytes[192];
    for (int r = 0; r < 64; ++r) {
      int targetRow = bottomUp ? (63 - r) : r;
      if (f.read(rowBytes, sizeof(rowBytes)) != sizeof(rowBytes)) {
        f.close();
        return false;
      }
      for (int c = 0; c < 64; ++c) {
        uint8_t b = rowBytes[c * 3 + 0];
        uint8_t g = rowBytes[c * 3 + 1];
        uint8_t r_col = rowBytes[c * 3 + 2];
        s_boxArtBuf[targetRow * 64 + c] = rgbToDisplayColor(r_col, g, b);
      }
    }
  } else if (bpp == 16) {
    // 16-bit RGB555 or RGB565: 64 pixels * 2 bytes = 128 bytes per row
    uint16_t rowPixels[64];
    for (int r = 0; r < 64; ++r) {
      int targetRow = bottomUp ? (63 - r) : r;
      if (f.read((uint8_t*)rowPixels, sizeof(rowPixels)) != sizeof(rowPixels)) {
        f.close();
        return false;
      }
      for (int c = 0; c < 64; ++c) {
        uint16_t raw16 = rowPixels[c];
        // Standard 16-bit RGB565 extraction
        uint8_t r_col = (raw16 >> 8) & 0xF8;
        uint8_t g = (raw16 >> 3) & 0xFC;
        uint8_t b = (raw16 << 3) & 0xF8;
        s_boxArtBuf[targetRow * 64 + c] = rgbToDisplayColor(r_col, g, b);
      }
    }
  }

  f.close();
  return true;
#else
  return false;
#endif
}

void BoxArt::draw(GFXcanvas16* canvas, int x, int y, bool drawBorder, uint16_t borderColor) {
  if (!canvas || !s_hasArt || !s_boxArtBuf) return;

  uint16_t* dst = canvas->getBuffer();
  if (!dst) return;

  // Ultra-fast row-by-row memcpy into GFXcanvas (320px pitch)
  for (int r = 0; r < 64; ++r) {
    int canvasY = y + r;
    if (canvasY >= 0 && canvasY < 240) {
      int copyX = (x < 0) ? 0 : x;
      int copyW = 64;
      if (copyX + copyW > 320) copyW = 320 - copyX;
      if (copyW > 0) {
        memcpy(dst + (canvasY * 320 + copyX), s_boxArtBuf + (r * 64 + (copyX - x)), copyW * sizeof(uint16_t));
      }
    }
  }

  if (drawBorder) {
    canvas->drawRoundRect(x - 2, y - 2, 68, 68, 3, borderColor);
  }
}
