#include "sd_internal.h"
#include "../hal/config.h"
#include "../hal/spi_arbiter.h"
#include "../ui/console_catalog.h"
#include "atomic_fs.h"
#include "box_art.h"
#include "rom_file_filter.h"
#include "rom_index.h"
#include "rom_path_match.h"
#include "sd_atomic_ops.h"
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
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {

constexpr int kMaxScanDepth = 3;
constexpr int kScanChunkEntries = 16;

struct DirQueueItem {
  char path[SDCard::MAX_ROM_PATH_LEN + 2U];
  int depth;
};

int compareRoms(const void* a, const void* b) {
  const RomFile* ra = static_cast<const RomFile*>(a);
  const RomFile* rb = static_cast<const RomFile*>(b);
  const char* fa = ra->filename ? ra->filename : "";
  const char* fb = rb->filename ? rb->filename : "";
  const char* sa = strrchr(fa, '/');
  const char* sb = strrchr(fb, '/');
  sa = sa ? (sa + 1) : fa;
  sb = sb ? (sb + 1) : fb;
  const int cmp = strcasecmp(sa, sb);
  return (cmp != 0) ? cmp : strcasecmp(fa, fb);
}

bool isIgnoredDirName(const char* baseName) {
  if (!baseName || baseName[0] == '\0' || baseName[0] == '.') return true;
  if (strcasecmp(baseName, "saves") == 0 || strcasecmp(baseName, "screenshots") == 0 ||
      strcasecmp(baseName, "boxart") == 0 || strcasecmp(baseName, "covers") == 0 ||
      strcasecmp(baseName, "System Volume Information") == 0 || strcasecmp(baseName, "LOST.DIR") == 0) {
    return true;
  }
  return false;
}

#if FEATURE_SD_CARD
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
  // Allocate FILINFO in PSRAM? No: it is ~280 B and called at bounded leaf helper, well below 512 B threshold.
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

uint32_t rootDirectoryStamp() {
  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return 0;
  }
  const time_t stamp = root.getLastWrite();
  root.close();
  return stamp > 0 ? static_cast<uint32_t>(stamp) : 0;
}

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

uint32_t computePrefixCrc32FromBuffer(const uint8_t* buf, size_t size) {
  if (!buf || size == 0U) return 0U;
  const size_t prefixLen = (size < BmoIndex::ROM_PREFIX_HASH_BYTES) ? size : BmoIndex::ROM_PREFIX_HASH_BYTES;
  return AtomicFs::crc32Update(buf, prefixLen, 0xFFFFFFFFU) ^ 0xFFFFFFFFU;
}

bool verifyIndexV3File(const char* path, size_t expectedSize, uint32_t expectedCrc) {
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  if (file.size() != expectedSize) {
    file.close();
    return false;
  }
  BmoIndexHeader header;
  if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header) ||
      memcmp(header.magic, BMO_INDEX_MAGIC, sizeof(header.magic)) != 0 || header.version != BMO_INDEX_VERSION ||
      header.entrySize != sizeof(BmoIndexEntry) || header.crc32 != expectedCrc) {
    file.close();
    return false;
  }
  const size_t entriesBytes = static_cast<size_t>(header.entryCount) * sizeof(BmoIndexEntry);
  if (entriesBytes > expectedSize || header.stringTableBytes > expectedSize - sizeof(header) - entriesBytes) {
    file.close();
    return false;
  }
  size_t remaining = entriesBytes + static_cast<size_t>(header.stringTableBytes);
  uint8_t buffer[256];
  uint32_t crc = 0xFFFFFFFFU;
  while (remaining > 0U) {
    const size_t request = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    const int got = file.read(buffer, request);
    if (got <= 0 || static_cast<size_t>(got) > request) {
      file.close();
      return false;
    }
    crc = AtomicFs::crc32Update(buffer, static_cast<size_t>(got), crc);
    remaining -= static_cast<size_t>(got);
  }
  file.close();
  return (crc ^ 0xFFFFFFFFU) == expectedCrc;
}

bool indexFileIsValidV3(const char* path) {
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  const size_t fileSize = file.size();
  BmoIndexHeader header;
  const bool headerRead = file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) == sizeof(header);
  file.close();
  if (!headerRead || memcmp(header.magic, BMO_INDEX_MAGIC, sizeof(header.magic)) != 0 ||
      header.version != BMO_INDEX_VERSION || header.entrySize != sizeof(BmoIndexEntry) ||
      header.entryCount > (SIZE_MAX - sizeof(BmoIndexHeader)) / sizeof(BmoIndexEntry) ||
      header.stringTableBytes > BmoIndex::MAX_STRING_TABLE_BYTES) {
    return false;
  }
  const size_t entriesBytes = static_cast<size_t>(header.entryCount) * sizeof(BmoIndexEntry);
  if (entriesBytes + static_cast<size_t>(header.stringTableBytes) > SIZE_MAX - sizeof(BmoIndexHeader)) {
    return false;
  }
  const size_t expectedSize = sizeof(BmoIndexHeader) + entriesBytes + static_cast<size_t>(header.stringTableBytes);
  return fileSize == expectedSize && verifyIndexV3File(path, expectedSize, header.crc32);
}

