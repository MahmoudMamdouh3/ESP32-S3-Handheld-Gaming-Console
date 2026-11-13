#include "box_art.h"
#include "box_art_lookup.h"
#include "sd_card.h"
#include "../hal/config.h"
#include "../hal/spi_arbiter.h"
#include "../ui/theme.h"
#include "path_key.h"
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

#if FEATURE_SD_CARD
bool sameNameIgnoreAsciiCase(const char* left, const char* right) {
  return BoxArtLookup::sameNameIgnoreAsciiCase(left, right);
}

void markArtDirectory(const char* folderPath, BoxArtLookup::CatalogMatcher& matcher) {
  File dir = SD.open(folderPath);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      matcher.markArtFile(entry.name());
    }
    entry.close();
  }
  dir.close();
}

bool legacyBaseIsUnique(const char* romFilename, const char* legacyBase) {
  if (!romFilename || !legacyBase || legacyBase[0] == '\0') return false;
  const int romCount = SDCard::getRomCount();
  for (int i = 0; i < romCount; ++i) {
    const RomFile* other = SDCard::getRomInfo(i);
    if (!other || strcmp(other->filename, romFilename) == 0) continue;
    char otherBase[64];
    BoxArt::getBaseName(other->filename, otherBase, sizeof(otherBase));
    if (sameNameIgnoreAsciiCase(legacyBase, otherBase)) return false;
  }
  return true;
}

bool migrateLegacyArt(const char* sourcePath, const char* destinationPath) {
  if (!sourcePath || !destinationPath || SD.exists(destinationPath)) return false;
  char temporaryPath[144];
  const int pathLength = snprintf(temporaryPath, sizeof(temporaryPath), "%s.tmp", destinationPath);
  if (pathLength <= 0 || static_cast<size_t>(pathLength) >= sizeof(temporaryPath)) return false;
  if (SD.exists(temporaryPath)) SD.remove(temporaryPath);

  File source = SD.open(sourcePath, FILE_READ);
  if (!source) return false;
  const size_t expectedSize = source.size();
  File target = SD.open(temporaryPath, FILE_WRITE);
  if (!target) {
    source.close();
    return false;
  }

  uint8_t buffer[256];
  size_t copied = 0;
  bool writeOk = true;
  while (source.available()) {
    const int bytesRead = source.read(buffer, sizeof(buffer));
    if (bytesRead <= 0 || target.write(buffer, static_cast<size_t>(bytesRead)) != static_cast<size_t>(bytesRead)) {
      writeOk = false;
      break;
    }
    copied += static_cast<size_t>(bytesRead);
  }
  target.flush();
  target.close();
  source.close();
  if (!writeOk || copied != expectedSize) return false;

  File verification = SD.open(temporaryPath, FILE_READ);
  const bool verified = verification && verification.size() == expectedSize;
  if (verification) verification.close();
  if (!verified || !SD.rename(temporaryPath, destinationPath)) return false;

  File migrated = SD.open(destinationPath, FILE_READ);
  const bool finalVerified = migrated && migrated.size() == expectedSize;
  if (migrated) migrated.close();
  return finalVerified;
}
#endif
} // namespace

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
  const bool hadArt = s_hasArt;
  s_cachedRomName[0] = '\0';
  s_hasArt = false;
  if (hadArt && s_boxArtBuf) {
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
  BoxArtLookup::getBaseName(romFilename, outBaseName, maxLen);
}

