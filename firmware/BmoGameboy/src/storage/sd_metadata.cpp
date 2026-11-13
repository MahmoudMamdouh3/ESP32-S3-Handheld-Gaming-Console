#include "sd_internal.h"
#include "../hal/config.h"
#include "../hal/spi_arbiter.h"
#include "atomic_fs.h"
#include "sd_atomic_ops.h"
#if FEATURE_SD_CARD
#include <SD.h>
#endif
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr int MAX_RECENT_HISTORY = 10;
char s_recentHistory[MAX_RECENT_HISTORY][64];
int s_recentCount = 0;

#if FEATURE_SD_CARD
bool writeMetadataLine(File& file, const char* line, size_t& byteCount, uint32_t& crc) {
  if (!line) return false;
  const size_t length = strlen(line);
  if (file.write((const uint8_t*)line, length) != length) return false;
  static const uint8_t lineEnding[] = {'\r', '\n'};
  if (file.write(lineEnding, sizeof(lineEnding)) != sizeof(lineEnding)) return false;
  crc = AtomicFs::crc32Update((const uint8_t*)line, length, crc);
  crc = AtomicFs::crc32Update(lineEnding, sizeof(lineEnding), crc);
  byteCount += length + sizeof(lineEnding);
  return true;
}
#endif

} // namespace

bool SDCard::isFavorite(int index) {
  using namespace BmoSdInternal;
  if (index < 0 || index >= numRoms) return false;
  return romList[index].isFavorite;
}

bool SDCard::isFavorite(const char* filename) {
  using namespace BmoSdInternal;
  if (!filename) return false;
  for (int i = 0; i < numRoms; ++i) {
    if (strcmp(romList[i].filename, filename) == 0) {
      return romList[i].isFavorite;
    }
  }
  return false;
}

void SDCard::toggleFavorite(int index) {
  using namespace BmoSdInternal;
  if (index < 0 || index >= numRoms) return;
  if (romList[index].isFavorite) {
    romList[index].isFavorite = false;
    favoritesCount--;
  } else {
    romList[index].isFavorite = true;
    favoritesCount++;
  }
  saveFavorites();
}

void SDCard::recordRecentLaunch(const char* filename) {
  if (!filename || filename[0] == '\0') return;
  int existing = -1;
  for (int i = 0; i < s_recentCount; ++i) {
    if (strcmp(s_recentHistory[i], filename) == 0) {
      existing = i;
      break;
    }
  }
  const int limit =
      (existing >= 0) ? existing : ((s_recentCount < MAX_RECENT_HISTORY) ? s_recentCount++ : (MAX_RECENT_HISTORY - 1));
  for (int i = limit; i > 0; --i) {
    memcpy(s_recentHistory[i], s_recentHistory[i - 1], 64);
  }
  strncpy(s_recentHistory[0], filename, 63);
  s_recentHistory[0][63] = '\0';
  saveRecentHistory();
}

int SDCard::getRecentCount() {
  return s_recentCount;
}

const char* SDCard::getRecentFilename(int index) {
  if (index < 0 || index >= s_recentCount) return nullptr;
  return s_recentHistory[index];
}

void SDCard::saveRecentHistory() {
#if FEATURE_SD_CARD
  if (!BmoSdInternal::mounted) return;
  bool ok = false;
  bool backupDeferred = false;
  {
    SpiArbiter::ScopedLock spiLock;
    const char* finalPath = "/history.txt";
    char temporaryPath[64];
    const AtomicFs::FsOps ops = SdAtomicOps::makeOps();
    ok = AtomicFs::recoverTransaction(ops, finalPath) &&
         AtomicFs::makeTempPath(finalPath, temporaryPath, sizeof(temporaryPath));
    if (ok && SD.exists(temporaryPath)) ok = SD.remove(temporaryPath);
    File f = ok ? SD.open(temporaryPath, FILE_WRITE) : File();
    if (f) {
      size_t byteCount = 0;
      uint32_t crc = 0xFFFFFFFFU;
      for (int i = 0; i < s_recentCount && ok; ++i) {
        ok = writeMetadataLine(f, s_recentHistory[i], byteCount, crc);
      }
      f.flush();
      f.close();
      ok = ok && AtomicFs::commitTransaction(ops, temporaryPath, finalPath, byteCount, 0, byteCount, crc ^ 0xFFFFFFFFU,
                                             &backupDeferred);
    } else {
      ok = false;
    }
  }
  if (!ok) {
    LOG_WARN_STR("SDCard: Could not commit recent-history metadata");
  } else if (backupDeferred) {
    LOG_WARN_STR("SDCard: Verified recent history; deferred backup cleanup");
  }
#endif
}