void recoverIndexTransaction() {
  const bool hasIndex = SD.exists(BMO_INDEX_PATH);
  const bool hasBackup = SD.exists(BMO_INDEX_BACKUP_PATH);
  if (hasIndex && indexFileIsValidV3(BMO_INDEX_PATH)) {
    if (hasBackup) SD.remove(BMO_INDEX_BACKUP_PATH);
  } else if (hasBackup && indexFileIsValidV3(BMO_INDEX_BACKUP_PATH)) {
    if (hasIndex) SD.remove(BMO_INDEX_PATH);
    SD.rename(BMO_INDEX_BACKUP_PATH, BMO_INDEX_PATH);
  }
  if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
}

bool stampHostIndexInPlace(BmoIndexHeader& header, uint64_t totalFileBytes, size_t expectedFileSize) {
  const uint32_t stamp = rootDirectoryStamp();
  if (stamp == 0U) return false;
  header.timestamp = stamp;
  header.fingerprint =
      BmoIndex::catalogFingerprint(header.entryCount, header.stringTableBytes, totalFileBytes, stamp, header.crc32);
  File f = SD.open(BMO_INDEX_PATH, "r+");
  if (!f) return false;
  const bool ok = f.seek(0) && f.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header);
  f.flush();
  f.close();
  return ok && rootDirectoryStamp() == stamp && verifyIndexV3File(BMO_INDEX_PATH, expectedFileSize, header.crc32);
}
#endif

} // namespace

void SDCard::sortRoms() {
  using namespace BmoSdInternal;
  const int bakedCount = bakedRomCount();
  if (numRoms > bakedCount) {
    qsort(&romList[bakedCount], static_cast<size_t>(numRoms - bakedCount), sizeof(RomFile), compareRoms);
  }
}