void BoxArt::populateCatalogAvailability(RomFile* roms, int count) {
  if (!roms || count <= 0) return;
  for (int i = 0; i < count; ++i) {
    roms[i].hasBoxArt = 0;
  }
#if FEATURE_SD_CARD
  if (!SDCard::isMounted()) return;

  bool hasBoxartDir = false;
  bool hasCoversDir = false;
  {
    SpiArbiter::ScopedLock spiLock;
    hasBoxartDir = SD.exists("/boxart");
    hasCoversDir = SD.exists("/covers");
  }
  if (!hasBoxartDir && !hasCoversDir) {
    return;
  }

  auto* keysBuf = static_cast<BoxArtLookup::KeyEntry*>(
      heap_caps_malloc(static_cast<size_t>(count) * sizeof(BoxArtLookup::KeyEntry), MALLOC_CAP_SPIRAM));
  auto* baseOrderBuf =
      static_cast<int32_t*>(heap_caps_malloc(static_cast<size_t>(count) * sizeof(int32_t), MALLOC_CAP_SPIRAM));

  BoxArtLookup::CatalogMatcher matcher;
  if (keysBuf && baseOrderBuf && matcher.init(roms, count, keysBuf, baseOrderBuf)) {
    SpiArbiter::ScopedLock spiLock;
    if (hasBoxartDir) markArtDirectory("/boxart", matcher);
    if (hasCoversDir) markArtDirectory("/covers", matcher);
  } else {
    for (int i = 0; i < count; ++i) {
      roms[i].hasBoxArt = existsForRom(roms[i].filename) ? 1 : 0;
    }
  }

  if (keysBuf) heap_caps_free(keysBuf);
  if (baseOrderBuf) heap_caps_free(baseOrderBuf);
#endif
}

bool BoxArt::existsForRom(const char* romFilename) {
#if FEATURE_SD_CARD
  if (!romFilename || !SDCard::isMounted()) return false;

  char key[33];
  if (!RomKey::make(romFilename, key, sizeof(key))) return false;
  char legacyBase[64];
  getBaseName(romFilename, legacyBase, sizeof(legacyBase));

  char path[128];
  SpiArbiter::ScopedLock spiLock;

  const char* folders[] = {"/boxart", "/covers"};
  const char* extensions[] = {"raw", "bmp"};
  for (const char* folder : folders) {
    for (const char* extension : extensions) {
      snprintf(path, sizeof(path), "%s/%s.%s", folder, key, extension);
      if (SD.exists(path)) return true;
    }
  }
  if (legacyBaseIsUnique(romFilename, legacyBase)) {
    for (const char* folder : folders) {
      for (const char* extension : extensions) {
        snprintf(path, sizeof(path), "%s/%s.%s", folder, legacyBase, extension);
        if (SD.exists(path)) return true;
      }
    }
  }
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

  char key[33];
  if (!RomKey::make(romFilename, key, sizeof(key))) return false;
  char legacyBase[64];
  getBaseName(romFilename, legacyBase, sizeof(legacyBase));

  const bool legacyBaseUnique = legacyBaseIsUnique(romFilename, legacyBase);
  const char* folders[] = {"/boxart", "/covers"};
  const char* extensions[] = {"raw", "bmp"};
  const char* stems[2] = {key, legacyBase};
  const size_t stemCount = legacyBaseUnique ? 2U : 1U;
  char candidatePath[128];

  SpiArbiter::ScopedLock spiLock;
  for (size_t s = 0; s < stemCount; ++s) {
    for (const char* folder : folders) {
      for (const char* extension : extensions) {
        const int written = snprintf(candidatePath, sizeof(candidatePath), "%s/%s.%s", folder, stems[s], extension);
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(candidatePath)) continue;
        const bool isBmp = (strcmp(extension, "bmp") == 0);
        if (isBmp ? loadBmp(candidatePath) : loadRaw(candidatePath)) {
          s_hasArt = true;
          if (s == 1U && legacyBaseUnique) {
            char keyPath[128];
            const int keyLen = snprintf(keyPath, sizeof(keyPath), "%s/%s.%s", folder, key, extension);
            if (keyLen > 0 && static_cast<size_t>(keyLen) < sizeof(keyPath)) {
              migrateLegacyArt(candidatePath, keyPath);
            }
          }
          return true;
        }
      }
    }
  }
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
  int32_t width = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
  int32_t height = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
  uint16_t bpp = header[28] | (header[29] << 8);

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
      int srcOffsetX = copyX - x;
      int copyW = 64 - srcOffsetX;
      if (copyX + copyW > 320) copyW = 320 - copyX;
      if (copyW > 0) {
        memcpy(dst + (canvasY * 320 + copyX), s_boxArtBuf + (r * 64 + srcOffsetX), copyW * sizeof(uint16_t));
      }
    }
  }

  if (drawBorder) {
    canvas->drawRoundRect(x - 2, y - 2, 68, 68, 3, borderColor);
  }
}
