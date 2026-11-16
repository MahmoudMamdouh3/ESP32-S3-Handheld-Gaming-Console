// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "sd_scanner.h"

#include "../hal/config.h"
#include "../hal/spi_arbiter.h"
#include "../ui/console_catalog.h"
#include "atomic_fs.h"
#include "box_art.h"
#include "rom_file_filter.h"
#include "rom_index.h"
#include "sd_index_io.h"
#include "sd_internal.h"
#if FEATURE_SD_CARD
#include <Arduino.h>
#include <SD.h>
#if defined(__has_include)
#if __has_include(<ff.h>)
#include <ff.h>
#define BMO_HAS_FATFS_STAT 1
#endif
#endif
#endif
#ifndef BMO_HAS_FATFS_STAT
#define BMO_HAS_FATFS_STAT 0
#endif
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

namespace BmoSdScanner {
namespace {

constexpr int kMaxScanDepth = 3;
constexpr int kScanChunkEntries = 16;

struct DirQueueItem {
  char path[SDCard::MAX_ROM_PATH_LEN + 2U];
  int depth;
};

#if FEATURE_SD_CARD
bool isIgnoredDirName(const char* baseName) {
  if (!baseName || baseName[0] == '\0' || baseName[0] == '.') return true;
  return strcasecmp(baseName, "saves") == 0 || strcasecmp(baseName, "screenshots") == 0 ||
         strcasecmp(baseName, "boxart") == 0 || strcasecmp(baseName, "covers") == 0 ||
         strcasecmp(baseName, "System Volume Information") == 0 || strcasecmp(baseName, "LOST.DIR") == 0;
}

bool isFatHiddenOrSystem(const char* dirPrefixNoSlash, const char* baseName) {
#if BMO_HAS_FATFS_STAT
  if (!baseName || baseName[0] == '\0') return false;
  char fatPath[SDCard::MAX_ROM_PATH_LEN + 8U];
  const int written = (dirPrefixNoSlash && dirPrefixNoSlash[0] != '\0')
                          ? snprintf(fatPath, sizeof(fatPath), "0:/%s/%s", dirPrefixNoSlash, baseName)
                          : snprintf(fatPath, sizeof(fatPath), "0:/%s", baseName);
  if (written <= 0 || static_cast<size_t>(written) >= sizeof(fatPath)) {
    return false;
  }
  FILINFO fno;
  if (f_stat(fatPath, &fno) == FR_OK) {
    return (fno.fattrib & (AM_HID | AM_SYS)) != 0;
  }
#else
  (void)dirPrefixNoSlash;
  (void)baseName;
#endif
  return false;
}

void enqueueChildDirectory(DirQueueItem*& queue, size_t& queueCap, size_t& tail, const DirQueueItem& current,
                           const char* baseName, const char* relPrefix) {
  using namespace BmoSdInternal;
  if (current.depth >= kMaxScanDepth || isIgnoredDirName(baseName)) return;

  char childPath[SDCard::MAX_ROM_PATH_LEN + 2U];
  const int wrote = (relPrefix[0] == '\0') ? snprintf(childPath, sizeof(childPath), "/%s", baseName)
                                           : snprintf(childPath, sizeof(childPath), "/%s/%s", relPrefix, baseName);
  if (wrote <= 0 || static_cast<size_t>(wrote) >= sizeof(childPath)) {
    scanStats.pathTooLong++;
    skippedRomCount++;
    return;
  }

  constexpr size_t kMaxQueueCap = 512U;
  if (tail >= queueCap && queueCap < kMaxQueueCap) {
    const size_t nextCap = queueCap * 2U;
    DirQueueItem* grown =
        static_cast<DirQueueItem*>(heap_caps_realloc(queue, sizeof(DirQueueItem) * nextCap, MALLOC_CAP_SPIRAM));
    if (grown) {
      queue = grown;
      queueCap = nextCap;
    }
  }
  if (tail < queueCap) {
    strncpy(queue[tail].path, childPath, sizeof(queue[tail].path) - 1U);
    queue[tail].path[sizeof(queue[tail].path) - 1U] = '\0';
    queue[tail].depth = current.depth + 1;
    tail++;
  } else {
    scanStats.pathTooLong++;
    skippedRomCount++;
  }
}

void processScannedFileEntry(File& entry, const char* baseName, uint32_t fileSize, const char* relPrefix) {
  using namespace BmoSdInternal;
  const RomType type = ConsoleCatalog::determineTypeFromFilename(baseName);
  if (type == ROM_UNKNOWN) {
    entry.close();
    return;
  }

  uint8_t prefix[32];
  size_t prefixSize = 0U;
  uint8_t genesisHeader[4];
  size_t genesisHeaderSize = 0U;
  const char* dot = strrchr(baseName, '.');
  const bool readMdMagic = (dot && strcasecmp(dot, ".md") == 0 && fileSize >= 0x104U);
  const uint32_t crcPrefix = computePrefixCrc32FromFile(entry, BmoIndex::ROM_PREFIX_HASH_BYTES, prefix, &prefixSize,
                                                        readMdMagic, genesisHeader, &genesisHeaderSize);
  entry.close();

  if (fileSize > 0U && prefixSize == 0U) {
    scanStats.unreadable++;
    skippedRomCount++;
    return;
  }
  if (!RomFileFilter::isPlausibleFile(baseName, fileSize, prefix, prefixSize, genesisHeader, genesisHeaderSize)) {
    scanStats.filteredFormat++;
    skippedRomCount++;
    return;
  }
  if (numRoms >= SDCard::MAX_ROM_POOL ||
      (numRoms >= maxCapacity && (poolGrowthFailed || !ensurePoolCapacity(SDCard::computePoolCapacity(numRoms + 1))))) {
    poolGrowthFailed = true;
    scanStats.poolFull++;
    skippedRomCount++;
    return;
  }

  char relPath[SDCard::MAX_ROM_PATH_LEN + 1U];
  const int written = (relPrefix[0] == '\0') ? snprintf(relPath, sizeof(relPath), "%s", baseName)
                                             : snprintf(relPath, sizeof(relPath), "%s/%s", relPrefix, baseName);
  if (written <= 0 || static_cast<size_t>(written) > SDCard::MAX_ROM_PATH_LEN) {
    scanStats.pathTooLong++;
    skippedRomCount++;
    return;
  }

  const char* interned = internPath(relPath, static_cast<size_t>(written));
  if (!interned) {
    poolGrowthFailed = true;
    scanStats.poolFull++;
    skippedRomCount++;
    return;
  }

  romList[numRoms].filename = interned;
  romList[numRoms].type = type;
  romList[numRoms].isFavorite = 0U;
  romList[numRoms].hasBoxArt = 0U;
  romList[numRoms].isPopular = SDCard::isPopularGame(baseName, type) ? 1U : 0U;
  romList[numRoms].fileSize = fileSize;
  romList[numRoms].crc32Prefix = crcPrefix;
  if (type <= ROM_COLEM) romCountsByType[type]++;
  numRoms++;
}

void scanDirectoryChunk(File& dir, const DirQueueItem& current, const char* relPrefix, DirQueueItem*& queue,
                        size_t& queueCap, size_t& tail, bool& dirExhausted) {
  using namespace BmoSdInternal;
  SpiArbiter::ScopedLock spiLock;
  int chunkCount = 0;
  while (chunkCount < kScanChunkEntries) {
    File entry = dir.openNextFile();
    if (!entry) {
      dirExhausted = true;
      break;
    }
    chunkCount++;
    const char* rawName = entry.name();
    const char* slash = strrchr(rawName, '/');
    const char* baseName = slash ? (slash + 1) : rawName;
    const bool isDir = entry.isDirectory();
    const uint32_t fileSize = isDir ? 0U : static_cast<uint32_t>(entry.size());

    if (RomFileFilter::isIgnoredName(baseName)) {
      scanStats.ignoredNames++;
      entry.close();
      continue;
    }
    if (isFatHiddenOrSystem(relPrefix, baseName)) {
      scanStats.hiddenOrSystem++;
      entry.close();
      continue;
    }
    if (isDir) {
      enqueueChildDirectory(queue, queueCap, tail, current, baseName, relPrefix);
      entry.close();
      continue;
    }
    processScannedFileEntry(entry, baseName, fileSize, relPrefix);
  }
}

void scanCatalogTreeChunked(const char* rootDirPath, int startDepth, SDCard::ScanCancelFn shouldCancel,
                            SDCard::ScanProgressFn onProgress) {
  using namespace BmoSdInternal;
  if (!rootDirPath || startDepth > kMaxScanDepth) return;
  SpiArbiter::ScopedLock spiLock;

  constexpr size_t kInitialQueueCap = 64U;
  DirQueueItem* queue =
      static_cast<DirQueueItem*>(heap_caps_malloc(sizeof(DirQueueItem) * kInitialQueueCap, MALLOC_CAP_SPIRAM));
  if (!queue) {
    scanStats.poolFull++;
    skippedRomCount++;
    poolGrowthFailed = true;
    return;
  }
  size_t queueCap = kInitialQueueCap;
  size_t head = 0U;
  size_t tail = 1U;
  strncpy(queue[0].path, rootDirPath, sizeof(queue[0].path) - 1U);
  queue[0].path[sizeof(queue[0].path) - 1U] = '\0';
  queue[0].depth = startDepth;

  while (head < tail && !(shouldCancel && shouldCancel())) {
    const DirQueueItem current = queue[head++];
    if (current.depth > kMaxScanDepth) continue;
    const char* relPrefix = (current.path[0] == '/') ? (current.path + 1) : current.path;
    File dir = SD.open(current.path);
    if (!dir || !dir.isDirectory()) {
      if (dir) dir.close();
      continue;
    }
    bool dirExhausted = false;
    while (!dirExhausted) {
      if (shouldCancel && shouldCancel()) {
        scanStats.cancelled = true;
        break;
      }
      scanDirectoryChunk(dir, current, relPrefix, queue, queueCap, tail, dirExhausted);
      if (onProgress) onProgress(numRoms);
      if (!dirExhausted) yield();
    }
    dir.close();
    if (scanStats.cancelled) break;
    yield();
  }
  if (shouldCancel && shouldCancel()) scanStats.cancelled = true;
  heap_caps_free(queue);
}
#endif

} // namespace

#if FEATURE_SD_CARD
uint32_t computePrefixCrc32FromFile(File& file, size_t maxBytes, uint8_t* optPrefixOut, size_t* optPrefixLen,
                                    bool readGenesisMagic, uint8_t* optGenesisOut, size_t* optGenesisLen) {
  if (optPrefixLen) *optPrefixLen = 0;
  if (optGenesisLen) *optGenesisLen = 0;
  if (!file.seek(0)) return 0;

  uint8_t chunk[256];
  size_t totalRead = 0;
  uint32_t crc = 0xFFFFFFFFU;
  while (totalRead < maxBytes) {
    const size_t remain = maxBytes - totalRead;
    const size_t req = (remain < sizeof(chunk)) ? remain : sizeof(chunk);
    const int got = file.read(chunk, req);
    if (got <= 0 || static_cast<size_t>(got) > req) break;
    const size_t chunkLen = static_cast<size_t>(got);

    if (optPrefixOut && optPrefixLen && *optPrefixLen < 32U) {
      const size_t copyLen = (32U - *optPrefixLen < chunkLen) ? (32U - *optPrefixLen) : chunkLen;
      memcpy(optPrefixOut + *optPrefixLen, chunk, copyLen);
      *optPrefixLen += copyLen;
    }
    if (readGenesisMagic && optGenesisOut && optGenesisLen && totalRead <= 0x103U && totalRead + chunkLen > 0x100U) {
      for (size_t off = 0; off < chunkLen; ++off) {
        const size_t absOff = totalRead + off;
        if (absOff >= 0x100U && absOff < 0x104U) {
          optGenesisOut[absOff - 0x100U] = chunk[off];
          (*optGenesisLen)++;
        }
      }
    }

    crc = AtomicFs::crc32Update(chunk, chunkLen, crc);
    totalRead += chunkLen;
  }
  return (totalRead > 0U) ? (crc ^ 0xFFFFFFFFU) : 0U;
}
#endif

uint32_t computePrefixCrc32FromBuffer(const uint8_t* buf, size_t size) {
  if (!buf || size == 0U) return 0U;
  const size_t prefixLen = (size < BmoIndex::ROM_PREFIX_HASH_BYTES) ? size : BmoIndex::ROM_PREFIX_HASH_BYTES;
  return AtomicFs::crc32Update(buf, prefixLen, 0xFFFFFFFFU) ^ 0xFFFFFFFFU;
}

} // namespace BmoSdScanner