bool SDCard::loadIndex() {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  if (!SD.exists(BMO_INDEX_PATH)) return false;

  File f = SD.open(BMO_INDEX_PATH, FILE_READ);
  if (!f) return false;

  const size_t actualFileSize = f.size();
  if (actualFileSize < sizeof(BmoIndexHeader)) {
    char magicPeek[8] = {};
    if (actualFileSize >= sizeof(magicPeek) && f.read(reinterpret_cast<uint8_t*>(magicPeek), sizeof(magicPeek)) == 8 &&
        memcmp(magicPeek, BMO_INDEX_LEGACY_MAGIC_V2, 8) == 0) {
      LOG_INFO_STR("SDCard: Legacy BMOIDX02 index found; migrating to BMOIDX03 via scan");
    }
    f.close();
    return false;
  }

  BmoIndexHeader header;
  if (f.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
    f.close();
    return false;
  }

  if (memcmp(header.magic, BMO_INDEX_LEGACY_MAGIC_V2, 8) == 0) {
    LOG_INFO_STR("SDCard: Legacy BMOIDX02 index found; upgrading to BMOIDX03 via scan");
    f.close();
    return false;
  }

  const int bakedCount = bakedRomCount();
  const uint32_t currentStamp = rootDirectoryStamp();
  const bool hostUnstampedIndex = (header.timestamp == 0U && currentStamp != 0U);
  if (memcmp(header.magic, BMO_INDEX_MAGIC, 8) != 0 || header.version != BMO_INDEX_VERSION ||
      header.entrySize != sizeof(BmoIndexEntry) || header.entryCount == 0U ||
      header.entryCount > static_cast<uint32_t>(MAX_ROM_POOL - bakedCount) || header.stringTableBytes == 0U ||
      header.stringTableBytes > BmoIndex::MAX_STRING_TABLE_BYTES ||
      (!hostUnstampedIndex && (header.timestamp == 0U || header.timestamp != currentStamp))) {
    f.close();
    return false;
  }

  const size_t entriesBytes = static_cast<size_t>(header.entryCount) * sizeof(BmoIndexEntry);
  const size_t stringBytes = static_cast<size_t>(header.stringTableBytes);
  if (entriesBytes / sizeof(BmoIndexEntry) != header.entryCount ||
      entriesBytes + stringBytes > SIZE_MAX - sizeof(BmoIndexHeader)) {
    f.close();
    return false;
  }
  const size_t expectedFileSize = sizeof(BmoIndexHeader) + entriesBytes + stringBytes;
  if (actualFileSize != expectedFileSize) {
    f.close();
    return false;
  }

  const int targetCap = computePoolCapacity(static_cast<int>(header.entryCount) + bakedCount);
  if (!ensurePoolCapacity(targetCap) || !reserveExactPathArena(stringBytes)) {
    f.close();
    return false;
  }

  BmoIndexEntry* entries = static_cast<BmoIndexEntry*>(heap_caps_malloc(entriesBytes, MALLOC_CAP_SPIRAM));
  if (!entries) {
    resetPathArena();
    f.close();
    return false;
  }

  const bool readEntriesOk = f.read(reinterpret_cast<uint8_t*>(entries), entriesBytes) == entriesBytes;
  char* arena = const_cast<char*>(arenaBase());
  const bool readStringsOk =
      readEntriesOk && arena && f.read(reinterpret_cast<uint8_t*>(arena), stringBytes) == stringBytes;
  f.close();

  if (!readEntriesOk || !readStringsOk) {
    heap_caps_free(entries);
    resetPathArena();
    return false;
  }

  uint32_t crc = AtomicFs::crc32Update(reinterpret_cast<const uint8_t*>(entries), entriesBytes, 0xFFFFFFFFU);
  crc = AtomicFs::crc32Update(reinterpret_cast<const uint8_t*>(arena), stringBytes, crc);
  crc ^= 0xFFFFFFFFU;
  if (crc != header.crc32) {
    heap_caps_free(entries);
    resetPathArena();
    return false;
  }

  uint64_t totalFileBytes = 0U;
  for (uint32_t i = 0; i < header.entryCount; ++i) {
    if (entries[i].type < ROM_GB || entries[i].type > ROM_COLEM ||
        !BmoIndex::validateEntryRecord(entries[i], header.stringTableBytes, arena)) {
      heap_caps_free(entries);
      resetPathArena();
      return false;
    }
    totalFileBytes += entries[i].fileSize;
  }

  if (static_cast<uint32_t>(totalFileBytes) != header.totalBytesLow ||
      static_cast<uint32_t>(totalFileBytes >> 32) != header.totalBytesHigh ||
      BmoIndex::catalogFingerprint(header.entryCount, header.stringTableBytes, totalFileBytes, header.timestamp,
                                   header.crc32) != header.fingerprint) {
    heap_caps_free(entries);
    resetPathArena();
    return false;
  }

  if (hostUnstampedIndex && !stampHostIndexInPlace(header, totalFileBytes, expectedFileSize)) {
    heap_caps_free(entries);
    resetPathArena();
    return false;
  }

  for (uint32_t i = 0; i < header.entryCount && numRoms < maxCapacity; ++i) {
    const BmoIndexEntry& e = entries[i];
    romList[numRoms].filename = arena + e.pathOffset;
    romList[numRoms].type = static_cast<RomType>(e.type);
    romList[numRoms].isFavorite = (e.flags & BmoIndex::FLAG_FAVORITE) ? 1U : 0U;
    if (romList[numRoms].isFavorite) {
      favoritesCount++;
    }
    romList[numRoms].hasBoxArt = (e.flags & BmoIndex::FLAG_BOX_ART) ? 1U : 0U;
    romList[numRoms].isPopular =
        (e.flags & BmoIndex::FLAG_POPULAR)
            ? 1U
            : (SDCard::isPopularGame(romList[numRoms].filename, romList[numRoms].type) ? 1U : 0U);
    romList[numRoms].fileSize = e.fileSize;
    romList[numRoms].crc32Prefix = e.crc32Prefix;
    if (romList[numRoms].type <= ROM_COLEM) {
      romCountsByType[romList[numRoms].type]++;
    }
    numRoms++;
  }

  heap_caps_free(entries);
  return true;
#else
  return false;
#endif
}