void SDCard::loadRecentHistory() {
  s_recentCount = 0;
#if FEATURE_SD_CARD
  if (!BmoSdInternal::mounted) return;
  SpiArbiter::ScopedLock spiLock;
  const AtomicFs::FsOps ops = SdAtomicOps::makeOps();
  if (!AtomicFs::recoverTransaction(ops, "/history.txt")) {
    return;
  }
  if (!SD.exists("/history.txt")) {
    return;
  }
  File f = SD.open("/history.txt", FILE_READ);
  if (!f) {
    return;
  }
  char line[64];
  while (f.available() && s_recentCount < MAX_RECENT_HISTORY) {
    int len = f.readBytesUntil('\n', line, (int)sizeof(line) - 1);
    line[len] = '\0';
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ')) {
      line[--len] = '\0';
    }
    if (len > 0) {
      strncpy(s_recentHistory[s_recentCount], line, 63);
      s_recentHistory[s_recentCount][63] = '\0';
      s_recentCount++;
    }
  }
  f.close();
#endif
}

int SDCard::getFavoritesCount() {
  return BmoSdInternal::favoritesCount;
}

void SDCard::saveFavorites() {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  if (!mounted) return;
  bool ok = false;
  bool backupDeferred = false;
  {
    SpiArbiter::ScopedLock spiLock;
    const char* finalPath = "/favorites.txt";
    char temporaryPath[64];
    const AtomicFs::FsOps ops = SdAtomicOps::makeOps();
    ok = AtomicFs::recoverTransaction(ops, finalPath) &&
         AtomicFs::makeTempPath(finalPath, temporaryPath, sizeof(temporaryPath));
    if (ok && SD.exists(temporaryPath)) ok = SD.remove(temporaryPath);
    File f = ok ? SD.open(temporaryPath, FILE_WRITE) : File();
    if (!f) {
      LOG_WARN_STR("SDCard: Could not stage favorite metadata");
      return;
    }
    size_t byteCount = 0;
    uint32_t crc = 0xFFFFFFFFU;
    for (int i = 0; i < numRoms; ++i) {
      if (romList[i].isFavorite) {
        if (!writeMetadataLine(f, romList[i].filename, byteCount, crc)) {
          ok = false;
          break;
        }
      }
    }
    f.flush();
    f.close();
    ok = ok && AtomicFs::commitTransaction(ops, temporaryPath, finalPath, byteCount, 0, byteCount, crc ^ 0xFFFFFFFFU,
                                           &backupDeferred);
  }
  if (!ok) {
    LOG_WARN_STR("SDCard: Could not commit favorite metadata");
  } else if (backupDeferred) {
    LOG_WARN_STR("SDCard: Verified favorites; deferred backup cleanup");
  }
#endif
}

void SDCard::loadFavorites() {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  if (!mounted) return;
  SpiArbiter::ScopedLock spiLock;
  const AtomicFs::FsOps ops = SdAtomicOps::makeOps();
  if (!AtomicFs::recoverTransaction(ops, "/favorites.txt")) {
    return;
  }
  if (!SD.exists("/favorites.txt")) {
    return;
  }
  File f = SD.open("/favorites.txt", FILE_READ);
  if (!f) {
    return;
  }
  favoritesCount = 0;
  for (int i = 0; i < numRoms; ++i) {
    romList[i].isFavorite = false;
  }
  char line[64];
  while (f.available()) {
    int len = f.readBytesUntil('\n', line, (int)sizeof(line) - 1);
    line[len] = '\0';
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ')) {
      line[--len] = '\0';
    }
    if (len > 0) {
      for (int i = 0; i < numRoms; ++i) {
        if (strcmp(romList[i].filename, line) == 0) {
          if (!romList[i].isFavorite) {
            romList[i].isFavorite = true;
            favoritesCount++;
          }
          break;
        }
      }
    }
  }
  f.close();
#endif
}

uint8_t* SDCard::loadRom(const char* filename, size_t* outSize) {
  if (!filename || !outSize) return nullptr;
  *outSize = 0;

  const uint8_t* baked = BmoSdInternal::tryLoadBakedRom(filename, outSize);
  if (baked) {
    return const_cast<uint8_t*>(baked);
  }

#if FEATURE_SD_CARD
  char path[128];
  snprintf(path, sizeof(path), "/%s", filename);
  uint8_t* buffer = nullptr;
  size_t size = 0;
  size_t bytesRead = 0;
  {
    SpiArbiter::ScopedLock spiLock;
    File file = SD.open(path, FILE_READ);
    if (!file) {
      return nullptr;
    }

    size = file.size();
    if (size == 0) {
      file.close();
      return nullptr;
    }
    *outSize = size;

    buffer = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!buffer) {
      file.close();
      return nullptr;
    }

    while (bytesRead < size) {
      const size_t toRead = (size - bytesRead > 65536) ? 65536 : (size - bytesRead);
      const int chunk = file.read(buffer + bytesRead, toRead);
      if (chunk <= 0) break;
      bytesRead += (size_t)chunk;
    }
    file.close();
  }
  if (bytesRead != size) {
    heap_caps_free(buffer);
    *outSize = 0;
    return nullptr;
  }
  return buffer;
#else
  return nullptr;
#endif
}

void SDCard::freeRom(uint8_t* buffer) {
  if (buffer) {
    if (BmoSdInternal::isBakedRomPointer(buffer)) {
      return;
    }
    heap_caps_free(buffer);
  }
}