void SDCard::scanDirectory(const char* dirPath, int depth, bool hasArtDirs) {
#if FEATURE_SD_CARD
  (void)hasArtDirs;
  BmoSdScanner::scanCatalogTreeChunked(dirPath, depth, nullptr, nullptr);
#else
  (void)dirPath;
  (void)depth;
  (void)hasArtDirs;
#endif
}

void SDCard::scanRoms(ScanCancelFn shouldCancel, ScanProgressFn onProgress) {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  skippedRomCount = 0;
  scanStats = ScanStats{};
  poolGrowthFailed = false;
  bool loadedFromCache = false;
  bool hasArtDirs = false;
  {
    SpiArbiter::ScopedLock spiLock;
    BmoSdIndexIo::recoverIndexTransaction();
    if (loadIndex()) {
      loadedFromCache = true;
      hasArtDirs = SD.exists("/boxart") || SD.exists("/covers");
    }
  }
  if (!loadedFromCache) {
    BmoSdScanner::scanCatalogTreeChunked("/", 0, shouldCancel, onProgress);
  }
  if (loadedFromCache) {
    const int bakedCount = bakedRomCount();
    for (int i = 0; i < bakedCount && i < numRoms; ++i) {
      romList[i].hasBoxArt = hasArtDirs ? BoxArt::existsForRom(romList[i].filename) : 0U;
    }
    return;
  }

  sortRoms();
  bumpCatalogGeneration();
  BoxArt::populateCatalogAvailability(romList, numRoms);

  if (!scanStats.cancelled && skippedRomCount == 0) {
    SpiArbiter::ScopedLock spiLock;
    saveIndex();
  } else if (scanStats.cancelled) {
    LOG_WARN("SDCard: ROM scan cancelled at %d entries; library index not saved", numRoms);
  } else {
    LOG_WARN("SDCard: skipped %d ROM paths (filter=%lu, long=%lu, pool=%lu, io=%lu); library index not saved",
             skippedRomCount, static_cast<unsigned long>(scanStats.filteredFormat),
             static_cast<unsigned long>(scanStats.pathTooLong), static_cast<unsigned long>(scanStats.poolFull),
             static_cast<unsigned long>(scanStats.unreadable));
  }
  ensurePoolCapacity(computePoolCapacity(numRoms));
#else
  (void)shouldCancel;
  (void)onProgress;
#endif
}

void SDCard::rebuildIndex() {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  {
    SpiArbiter::ScopedLock spiLock;
    skippedRomCount = 0;
    scanStats = ScanStats{};
    poolGrowthFailed = false;
    if (SD.exists(BMO_INDEX_PATH)) SD.remove(BMO_INDEX_PATH);
    if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
    if (SD.exists(BMO_INDEX_BACKUP_PATH)) SD.remove(BMO_INDEX_BACKUP_PATH);

    const int bakedCount = bakedRomCount();
    numRoms = bakedCount;
    favoritesCount = 0;
    memset(romCountsByType, 0, sizeof(romCountsByType));
    resetPathArena();
    for (int i = 0; i < bakedCount; ++i) {
      if (romList[i].type <= ROM_COLEM) romCountsByType[romList[i].type]++;
      if (romList[i].isFavorite) favoritesCount++;
    }
    invalidateCatalogLookup();
  }
  scanRoms();
  loadFavorites();
#endif
}