bool SDCard::saveIndex() {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  const int bakedCount = bakedRomCount();
  if (numRoms <= bakedCount) return false;

  const uint32_t entryCount = static_cast<uint32_t>(numRoms - bakedCount);
  size_t stringTableBytes = 0U;
  for (uint32_t i = 0; i < entryCount; ++i) {
    const char* name = romList[bakedCount + static_cast<int>(i)].filename;
    const size_t len = name ? strlen(name) : 0U;
    if (len == 0U || len > MAX_ROM_PATH_LEN) return false;
    stringTableBytes += len + 1U;
  }
  if (stringTableBytes == 0U || stringTableBytes > BmoIndex::MAX_STRING_TABLE_BYTES) return false;

  const size_t entriesBytes = static_cast<size_t>(entryCount) * sizeof(BmoIndexEntry);
  BmoIndexEntry* entries = static_cast<BmoIndexEntry*>(heap_caps_malloc(entriesBytes, MALLOC_CAP_SPIRAM));
  char* stringTable = static_cast<char*>(heap_caps_malloc(stringTableBytes, MALLOC_CAP_SPIRAM));
  if (!entries || !stringTable) {
    if (entries) heap_caps_free(entries);
    if (stringTable) heap_caps_free(stringTable);
    return false;
  }

  size_t strPos = 0U;
  uint64_t totalFileBytes = 0U;
  for (uint32_t i = 0; i < entryCount; ++i) {
    const RomFile& rf = romList[bakedCount + static_cast<int>(i)];
    const size_t len = strlen(rf.filename);
    memcpy(stringTable + strPos, rf.filename, len + 1U);

    entries[i].pathOffset = static_cast<uint32_t>(strPos);
    entries[i].pathLength = static_cast<uint16_t>(len);
    entries[i].type = static_cast<uint8_t>(rf.type);
    uint8_t flags = 0U;
    if (rf.isFavorite) flags |= BmoIndex::FLAG_FAVORITE;
    if (rf.hasBoxArt) flags |= BmoIndex::FLAG_BOX_ART;
    if (rf.isPopular) flags |= BmoIndex::FLAG_POPULAR;
    entries[i].flags = flags;
    entries[i].fileSize = rf.fileSize;
    entries[i].crc32Prefix = rf.crc32Prefix;
    entries[i].reserved = 0U;

    strPos += len + 1U;
    totalFileBytes += rf.fileSize;
  }

  const uint32_t rootStamp = rootDirectoryStamp();
  if (rootStamp == 0U) {
    heap_caps_free(entries);
    heap_caps_free(stringTable);
    LOG_WARN_STR("SDCard: root directory timestamp unavailable; ROM index cache not saved");
    return false;
  }

  uint32_t crc = AtomicFs::crc32Update(reinterpret_cast<const uint8_t*>(entries), entriesBytes, 0xFFFFFFFFU);
  crc = AtomicFs::crc32Update(reinterpret_cast<const uint8_t*>(stringTable), stringTableBytes, crc);
  crc ^= 0xFFFFFFFFU;

  BmoIndexHeader header;
  memset(&header, 0, sizeof(header));
  memcpy(header.magic, BMO_INDEX_MAGIC, 8);
  header.version = BMO_INDEX_VERSION;
  header.entryCount = entryCount;
  header.entrySize = sizeof(BmoIndexEntry);
  header.stringTableBytes = static_cast<uint32_t>(stringTableBytes);
  header.crc32 = crc;
  header.timestamp = 0U;
  header.totalBytesLow = static_cast<uint32_t>(totalFileBytes);
  header.totalBytesHigh = static_cast<uint32_t>(totalFileBytes >> 32);
  header.fingerprint =
      BmoIndex::catalogFingerprint(entryCount, header.stringTableBytes, totalFileBytes, rootStamp, crc);

  if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
  File f = SD.open(BMO_INDEX_TEMP_PATH, FILE_WRITE);
  if (!f) {
    heap_caps_free(entries);
    heap_caps_free(stringTable);
    return false;
  }

  bool ok = true;
  if (f.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) != sizeof(header)) ok = false;
  if (ok && f.write(reinterpret_cast<const uint8_t*>(entries), entriesBytes) != entriesBytes) ok = false;
  if (ok && f.write(reinterpret_cast<const uint8_t*>(stringTable), stringTableBytes) != stringTableBytes) ok = false;
  f.flush();
  f.close();

  const size_t expectedFileSize = sizeof(BmoIndexHeader) + entriesBytes + stringTableBytes;
  ok = ok && verifyIndexV3File(BMO_INDEX_TEMP_PATH, expectedFileSize, header.crc32);

  bool hadBackup = false;
  if (ok) {
    if (SD.exists(BMO_INDEX_BACKUP_PATH)) SD.remove(BMO_INDEX_BACKUP_PATH);
    if (SD.exists(BMO_INDEX_PATH)) {
      hadBackup = SD.rename(BMO_INDEX_PATH, BMO_INDEX_BACKUP_PATH);
      ok = hadBackup;
    }
  }
  if (ok) {
    ok = SD.rename(BMO_INDEX_TEMP_PATH, BMO_INDEX_PATH);
    if (!ok && hadBackup) SD.rename(BMO_INDEX_BACKUP_PATH, BMO_INDEX_PATH);
  }

  auto syncFinalTimestamp = [&]() -> bool {
    header.timestamp = rootDirectoryStamp();
    header.fingerprint =
        BmoIndex::catalogFingerprint(entryCount, header.stringTableBytes, totalFileBytes, header.timestamp, crc);
    bool synced = false;
    File finalFile = SD.open(BMO_INDEX_PATH, "r+");
    if (finalFile && header.timestamp != 0U && finalFile.seek(0) &&
        finalFile.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header)) {
      synced = true;
    }
    if (finalFile) {
      finalFile.flush();
      finalFile.close();
    }
    return synced && verifyIndexV3File(BMO_INDEX_PATH, expectedFileSize, header.crc32);
  };

  if (ok) {
    ok = syncFinalTimestamp();
  }
  if (ok) {
    if (hadBackup) SD.remove(BMO_INDEX_BACKUP_PATH);
    ok = syncFinalTimestamp();
  } else {
    if (SD.exists(BMO_INDEX_PATH)) SD.remove(BMO_INDEX_PATH);
    if (hadBackup && SD.exists(BMO_INDEX_BACKUP_PATH)) {
      SD.rename(BMO_INDEX_BACKUP_PATH, BMO_INDEX_PATH);
    }
    if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
  }

  heap_caps_free(entries);
  heap_caps_free(stringTable);
  return ok;
#else
  return false;
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
      if (romList[i].type <= ROM_COLEM) {
        romCountsByType[romList[i].type]++;
      }
      if (romList[i].isFavorite) {
        favoritesCount++;
      }
    }
  }
  scanRoms();
  loadFavorites();
#endif
}

#if FEATURE_SD_CARD
static void scanCatalogTreeChunked(const char* rootDirPath, int startDepth, SDCard::ScanCancelFn shouldCancel,
                                   SDCard::ScanProgressFn onProgress) {
  using namespace BmoSdInternal;
  if (!rootDirPath || startDepth > kMaxScanDepth) return;

  constexpr size_t kInitialQueueCap = 64U;
  constexpr size_t kMaxQueueCap = 512U;
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
  size_t tail = 0U;

  strncpy(queue[tail].path, rootDirPath, sizeof(queue[tail].path) - 1U);
  queue[tail].path[sizeof(queue[tail].path) - 1U] = '\0';
  queue[tail].depth = startDepth;
  tail++;

  while (head < tail) {
    if (shouldCancel && shouldCancel()) {
      scanStats.cancelled = true;
      break;
    }

    const DirQueueItem current = queue[head++];
    if (current.depth > kMaxScanDepth) continue;
    const char* relPrefix = (current.path[0] == '/') ? (current.path + 1) : current.path;

    File dir;
    {
      SpiArbiter::ScopedLock spiLock;
      dir = SD.open(current.path);
    }
    if (!dir || !dir.isDirectory()) {
      if (dir) {
        SpiArbiter::ScopedLock spiLock;
        dir.close();
      }
      continue;
    }

    bool dirExhausted = false;
    while (!dirExhausted) {
      if (shouldCancel && shouldCancel()) {
        scanStats.cancelled = true;
        break;
      }

      {
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
            if (current.depth < kMaxScanDepth && !isIgnoredDirName(baseName)) {
              char childPath[SDCard::MAX_ROM_PATH_LEN + 2U];
              const int wrote = (relPrefix[0] == '\0')
                                    ? snprintf(childPath, sizeof(childPath), "/%s", baseName)
                                    : snprintf(childPath, sizeof(childPath), "/%s/%s", relPrefix, baseName);
              if (wrote > 0 && static_cast<size_t>(wrote) < sizeof(childPath)) {
                if (tail >= queueCap && queueCap < kMaxQueueCap) {
                  const size_t nextCap = queueCap * 2U;
                  DirQueueItem* grown = static_cast<DirQueueItem*>(
                      heap_caps_realloc(queue, sizeof(DirQueueItem) * nextCap, MALLOC_CAP_SPIRAM));
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
              } else {
                scanStats.pathTooLong++;
                skippedRomCount++;
              }
            }
            entry.close();
            continue;
          }

          const RomType type = ConsoleCatalog::determineTypeFromFilename(baseName);
          if (type == ROM_UNKNOWN) {
            entry.close();
            continue;
          }

          uint8_t prefix[32];
          size_t prefixSize = 0U;
          uint8_t genesisHeader[4];
          size_t genesisHeaderSize = 0U;
          const char* dot = strrchr(baseName, '.');
          const bool readMdMagic = (dot && strcasecmp(dot, ".md") == 0 && fileSize >= 0x104U);
          const uint32_t crcPrefix =
              computePrefixCrc32FromFile(entry, BmoIndex::ROM_PREFIX_HASH_BYTES, prefix, &prefixSize, readMdMagic,
                                         genesisHeader, &genesisHeaderSize);
          entry.close();

          if (fileSize > 0U && prefixSize == 0U) {
            scanStats.unreadable++;
            skippedRomCount++;
            continue;
          }

          if (!RomFileFilter::isPlausibleFile(baseName, fileSize, prefix, prefixSize, genesisHeader,
                                              genesisHeaderSize)) {
            scanStats.filteredFormat++;
            skippedRomCount++;
            continue;
          }

          if (numRoms >= SDCard::MAX_ROM_POOL ||
              (numRoms >= maxCapacity &&
               (poolGrowthFailed || !ensurePoolCapacity(SDCard::computePoolCapacity(numRoms + 1))))) {
            poolGrowthFailed = true;
            scanStats.poolFull++;
            skippedRomCount++;
            continue;
          }

          char relPath[SDCard::MAX_ROM_PATH_LEN + 1U];
          const int written = (relPrefix[0] == '\0') ? snprintf(relPath, sizeof(relPath), "%s", baseName)
                                                     : snprintf(relPath, sizeof(relPath), "%s/%s", relPrefix, baseName);
          if (written <= 0 || static_cast<size_t>(written) > SDCard::MAX_ROM_PATH_LEN) {
            scanStats.pathTooLong++;
            skippedRomCount++;
            continue;
          }

          const char* interned = internPath(relPath, static_cast<size_t>(written));
          if (!interned) {
            poolGrowthFailed = true;
            scanStats.poolFull++;
            skippedRomCount++;
            continue;
          }

          romList[numRoms].filename = interned;
          romList[numRoms].type = type;
          romList[numRoms].isFavorite = 0U;
          romList[numRoms].hasBoxArt = 0U;
          romList[numRoms].isPopular = SDCard::isPopularGame(baseName, type) ? 1U : 0U;
          romList[numRoms].fileSize = fileSize;
          romList[numRoms].crc32Prefix = crcPrefix;
          if (type <= ROM_COLEM) {
            romCountsByType[type]++;
          }
          numRoms++;
        }
      }

      if (onProgress) {
        onProgress(numRoms);
      }
      if (!dirExhausted) {
        yield();
      }
    }

    {
      SpiArbiter::ScopedLock spiLock;
      dir.close();
    }
    if (scanStats.cancelled) {
      break;
    }
    yield();
  }

  heap_caps_free(queue);
}
#endif

void SDCard::scanDirectory(const char* dirPath, int depth, bool hasArtDirs) {
#if FEATURE_SD_CARD
  (void)hasArtDirs;
  scanCatalogTreeChunked(dirPath, depth, nullptr, nullptr);
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
    recoverIndexTransaction();
    if (loadIndex()) {
      loadedFromCache = true;
      hasArtDirs = SD.exists("/boxart") || SD.exists("/covers");
    }
  }
  if (!loadedFromCache) {
    scanCatalogTreeChunked("/", 0, shouldCancel, onProgress);
  }
  if (loadedFromCache) {
    const int bakedCount = bakedRomCount();
    for (int i = 0; i < bakedCount && i < numRoms; ++i) {
      romList[i].hasBoxArt = hasArtDirs ? BoxArt::existsForRom(romList[i].filename) : 0U;
    }
    return;
  }

  sortRoms();
  BoxArt::populateCatalogAvailability(romList, numRoms);

  if (!scanStats.cancelled && skippedRomCount == 0) {
    SpiArbiter::ScopedLock spiLock;
    saveIndex();
  } else if (scanStats.cancelled) {
    LOG_WARN("SDCard: ROM scan cancelled at %d entries; library index not saved", numRoms);
  } else if (skippedRomCount != 0) {
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

int SDCard::getRomCount() {
  return BmoSdInternal::numRoms;
}

int SDCard::getRomCountForType(RomType type) {
  if (type == ROM_FAVORITES) {
    return getFavoritesCount();
  }
  if (type <= ROM_COLEM) return BmoSdInternal::romCountsByType[type];
  return 0;
}

const RomFile* SDCard::findRomByPath(const char* relativePath) {
  using namespace BmoSdInternal;
  if (!relativePath || !relativePath[0]) return nullptr;
  for (int i = 0; i < numRoms; ++i) {
    if (romList[i].filename && RomPathMatch::equalsNormalized(romList[i].filename, relativePath)) {
      return &romList[i];
    }
  }
  return nullptr;
}

uint32_t SDCard::findRomFileSize(const char* relativePath) {
  const RomFile* rf = findRomByPath(relativePath);
  return rf ? rf->fileSize : 0U;
}

bool SDCard::verifyRomIdentity(const RomFile* rom, const uint8_t* loadedBuffer, size_t loadedSize) {
  if (!rom || !rom->filename || rom->filename[0] == '\0') return false;
  if (loadedBuffer && BmoSdInternal::isBakedRomPointer(loadedBuffer)) {
    return true;
  }
  if (loadedBuffer != nullptr) {
    if (rom->fileSize != 0U && loadedSize != rom->fileSize) return false;
    if (rom->crc32Prefix != 0U) {
#if FEATURE_SD_CARD
      const uint32_t actualPrefix = computePrefixCrc32FromBuffer(loadedBuffer, loadedSize);
#else
      const size_t prefixLen =
          (loadedSize < BmoIndex::ROM_PREFIX_HASH_BYTES) ? loadedSize : BmoIndex::ROM_PREFIX_HASH_BYTES;
      const uint32_t actualPrefix = AtomicFs::crc32Update(loadedBuffer, prefixLen, 0xFFFFFFFFU) ^ 0xFFFFFFFFU;
#endif
      if (actualPrefix != rom->crc32Prefix) return false;
    }
    return true;
  }
#if FEATURE_SD_CARD
  if (!BmoSdInternal::mounted) return false;
  char path[SDCard::MAX_ROM_PATH_LEN + 2U];
  const char* clean = RomPathMatch::normalizeRelative(rom->filename);
  const int wrote = snprintf(path, sizeof(path), "/%s", clean);
  if (wrote <= 1 || static_cast<size_t>(wrote) >= sizeof(path)) return false;
  SpiArbiter::ScopedLock spiLock;
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return false;
  }
  const uint32_t actualSize = static_cast<uint32_t>(f.size());
  if (actualSize == 0U || (rom->fileSize != 0U && actualSize != rom->fileSize)) {
    f.close();
    return false;
  }
  bool prefixOk = true;
  if (rom->crc32Prefix != 0U) {
    const uint32_t actualPrefix =
        computePrefixCrc32FromFile(f, BmoIndex::ROM_PREFIX_HASH_BYTES, nullptr, nullptr, false, nullptr, nullptr);
    prefixOk = (actualPrefix == rom->crc32Prefix);
  }
  f.close();
  return prefixOk;
#else
  return true;
#endif
}

bool SDCard::isPopularGame(const char* filename, RomType type) {
  if (!filename || !filename[0]) return false;
  const char* base = strrchr(filename, '/');
  if (!base) base = strrchr(filename, '\\');
  base = base ? (base + 1) : filename;

  char lower[64];
  size_t i = 0;
  for (; base[i] != '\0' && i + 1 < sizeof(lower); ++i) {
    char c = base[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    lower[i] = c;
  }
  lower[i] = '\0';

  auto matchAny = [&](const char* const* keywords, int n) -> bool {
    for (int k = 0; k < n; ++k) {
      if (strstr(lower, keywords[k]) != nullptr) return true;
    }
    return false;
  };

  static const char* const kGbTop[] = {"virtual bmo", "tetris",        "super mario", "zelda",       "link's awakening",
                                       "pokemon",     "kirby",         "metroid",     "donkey kong", "gargoyle",
                                       "castlevania", "final fantasy", "wario land",  "mega man",    "contra",
                                       "ducktales",   "dr. mario",     "spider-man"};
  static const char* const kGbcTop[] = {"mario",      "zelda",         "oracle",        "pokemon",
                                        "metal gear", "shantae",       "wario land",    "dragon warrior",
                                        "aladdin",    "lego racers",   "kirby",         "grand theft auto",
                                        "tetris",     "resident evil", "survival kids", "lufia"};
  static const char* const kNesTop[] = {"super mario", "zelda",         "metroid",   "mega man",      "castlevania",
                                        "contra",      "kirby",         "punch-out", "final fantasy", "dragon warrior",
                                        "tetris",      "ninja gaiden",  "ducktales", "river city",    "double dragon",
                                        "excitebike",  "blaster master"};
  static const char* const kSnesTop[] = {
      "chrono trigger", "super mario", "zelda",          "link to the past", "super metroid", "donkey kong",
      "final fantasy",  "earthbound",  "secret of mana", "mega man x",       "contra",        "castlevania",
      "f-zero",         "star fox",    "street fighter", "actraiser",        "terranigma"};
  static const char* const kGenesisTop[] = {
      "sonic",         "streets of rage", "gunstar",       "shinobi",       "phantasy star", "shining force",
      "castlevania",   "contra",          "golden axe",    "altered beast", "comix zone",    "earthworm jim",
      "mortal kombat", "aladdin",         "thunder force", "vectorman",     "ristar"};
  static const char* const kSmsGgTop[] = {
      "alex kidd",          "phantasy star", "wonder boy", "sonic",  "shinobi", "castle of illusion",
      "master of darkness", "power strike",  "golden axe", "r-type", "outrun",  "columns",
      "defender of oasis",  "tails",         "gunstar",    "ristar"};
  static const char* const kPceTop[] = {"bonk",          "blazing lazers", "rondo",        "castlevania",
                                        "soldier blade", "star soldier",   "bomberman",    "splatterhouse",
                                        "ys ",           "air zonk",       "neutopia",     "military madness",
                                        "parasol stars", "r-type",         "devil's crush"};
  static const char* const kAtariTop[] = {"pitfall",        "river raid",      "adventure", "yar",     "kaboom",
                                          "space invaders", "missile command", "frogger",   "pac-man", "pacman",
                                          "asteroids",      "centipede",       "berzerk",   "h.e.r.o"};
  static const char* const kColemTop[] = {"donkey kong", "zaxxon",    "venture",      "lady bug", "mouse trap",
                                          "smurf",       "pepper",    "frogger",      "galaxian", "gorf",
                                          "antarctic",   "montezuma", "boulder dash", "q*bert"};
  static const char* const kNgpTop[] = {"snk vs",     "card fighters",    "millennium",      "metal slug",
                                        "sonic",      "king of fighters", "samurai shodown", "fatal fury",
                                        "last blade", "dark arms",        "faselei"};
  static const char* const kLynxTop[] = {
      "california games", "blue lightning", "chip's challenge", "chips challenge", "klax", "rygar",   "stun runner",
      "s.t.u.n",          "xenophobe",      "rampart",          "gauntlet",        "toki", "lemmings"};
  static const char* const kWSwanTop[] = {"final fantasy", "klonoa",      "gunpey",    "silversword",
                                          "dicing knight", "guilty gear", "rockman",   "mega man",
                                          "digimon",       "chocobo",     "makaimura", "golden axe"};
  static const char* const kPicoTop[] = {"celeste", "high stakes", "slipways", "picohot",  "jelpi",        "combo pool",
                                         "poom",    "dusk child",  "bmo",      "cab ride", "just one boss"};
  static const char* const kDoomTop[] = {"doom", "freedoom", "plutonia", "tnt", "heretic", "chex"};

  switch (type) {
  case ROM_GB:
    return matchAny(kGbTop, sizeof(kGbTop) / sizeof(kGbTop[0]));
  case ROM_GBC:
    return matchAny(kGbcTop, sizeof(kGbcTop) / sizeof(kGbcTop[0]));
  case ROM_NES:
    return matchAny(kNesTop, sizeof(kNesTop) / sizeof(kNesTop[0]));
  case ROM_SNES:
    return matchAny(kSnesTop, sizeof(kSnesTop) / sizeof(kSnesTop[0]));
  case ROM_GENESIS:
    return matchAny(kGenesisTop, sizeof(kGenesisTop) / sizeof(kGenesisTop[0]));
  case ROM_SMS:
  case ROM_GG:
    return matchAny(kSmsGgTop, sizeof(kSmsGgTop) / sizeof(kSmsGgTop[0]));
  case ROM_PCE:
    return matchAny(kPceTop, sizeof(kPceTop) / sizeof(kPceTop[0]));
  case ROM_ATARI:
    return matchAny(kAtariTop, sizeof(kAtariTop) / sizeof(kAtariTop[0]));
  case ROM_COLEM:
    return matchAny(kColemTop, sizeof(kColemTop) / sizeof(kColemTop[0]));
  case ROM_NGP:
    return matchAny(kNgpTop, sizeof(kNgpTop) / sizeof(kNgpTop[0]));
  case ROM_LYNX:
    return matchAny(kLynxTop, sizeof(kLynxTop) / sizeof(kLynxTop[0]));
  case ROM_WSWAN:
    return matchAny(kWSwanTop, sizeof(kWSwanTop) / sizeof(kWSwanTop[0]));
  case ROM_PICO8:
    return matchAny(kPicoTop, sizeof(kPicoTop) / sizeof(kPicoTop[0]));
  case ROM_WAD:
    return matchAny(kDoomTop, sizeof(kDoomTop) / sizeof(kDoomTop[0]));
  default:
    return false;
  }
}

int SDCard::getPopularCountForType(RomType type) {
  using namespace BmoSdInternal;
  int count = 0;
  for (int i = 0; i < numRoms; ++i) {
    if (romList[i].type == type && romList[i].isPopular) {
      count++;
    }
  }
  return count;
}

const RomFile* SDCard::getRomInfo(int index) {
  using namespace BmoSdInternal;
  if (index < 0 || index >= numRoms) return nullptr;
  return &romList[index];
}
