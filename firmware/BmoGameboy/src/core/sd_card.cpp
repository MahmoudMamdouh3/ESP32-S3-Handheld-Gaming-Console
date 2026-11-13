#include "sd_card.h"
#include "config.h"
#include "spi_arbiter.h"
#include "rom_index.h"
#include "rom_file_filter.h"
#include "save_manager.h"
#include "box_art.h"
#if FEATURE_SD_CARD
#include <SD.h>
#include <SPI.h>
#include <esp32-hal-spi.h>
#endif
#include <esp_heap_caps.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#if defined(__has_include)
#if __has_include("../assets/roms/virtual_bmo.h")
#include "../assets/roms/virtual_bmo.h"
#define BMO_HAS_VIRTUAL_BMO_ROM 1
#endif
#endif
#ifndef BMO_HAS_VIRTUAL_BMO_ROM
#define BMO_HAS_VIRTUAL_BMO_ROM 0
#endif
#include "../assets/roms/mario_deluxe.h"
#include "../assets/roms/zelda_ages.h"
// Baked GBC ROMs (1MB each; tracked in repository under src/assets/roms/)
#include "../assets/roms/aladdin.h"
#include "../assets/roms/lego_racers.h"

namespace {
  static_assert(sizeof(RomFile) == sizeof(BmoIndexEntry), "RomFile and BmoIndexEntry size parity");
  static_assert(offsetof(RomFile, fileSize) == offsetof(BmoIndexEntry, fileSize), "ROM index file-size offset parity");

  bool mounted = false;
  uint32_t mountedClockHz = 0;
  static const int MAX_ROMS = SDCard::MAX_ROM_POOL;
  static const int BAKED_ROM_COUNT = 4 + BMO_HAS_VIRTUAL_BMO_ROM;
  static RomFile fallbackRomList[32];
  static RomFile* romList = fallbackRomList;
  static int maxCapacity = 32;
  int numRoms = 0;
  int skippedRomCount = 0;
  bool poolGrowthFailed = false;
  static int s_favoritesCount = 0;                  // PERF-C1: O(1) counter — maintained by begin/toggleFavorite/loadFavorites
  static int romCountsByType[ROM_COLEM + 1] = {0};

  bool ensurePoolCapacity(int targetCapacity) {
    if (targetCapacity < SDCard::MIN_ROM_POOL) targetCapacity = SDCard::MIN_ROM_POOL;
    if (targetCapacity > SDCard::MAX_ROM_POOL) targetCapacity = SDCard::MAX_ROM_POOL;
    if (romList == fallbackRomList) {
      RomFile* psramList = (RomFile*)heap_caps_malloc(sizeof(RomFile) * (size_t)targetCapacity, MALLOC_CAP_SPIRAM);
      if (!psramList) return false;
      if (numRoms > 0) {
        int copyCount = (numRoms < targetCapacity) ? numRoms : targetCapacity;
        memcpy(psramList, fallbackRomList, sizeof(RomFile) * (size_t)copyCount);
      }
      romList = psramList;
      maxCapacity = targetCapacity;
      return true;
    }
    if (targetCapacity == maxCapacity) return true;
    RomFile* resized = (RomFile*)heap_caps_realloc(romList, sizeof(RomFile) * (size_t)targetCapacity, MALLOC_CAP_SPIRAM);
    if (!resized) return (maxCapacity >= targetCapacity);
    romList = resized;
    maxCapacity = targetCapacity;
    if (numRoms > maxCapacity) numRoms = maxCapacity;
    return true;
  }

  RomType determineType(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return ROM_UNKNOWN;
    if (strcasecmp(ext, ".gb") == 0) return ROM_GB;
    if (strcasecmp(ext, ".gbc") == 0) return ROM_GBC;
    if (strcasecmp(ext, ".nes") == 0) return ROM_NES;
    if (strcasecmp(ext, ".wad") == 0) return ROM_WAD;
    if (strcasecmp(ext, ".sms") == 0) return ROM_SMS;
    if (strcasecmp(ext, ".gg") == 0) return ROM_GG;
    if (strcasecmp(ext, ".pce") == 0) return ROM_PCE;
    if (strcasecmp(ext, ".a26") == 0) return ROM_ATARI;
    if (strcasecmp(ext, ".p8") == 0) return ROM_PICO8;
    if (strcasecmp(ext, ".gen") == 0 || strcasecmp(ext, ".md") == 0 || strcasecmp(ext, ".smd") == 0) return ROM_GENESIS;
    if (strcasecmp(ext, ".sfc") == 0 || strcasecmp(ext, ".smc") == 0) return ROM_SNES;
    if (strcasecmp(ext, ".ws") == 0 || strcasecmp(ext, ".wsc") == 0) return ROM_WSWAN;
    if (strcasecmp(ext, ".ngp") == 0 || strcasecmp(ext, ".ngc") == 0) return ROM_NGP;
    if (strcasecmp(ext, ".lnx") == 0) return ROM_LYNX;
    if (strcasecmp(ext, ".col") == 0 || strcasecmp(ext, ".sg") == 0) return ROM_COLEM;
    return ROM_UNKNOWN;
  }

  int compareRoms(const void* a, const void* b) {
    const RomFile* ra = (const RomFile*)a;
    const RomFile* rb = (const RomFile*)b;
    const char* sa = strrchr(ra->filename, '/');
    const char* sb = strrchr(rb->filename, '/');
    sa = sa ? (sa + 1) : ra->filename;
    sb = sb ? (sb + 1) : rb->filename;
    int cmp = strcasecmp(sa, sb);
    return (cmp != 0) ? cmp : strcasecmp(ra->filename, rb->filename);
  }

  bool isIgnoredDirName(const char* baseName) {
    if (!baseName || baseName[0] == '\0' || baseName[0] == '.') return true;
    if (strcasecmp(baseName, "saves") == 0 ||
        strcasecmp(baseName, "screenshots") == 0 ||
        strcasecmp(baseName, "boxart") == 0 ||
        strcasecmp(baseName, "covers") == 0 ||
        strcasecmp(baseName, "System Volume Information") == 0 ||
        strcasecmp(baseName, "LOST.DIR") == 0) {
      return true;
    }
    return false;
  }

  uint32_t rootDirectoryStamp() {
#if FEATURE_SD_CARD
    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
      if (root) root.close();
      return 0;
    }
    const time_t stamp = root.getLastWrite();
    root.close();
    return stamp > 0 ? (uint32_t)stamp : 0;
#else
    return 0;
#endif
  }

  bool verifyIndexFile(const char* path, size_t expectedSize, uint32_t expectedCrc) {
#if FEATURE_SD_CARD
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    if (file.size() != expectedSize) {
      file.close();
      return false;
    }
    BmoIndexHeader header;
    if (file.read((uint8_t*)&header, sizeof(header)) != sizeof(header) ||
        strncmp(header.magic, BMO_INDEX_MAGIC, sizeof(header.magic)) != 0 ||
        header.version != BMO_INDEX_VERSION || header.entrySize != sizeof(BmoIndexEntry) ||
        header.crc32 != expectedCrc) {
      file.close();
      return false;
    }
    uint8_t buffer[512];
    size_t remaining = (size_t)header.entryCount * sizeof(BmoIndexEntry);
    uint32_t crc = 0xFFFFFFFFU;
    while (remaining > 0) {
      const size_t request = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
      const int got = file.read(buffer, request);
      if (got <= 0 || (size_t)got > request) {
        file.close();
        return false;
      }
      crc = SaveManager::computeCRC32(buffer, (size_t)got, crc);
      remaining -= (size_t)got;
    }
    file.close();
    return crc == expectedCrc;
#else
    (void)path;
    (void)expectedSize;
    (void)expectedCrc;
    return false;
#endif
  }

  bool indexFileIsValid(const char* path) {
#if FEATURE_SD_CARD
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    const size_t fileSize = file.size();
    BmoIndexHeader header;
    const bool headerRead = file.read((uint8_t*)&header, sizeof(header)) == sizeof(header);
    file.close();
    if (!headerRead || strncmp(header.magic, BMO_INDEX_MAGIC, sizeof(header.magic)) != 0 ||
        header.version != BMO_INDEX_VERSION || header.entrySize != sizeof(BmoIndexEntry) ||
        header.entryCount > (SIZE_MAX - sizeof(BmoIndexHeader)) / sizeof(BmoIndexEntry)) return false;
    const size_t expectedSize = sizeof(BmoIndexHeader) +
        (size_t)header.entryCount * sizeof(BmoIndexEntry);
    return fileSize == expectedSize && verifyIndexFile(path, expectedSize, header.crc32);
#else
    (void)path;
    return false;
#endif
  }

  void recoverIndexTransaction() {
#if FEATURE_SD_CARD
    const bool hasIndex = SD.exists(BMO_INDEX_PATH);
    const bool hasBackup = SD.exists(BMO_INDEX_BACKUP_PATH);
    if (hasIndex && indexFileIsValid(BMO_INDEX_PATH)) {
      if (hasBackup) SD.remove(BMO_INDEX_BACKUP_PATH);
    } else if (hasBackup && indexFileIsValid(BMO_INDEX_BACKUP_PATH)) {
      if (hasIndex) SD.remove(BMO_INDEX_PATH);
      SD.rename(BMO_INDEX_BACKUP_PATH, BMO_INDEX_PATH);
    }
    if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
#endif
  }

#if FEATURE_SD_CARD
  bool makeMetadataPath(const char* finalPath, const char* suffix,
                        char* out, size_t outSize) {
    if (!finalPath || !suffix || !out || outSize == 0) return false;
    const int written = snprintf(out, outSize, "%s%s", finalPath, suffix);
    return written > 0 && (size_t)written < outSize;
  }

  bool recoverMetadataFile(const char* finalPath) {
    char temporaryPath[64];
    char backupPath[64];
    if (!makeMetadataPath(finalPath, ".tmp", temporaryPath, sizeof(temporaryPath)) ||
        !makeMetadataPath(finalPath, ".bak", backupPath, sizeof(backupPath))) return false;
    if (SD.exists(finalPath)) {
      if (SD.exists(backupPath) && !SD.remove(backupPath)) return false;
      if (SD.exists(temporaryPath) && !SD.remove(temporaryPath)) return false;
      return true;
    }
    if (SD.exists(backupPath) && !SD.rename(backupPath, finalPath)) return false;
    if (SD.exists(temporaryPath) && !SD.remove(temporaryPath)) return false;
    return true;
  }

  bool verifyMetadataFile(const char* path, size_t expectedSize, uint32_t expectedCrc) {
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    if (file.size() != expectedSize) {
      file.close();
      return false;
    }
    uint8_t buffer[256];
    size_t remaining = expectedSize;
    uint32_t crc = 0xFFFFFFFFU;
    while (remaining > 0) {
      const size_t request = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
      const int readCount = file.read(buffer, request);
      if (readCount <= 0 || (size_t)readCount > request) {
        file.close();
        return false;
      }
      crc = SaveManager::computeCRC32(buffer, (size_t)readCount, crc);
      remaining -= (size_t)readCount;
    }
    file.close();
    return (crc ^ 0xFFFFFFFFU) == expectedCrc;
  }

  bool commitMetadataFile(const char* temporaryPath, const char* finalPath,
                          size_t expectedSize, uint32_t expectedCrc) {
    char backupPath[64];
    if (!makeMetadataPath(finalPath, ".bak", backupPath, sizeof(backupPath)) ||
        !verifyMetadataFile(temporaryPath, expectedSize, expectedCrc)) return false;
    const bool hadFinal = SD.exists(finalPath);
    if (hadFinal && !SD.rename(finalPath, backupPath)) return false;
    if (!SD.rename(temporaryPath, finalPath)) {
      if (hadFinal) SD.rename(backupPath, finalPath);
      return false;
    }
    if (!verifyMetadataFile(finalPath, expectedSize, expectedCrc)) {
      SD.remove(finalPath);
      if (hadFinal) SD.rename(backupPath, finalPath);
      return false;
    }
    if (hadFinal && SD.exists(backupPath) && !SD.remove(backupPath)) {
      LOG_WARN("SDCard: Verified metadata; deferred backup cleanup for %s", finalPath);
    }
    return true;
  }

  bool writeMetadataLine(File& file, const char* line, size_t& byteCount,
                         uint32_t& crc) {
    if (!line) return false;
    const size_t length = strlen(line);
    if (file.write((const uint8_t*)line, length) != length) return false;
    static const uint8_t lineEnding[] = {'\r', '\n'};
    if (file.write(lineEnding, sizeof(lineEnding)) != sizeof(lineEnding)) return false;
    crc = SaveManager::computeCRC32((const uint8_t*)line, length, crc);
    crc = SaveManager::computeCRC32(lineEnding, sizeof(lineEnding), crc);
    byteCount += length + sizeof(lineEnding);
    return true;
  }
#endif
}

int SDCard::getPoolCapacity() {
  return maxCapacity;
}

int SDCard::getSkippedRomCount() {
  return skippedRomCount;
}

bool SDCard::begin() {
  numRoms = 0;
  skippedRomCount = 0;
  poolGrowthFailed = false;
  s_favoritesCount = 0;
  memset(romCountsByType, 0, sizeof(romCountsByType));

  ensurePoolCapacity(MIN_ROM_POOL);

  // Keep the optional local Virtual BMO ROM first when it is available.
#if BMO_HAS_VIRTUAL_BMO_ROM
  strncpy(romList[numRoms].filename, "Virtual BMO (Official Game).gb", 63);
  romList[numRoms].filename[63] = '\0';
  romList[numRoms].type = ROM_GB;
  romList[numRoms].isFavorite = true;
  romList[numRoms].hasBoxArt = false;
  romList[numRoms].isPopular = true;
  romList[numRoms].fileSize = 0;
  s_favoritesCount++;                  // PERF-C1
  romCountsByType[ROM_GB]++;
  numRoms++;
#endif

  // Baked classics
  strncpy(romList[numRoms].filename, "Super Mario Bros Deluxe (Baked).gbc", 63);
  romList[numRoms].filename[63] = '\0';
  romList[numRoms].type = ROM_GBC;
  romList[numRoms].isFavorite = true;
  romList[numRoms].hasBoxArt = false;
  romList[numRoms].isPopular = true;
  romList[numRoms].fileSize = 0;
  s_favoritesCount++;                  // PERF-C1
  romCountsByType[ROM_GBC]++;
  numRoms++;

  strncpy(romList[numRoms].filename, "Legend of Zelda Ages (Baked).gbc", 63);
  romList[numRoms].filename[63] = '\0';
  romList[numRoms].type = ROM_GBC;
  romList[numRoms].isFavorite = true;
  romList[numRoms].hasBoxArt = false;
  romList[numRoms].isPopular = true;
  romList[numRoms].fileSize = 0;
  s_favoritesCount++;                  // PERF-C1
  romCountsByType[ROM_GBC]++;
  numRoms++;

  strncpy(romList[numRoms].filename, "Aladdin (Baked).gbc", 63);
  romList[numRoms].filename[63] = '\0';
  romList[numRoms].type = ROM_GBC;
  romList[numRoms].isFavorite = false;
  romList[numRoms].hasBoxArt = false;
  romList[numRoms].isPopular = true;
  romList[numRoms].fileSize = 0;
  romCountsByType[ROM_GBC]++;
  numRoms++;

  strncpy(romList[numRoms].filename, "Lego Racers (Baked).gbc", 63);
  romList[numRoms].filename[63] = '\0';
  romList[numRoms].type = ROM_GBC;
  romList[numRoms].isFavorite = false;
  romList[numRoms].hasBoxArt = false;
  romList[numRoms].isPopular = true;
  romList[numRoms].fileSize = 0;
  romCountsByType[ROM_GBC]++;
  numRoms++;

#if FEATURE_SD_CARD
  SpiArbiter::lock();
  const uint32_t candidatesHz[] = {SD_SPI_HZ, 20000000UL, 10000000UL, 4000000UL};
  uint32_t successfulRequestHz = 0;
  for (size_t candidateIndex = 0;
       candidateIndex < sizeof(candidatesHz) / sizeof(candidatesHz[0]);
       ++candidateIndex) {
    const uint32_t candidateHz = candidatesHz[candidateIndex];
    if (candidateHz == 0 || candidateHz > SD_SPI_HZ) continue;

    bool duplicate = false;
    for (size_t earlier = 0; earlier < candidateIndex; ++earlier) {
      if (candidatesHz[earlier] == candidateHz) duplicate = true;
    }
    if (duplicate) continue;

    if (SD.begin(SD_CS, SPI, candidateHz, "/sd")) {
      successfulRequestHz = candidateHz;
      break;
    }
    LOG_WARN("SD mount failed at %lu Hz; trying a lower SPI clock when available",
             (unsigned long)candidateHz);
    SD.end();
  }

  if (successfulRequestHz == 0) {
    mounted = false;
    mountedClockHz = 0;
    SpiArbiter::unlock();
    // Do not reset numRoms to 0, because we have baked ROMs!
    return false;
  }
  mounted = true;
  const uint32_t cappedRequestHz = successfulRequestHz > 25000000UL ? 25000000UL : successfulRequestHz;
  mountedClockHz = spiClockDivToFrequency(nullptr,
                                           spiFrequencyToClockDiv(nullptr, cappedRequestHz));
  LOG_INFO("SD mounted with nominal SPI clock %lu Hz (request %lu Hz)",
           (unsigned long)mountedClockHz, (unsigned long)successfulRequestHz);
  SpiArbiter::unlock();
  scanRoms();
  loadFavorites();
  loadRecentHistory();
#else
  mounted = false;
#endif
  return true;
}

bool SDCard::isMounted() {
  return mounted;
}

uint32_t SDCard::getClockHz() {
  return mounted ? mountedClockHz : 0;
}

void SDCard::sortRoms() {
  if (numRoms > BAKED_ROM_COUNT) {
    qsort(&romList[BAKED_ROM_COUNT], numRoms - BAKED_ROM_COUNT, sizeof(RomFile), compareRoms);
  }
}

bool SDCard::loadIndex() {
#if FEATURE_SD_CARD
  if (!SD.exists(BMO_INDEX_PATH)) return false;

  File f = SD.open(BMO_INDEX_PATH, FILE_READ);
  if (!f) return false;

  BmoIndexHeader header;
  if (f.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
    f.close();
    return false;
  }

  if (strncmp(header.magic, BMO_INDEX_MAGIC, 8) != 0 ||
      header.version != BMO_INDEX_VERSION ||
      header.entrySize != sizeof(BmoIndexEntry) ||
      header.entryCount == 0 ||
      header.entryCount > (uint32_t)(MAX_ROM_POOL - BAKED_ROM_COUNT) ||
      header.timestamp == 0 || header.timestamp != rootDirectoryStamp()) {
    f.close();
    return false;
  }

  int targetCap = computePoolCapacity((int)header.entryCount + BAKED_ROM_COUNT);
  if (!ensurePoolCapacity(targetCap)) {
    f.close();
    return false;
  }

  size_t expectedFileSize = sizeof(BmoIndexHeader) + header.entryCount * sizeof(BmoIndexEntry);
  if (f.size() != expectedFileSize) {
    f.close();
    return false;
  }

  size_t entriesBytes = header.entryCount * sizeof(BmoIndexEntry);
  BmoIndexEntry* entries = (BmoIndexEntry*)heap_caps_malloc(entriesBytes, MALLOC_CAP_SPIRAM);
  if (!entries) {
    f.close();
    return false;
  }

  if (f.read((uint8_t*)entries, entriesBytes) != entriesBytes) {
    heap_caps_free(entries);
    f.close();
    return false;
  }
  f.close();

  uint32_t computedCrc = SaveManager::computeCRC32((const uint8_t*)entries, entriesBytes);
  if (computedCrc != header.crc32) {
    heap_caps_free(entries);
    return false;
  }

  uint64_t totalFileBytes = 0;
  for (uint32_t i = 0; i < header.entryCount; ++i) {
    if (entries[i].type < ROM_GB || entries[i].type > ROM_COLEM ||
        entries[i].isFavorite > 1 || entries[i].hasBoxArt > 1 || entries[i].reserved != 0 ||
        entries[i].filename[0] == '\0' ||
        memchr(entries[i].filename, '\0', sizeof(entries[i].filename)) == nullptr) {
      heap_caps_free(entries);
      return false;
    }
    totalFileBytes += entries[i].fileSize;
  }
  if ((uint32_t)totalFileBytes != header.reserved[0] ||
      BmoIndex::catalogFingerprint(header.entryCount, totalFileBytes, header.timestamp, header.crc32) != header.reserved[1]) {
    heap_caps_free(entries);
    return false;
  }

  for (uint32_t i = 0; i < header.entryCount && numRoms < maxCapacity; ++i) {
    const BmoIndexEntry& e = entries[i];
    strncpy(romList[numRoms].filename, e.filename, 63);
    romList[numRoms].filename[63] = '\0';
    romList[numRoms].type = (RomType)e.type;
    romList[numRoms].isFavorite = (e.isFavorite != 0);
    if (romList[numRoms].isFavorite) {
      s_favoritesCount++;
    }
    romList[numRoms].hasBoxArt = (e.hasBoxArt != 0);
    romList[numRoms].isPopular = SDCard::isPopularGame(romList[numRoms].filename, romList[numRoms].type);
    romList[numRoms].fileSize = e.fileSize;
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
  if (numRoms <= BAKED_ROM_COUNT) return false;

  uint32_t entryCount = numRoms - BAKED_ROM_COUNT;
  size_t entriesBytes = entryCount * sizeof(BmoIndexEntry);
  BmoIndexEntry* entries = (BmoIndexEntry*)heap_caps_malloc(entriesBytes, MALLOC_CAP_SPIRAM);
  if (!entries) return false;

  for (uint32_t i = 0; i < entryCount; ++i) {
    const RomFile& rf = romList[BAKED_ROM_COUNT + i];
    strncpy(entries[i].filename, rf.filename, 63);
    entries[i].filename[63] = '\0';
    entries[i].type = (uint8_t)rf.type;
    entries[i].isFavorite = rf.isFavorite ? 1 : 0;
    entries[i].hasBoxArt = rf.hasBoxArt ? 1 : 0;
    entries[i].reserved = 0;
    entries[i].fileSize = rf.fileSize;
  }

  uint64_t totalFileBytes = 0;
  for (uint32_t i = 0; i < entryCount; ++i) totalFileBytes += entries[i].fileSize;
  const uint32_t rootStamp = rootDirectoryStamp();
  if (rootStamp == 0) {
    heap_caps_free(entries);
    LOG_WARN("SDCard: root directory timestamp unavailable; ROM index cache not saved");
    return false;
  }

  BmoIndexHeader header;
  memset(&header, 0, sizeof(header));
  memcpy(header.magic, BMO_INDEX_MAGIC, 8);
  header.version = BMO_INDEX_VERSION;
  header.entryCount = entryCount;
  header.entrySize = sizeof(BmoIndexEntry);
  header.crc32 = SaveManager::computeCRC32((const uint8_t*)entries, entriesBytes);
  header.timestamp = 0;
  header.reserved[0] = (uint32_t)totalFileBytes;
  header.reserved[1] = BmoIndex::catalogFingerprint(entryCount, totalFileBytes, rootStamp, header.crc32);

  if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
  File f = SD.open(BMO_INDEX_TEMP_PATH, FILE_WRITE);
  if (!f) {
    heap_caps_free(entries);
    return false;
  }

  bool ok = true;
  if (f.write((const uint8_t*)&header, sizeof(header)) != sizeof(header)) ok = false;
  if (ok && f.write((const uint8_t*)entries, entriesBytes) != entriesBytes) ok = false;
  f.flush();
  f.close();

  const size_t expectedFileSize = sizeof(BmoIndexHeader) + entriesBytes;
  ok = ok && verifyIndexFile(BMO_INDEX_TEMP_PATH, expectedFileSize, header.crc32);

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

  if (ok) {
    // Renaming the new directory entry changes the root timestamp. Store the
    // post-rename value so the cache validates on the next boot.
    header.timestamp = rootDirectoryStamp();
    header.reserved[1] = BmoIndex::catalogFingerprint(entryCount, totalFileBytes,
                                                       header.timestamp, header.crc32);
    File finalFile = SD.open(BMO_INDEX_PATH, "r+");
    if (!finalFile || header.timestamp == 0 || !finalFile.seek(0) ||
        finalFile.write((const uint8_t*)&header, sizeof(header)) != sizeof(header)) {
      ok = false;
    }
    if (finalFile) {
      finalFile.flush();
      finalFile.close();
    }
    if (ok) ok = verifyIndexFile(BMO_INDEX_PATH, expectedFileSize, header.crc32);
  }

  if (ok) {
    if (hadBackup) SD.remove(BMO_INDEX_BACKUP_PATH);
    header.timestamp = rootDirectoryStamp();
    header.reserved[1] = BmoIndex::catalogFingerprint(entryCount, totalFileBytes,
                                                       header.timestamp, header.crc32);
    File finalFile = SD.open(BMO_INDEX_PATH, "r+");
    if (!finalFile || header.timestamp == 0 || !finalFile.seek(0) ||
        finalFile.write((const uint8_t*)&header, sizeof(header)) != sizeof(header)) {
      ok = false;
    }
    if (finalFile) {
      finalFile.flush();
      finalFile.close();
    }
    if (ok) ok = verifyIndexFile(BMO_INDEX_PATH, expectedFileSize, header.crc32);
  } else {
    if (SD.exists(BMO_INDEX_PATH)) SD.remove(BMO_INDEX_PATH);
    if (hadBackup && SD.exists(BMO_INDEX_BACKUP_PATH)) {
      SD.rename(BMO_INDEX_BACKUP_PATH, BMO_INDEX_PATH);
    }
    if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
  }

  heap_caps_free(entries);
  return ok;
#else
  return false;
#endif
}

void SDCard::rebuildIndex() {
#if FEATURE_SD_CARD
  SpiArbiter::lock();
  skippedRomCount = 0;
  poolGrowthFailed = false;
  if (SD.exists(BMO_INDEX_PATH)) SD.remove(BMO_INDEX_PATH);
  if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
  if (SD.exists(BMO_INDEX_BACKUP_PATH)) SD.remove(BMO_INDEX_BACKUP_PATH);

  numRoms = BAKED_ROM_COUNT;
  s_favoritesCount = 0;
  memset(romCountsByType, 0, sizeof(romCountsByType));
  for (int i = 0; i < BAKED_ROM_COUNT; ++i) {
    if (romList[i].type <= ROM_COLEM) {
      romCountsByType[romList[i].type]++;
    }
    if (romList[i].isFavorite) {
      s_favoritesCount++;
    }
  }

  SpiArbiter::unlock();
  scanRoms();
  loadFavorites();
#endif
}

void SDCard::scanDirectory(const char* dirPath, int depth, bool hasArtDirs) {
#if FEATURE_SD_CARD
  if (!dirPath || depth > 3) return;

  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }

  const char* relPrefix = (dirPath[0] == '/') ? (dirPath + 1) : dirPath;

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    const char* rawName = entry.name();
    const char* slash = strrchr(rawName, '/');
    const char* baseName = slash ? (slash + 1) : rawName;
    const bool isDir = entry.isDirectory();
    const uint32_t fileSize = isDir ? 0 : (uint32_t)entry.size();

    if (RomFileFilter::isIgnoredName(baseName)) {
      entry.close();
      continue;
    }

    if (isDir) {
      if (depth < 3 && !isIgnoredDirName(baseName)) {
        char childPath[128];
        if (relPrefix[0] == '\0') {
          snprintf(childPath, sizeof(childPath), "/%s", baseName);
        } else {
          snprintf(childPath, sizeof(childPath), "/%s/%s", relPrefix, baseName);
        }
        entry.close();
        scanDirectory(childPath, depth + 1, hasArtDirs);
      } else {
        entry.close();
      }
      continue;
    }

    RomType type = determineType(baseName);
    if (type != ROM_UNKNOWN) {
      uint8_t prefix[32];
      size_t prefixSize = 0;
      uint8_t genesisHeader[4];
      size_t genesisHeaderSize = 0;
      if (RomFileFilter::needsSample(baseName)) {
        if (entry.seek(0)) prefixSize = entry.read(prefix, sizeof(prefix));
        if (strcasecmp(strrchr(baseName, '.'), ".md") == 0 &&
            fileSize >= 0x104 && entry.seek(0x100)) {
          genesisHeaderSize = entry.read(genesisHeader, sizeof(genesisHeader));
        }
      }
      if (!RomFileFilter::isPlausibleFile(baseName, fileSize, prefix, prefixSize,
                                          genesisHeader, genesisHeaderSize)) {
        entry.close();
        skippedRomCount++;
        continue;
      }
      entry.close();

      if (numRoms >= MAX_ROM_POOL ||
          (numRoms >= maxCapacity &&
           (poolGrowthFailed || !ensurePoolCapacity(computePoolCapacity(numRoms + 1))))) {
        poolGrowthFailed = true;
        skippedRomCount++;
        continue;
      }

      char relPath[64];
      int written = 0;
      if (relPrefix[0] == '\0') {
        written = snprintf(relPath, sizeof(relPath), "%s", baseName);
      } else {
        written = snprintf(relPath, sizeof(relPath), "%s/%s", relPrefix, baseName);
      }
      if (written > 0 && written < (int)sizeof(relPath)) {
        strncpy(romList[numRoms].filename, relPath, 63);
        romList[numRoms].filename[63] = '\0';
        romList[numRoms].type = type;
        romList[numRoms].isFavorite = false;
        romList[numRoms].hasBoxArt = false;
        romList[numRoms].isPopular = SDCard::isPopularGame(baseName, type);
        romList[numRoms].fileSize = fileSize;
        if (type <= ROM_COLEM) {
          romCountsByType[type]++;
        }
        numRoms++;
      } else {
        skippedRomCount++;
      }
    } else {
      entry.close();
    }
  }
  dir.close();
  (void)hasArtDirs;
#else
  (void)dirPath;
  (void)depth;
  (void)hasArtDirs;
#endif
}

void SDCard::scanRoms() {
#if FEATURE_SD_CARD
  skippedRomCount = 0;
  poolGrowthFailed = false;
  SpiArbiter::lock();

  // Try fast-loading cached index first (<15ms vs 3500ms directory crawl)
  recoverIndexTransaction();
  if (loadIndex()) {
    const bool hasArtDirs = SD.exists("/boxart") || SD.exists("/covers");
    for (int i = 0; i < BAKED_ROM_COUNT && i < numRoms; ++i) {
      romList[i].hasBoxArt = hasArtDirs ? BoxArt::existsForRom(romList[i].filename) : false;
    }
    SpiArbiter::unlock();
    return;
  }

  // Grow the PSRAM pool as the cold scan finds ROMs; small libraries do not
  // reserve the 36,000-entry maximum up front.
  scanDirectory("/", 0, false);
  SpiArbiter::unlock();

  // Sort newly crawled ROMs alphabetically
  sortRoms();

  // Compute hasBoxArt across the complete catalog in O((N + M) log N) so legacy
  // stem collision detection sees every ROM across all directories.
  BoxArt::populateCatalogAvailability(romList, numRoms);

  // Do not cache an incomplete library. The next boot will retry the scan.
  if (skippedRomCount == 0) {
    SpiArbiter::lock();
    saveIndex();
    SpiArbiter::unlock();
  }
  if (skippedRomCount != 0) {
    LOG_WARN("SDCard: skipped %d ROM paths; library index not saved", skippedRomCount);
  }

  ensurePoolCapacity(computePoolCapacity(numRoms));

#endif
}

int SDCard::getRomCount() {
  return numRoms;
}

int SDCard::getRomCountForType(RomType type) {
  if (type == ROM_FAVORITES) {
    return getFavoritesCount();
  }
  if (type <= ROM_COLEM) return romCountsByType[type];
  return 0;
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
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    lower[i] = c;
  }
  lower[i] = '\0';

  auto matchAny = [&](const char* const* keywords, int n) -> bool {
    for (int k = 0; k < n; ++k) {
      if (strstr(lower, keywords[k]) != nullptr) return true;
    }
    return false;
  };

  static const char* const kGbTop[] = {
    "virtual bmo", "tetris", "super mario", "zelda", "link's awakening",
    "pokemon", "kirby", "metroid", "donkey kong", "gargoyle", "castlevania",
    "final fantasy", "wario land", "mega man", "contra", "ducktales", "dr. mario", "spider-man"
  };
  static const char* const kGbcTop[] = {
    "mario", "zelda", "oracle", "pokemon", "metal gear", "shantae",
    "wario land", "dragon warrior", "aladdin", "lego racers", "kirby",
    "grand theft auto", "tetris", "resident evil", "survival kids", "lufia"
  };
  static const char* const kNesTop[] = {
    "super mario", "zelda", "metroid", "mega man", "castlevania", "contra",
    "kirby", "punch-out", "final fantasy", "dragon warrior", "tetris",
    "ninja gaiden", "ducktales", "river city", "double dragon", "excitebike", "blaster master"
  };
  static const char* const kSnesTop[] = {
    "chrono trigger", "super mario", "zelda", "link to the past", "super metroid",
    "donkey kong", "final fantasy", "earthbound", "secret of mana", "mega man x",
    "contra", "castlevania", "f-zero", "star fox", "street fighter", "actraiser", "terranigma"
  };
  static const char* const kGenesisTop[] = {
    "sonic", "streets of rage", "gunstar", "shinobi", "phantasy star",
    "shining force", "castlevania", "contra", "golden axe", "altered beast",
    "comix zone", "earthworm jim", "mortal kombat", "aladdin", "thunder force", "vectorman", "ristar"
  };
  static const char* const kSmsGgTop[] = {
    "alex kidd", "phantasy star", "wonder boy", "sonic", "shinobi",
    "castle of illusion", "master of darkness", "power strike", "golden axe",
    "r-type", "outrun", "columns", "defender of oasis", "tails", "gunstar", "ristar"
  };
  static const char* const kPceTop[] = {
    "bonk", "blazing lazers", "rondo", "castlevania", "soldier blade",
    "star soldier", "bomberman", "splatterhouse", "ys ", "air zonk",
    "neutopia", "military madness", "parasol stars", "r-type", "devil's crush"
  };
  static const char* const kAtariTop[] = {
    "pitfall", "river raid", "adventure", "yar", "kaboom", "space invaders",
    "missile command", "frogger", "pac-man", "pacman", "asteroids", "centipede", "berzerk", "h.e.r.o"
  };
  static const char* const kColemTop[] = {
    "donkey kong", "zaxxon", "venture", "lady bug", "mouse trap", "smurf",
    "pepper", "frogger", "galaxian", "gorf", "antarctic", "montezuma", "boulder dash", "q*bert"
  };
  static const char* const kNgpTop[] = {
    "snk vs", "card fighters", "millennium", "metal slug", "sonic",
    "king of fighters", "samurai shodown", "fatal fury", "last blade", "dark arms", "faselei"
  };
  static const char* const kLynxTop[] = {
    "california games", "blue lightning", "chip's challenge", "chips challenge",
    "klax", "rygar", "stun runner", "s.t.u.n", "xenophobe", "rampart", "gauntlet", "toki", "lemmings"
  };
  static const char* const kWSwanTop[] = {
    "final fantasy", "klonoa", "gunpey", "silversword", "dicing knight",
    "guilty gear", "rockman", "mega man", "digimon", "chocobo", "makaimura", "golden axe"
  };
  static const char* const kPicoTop[] = {
    "celeste", "high stakes", "slipways", "picohot", "jelpi", "combo pool",
    "poom", "dusk child", "bmo", "cab ride", "just one boss"
  };
  static const char* const kDoomTop[] = {
    "doom", "freedoom", "plutonia", "tnt", "heretic", "chex"
  };

  switch (type) {
    case ROM_GB:      return matchAny(kGbTop, sizeof(kGbTop) / sizeof(kGbTop[0]));
    case ROM_GBC:     return matchAny(kGbcTop, sizeof(kGbcTop) / sizeof(kGbcTop[0]));
    case ROM_NES:     return matchAny(kNesTop, sizeof(kNesTop) / sizeof(kNesTop[0]));
    case ROM_SNES:    return matchAny(kSnesTop, sizeof(kSnesTop) / sizeof(kSnesTop[0]));
    case ROM_GENESIS: return matchAny(kGenesisTop, sizeof(kGenesisTop) / sizeof(kGenesisTop[0]));
    case ROM_SMS:
    case ROM_GG:      return matchAny(kSmsGgTop, sizeof(kSmsGgTop) / sizeof(kSmsGgTop[0]));
    case ROM_PCE:     return matchAny(kPceTop, sizeof(kPceTop) / sizeof(kPceTop[0]));
    case ROM_ATARI:   return matchAny(kAtariTop, sizeof(kAtariTop) / sizeof(kAtariTop[0]));
    case ROM_COLEM:   return matchAny(kColemTop, sizeof(kColemTop) / sizeof(kColemTop[0]));
    case ROM_NGP:     return matchAny(kNgpTop, sizeof(kNgpTop) / sizeof(kNgpTop[0]));
    case ROM_LYNX:    return matchAny(kLynxTop, sizeof(kLynxTop) / sizeof(kLynxTop[0]));
    case ROM_WSWAN:   return matchAny(kWSwanTop, sizeof(kWSwanTop) / sizeof(kWSwanTop[0]));
    case ROM_PICO8:   return matchAny(kPicoTop, sizeof(kPicoTop) / sizeof(kPicoTop[0]));
    case ROM_WAD:     return matchAny(kDoomTop, sizeof(kDoomTop) / sizeof(kDoomTop[0]));
    default:          return false;
  }
}

int SDCard::getPopularCountForType(RomType type) {
  int count = 0;
  for (int i = 0; i < numRoms; ++i) {
    if (romList[i].type == type && romList[i].isPopular) {
      count++;
    }
  }
  return count;
}

const RomFile* SDCard::getRomInfo(int index) {
  if (index < 0 || index >= numRoms) return nullptr;
  return &romList[index];
}

bool SDCard::isFavorite(int index) {
  if (index < 0 || index >= numRoms) return false;
  return romList[index].isFavorite;
}

bool SDCard::isFavorite(const char* filename) {
  if (!filename) return false;
  for (int i = 0; i < numRoms; ++i) {
    if (strcmp(romList[i].filename, filename) == 0) {
      return romList[i].isFavorite;
    }
  }
  return false;
}

void SDCard::toggleFavorite(int index) {
  if (index < 0 || index >= numRoms) return;
  // PERF-C1: Maintain O(1) counter instead of letting getFavoritesCount() re-scan.
  if (romList[index].isFavorite) {
    romList[index].isFavorite = false;
    s_favoritesCount--;
  } else {
    romList[index].isFavorite = true;
    s_favoritesCount++;
  }
  saveFavorites();
}

namespace {
  static constexpr int MAX_RECENT_HISTORY = 10;
  static char s_recentHistory[MAX_RECENT_HISTORY][64];
  static int s_recentCount = 0;
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
  int limit = (existing >= 0) ? existing : ((s_recentCount < MAX_RECENT_HISTORY) ? s_recentCount++ : (MAX_RECENT_HISTORY - 1));
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
  if (!mounted) return;
  SpiArbiter::lock();
  const char* finalPath = "/history.txt";
  char temporaryPath[64];
  bool ok = recoverMetadataFile(finalPath) &&
      makeMetadataPath(finalPath, ".tmp", temporaryPath, sizeof(temporaryPath));
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
    ok = ok && commitMetadataFile(temporaryPath, finalPath, byteCount,
                                  crc ^ 0xFFFFFFFFU);
  } else {
    ok = false;
  }
  SpiArbiter::unlock();
  if (!ok) LOG_WARN_STR("SDCard: Could not commit recent-history metadata");
#endif
}

void SDCard::loadRecentHistory() {
  s_recentCount = 0;
#if FEATURE_SD_CARD
  if (!mounted) return;
  SpiArbiter::lock();
  if (!recoverMetadataFile("/history.txt")) {
    SpiArbiter::unlock();
    return;
  }
  if (!SD.exists("/history.txt")) {
    SpiArbiter::unlock();
    return;
  }
  File f = SD.open("/history.txt", FILE_READ);
  if (!f) {
    SpiArbiter::unlock();
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
  SpiArbiter::unlock();
#endif
}

int SDCard::getFavoritesCount() {
  // PERF-C1: O(1) — counter maintained by begin/toggleFavorite/loadFavorites.
  return s_favoritesCount;
}

void SDCard::saveFavorites() {
#if FEATURE_SD_CARD
  if (!mounted) return;
  SpiArbiter::lock();
  const char* finalPath = "/favorites.txt";
  char temporaryPath[64];
  bool ok = recoverMetadataFile(finalPath) &&
      makeMetadataPath(finalPath, ".tmp", temporaryPath, sizeof(temporaryPath));
  if (ok && SD.exists(temporaryPath)) ok = SD.remove(temporaryPath);
  File f = ok ? SD.open(temporaryPath, FILE_WRITE) : File();
  if (!f) {
    SpiArbiter::unlock();
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
  ok = ok && commitMetadataFile(temporaryPath, finalPath, byteCount,
                                crc ^ 0xFFFFFFFFU);
  SpiArbiter::unlock();
  if (!ok) LOG_WARN_STR("SDCard: Could not commit favorite metadata");
#endif
}

void SDCard::loadFavorites() {
#if FEATURE_SD_CARD
  if (!mounted) return;
  SpiArbiter::lock();
  if (!recoverMetadataFile("/favorites.txt")) {
    SpiArbiter::unlock();
    return;
  }
  if (!SD.exists("/favorites.txt")) {
    SpiArbiter::unlock();
    return;
  }
  File f = SD.open("/favorites.txt", FILE_READ);
  if (!f) {
    SpiArbiter::unlock();
    return;
  }
  // Authoritative override when /favorites.txt exists on SD card.
  s_favoritesCount = 0;
  for (int i = 0; i < numRoms; ++i) {
    romList[i].isFavorite = false;
  }
  // H-3 / L-5: Stack char[] instead of Arduino String — no heap alloc per line.
  char line[64];
  while (f.available()) {
    int len = f.readBytesUntil('\n', line, (int)sizeof(line) - 1);
    line[len] = '\0';
    // Trim trailing \r and spaces to handle Windows \r\n line endings.
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ')) {
      line[--len] = '\0';
    }
    if (len > 0) {
      for (int i = 0; i < numRoms; ++i) {
        if (strcmp(romList[i].filename, line) == 0) {
          if (!romList[i].isFavorite) {
            romList[i].isFavorite = true;
            s_favoritesCount++;  // PERF-C1: maintain O(1) counter
          }
          break;
        }
      }
    }
  }
  f.close();
  SpiArbiter::unlock();
#endif
}

uint8_t* SDCard::loadRom(const char* filename, size_t* outSize) {
  if (!filename || !outSize) return nullptr;
  *outSize = 0;

  // Check for baked ROMs first
#if BMO_HAS_VIRTUAL_BMO_ROM
  if (strcmp(filename, "Virtual BMO (Official Game).gb") == 0) {
    *outSize = virtual_bmo_rom_size;
    return (uint8_t*)virtual_bmo_rom;
  }
#endif
  if (strcmp(filename, "Super Mario Bros Deluxe (Baked).gbc") == 0) {
    *outSize = mario_deluxe_rom_size;
    return (uint8_t*)mario_deluxe_rom;
  }
  if (strcmp(filename, "Legend of Zelda Ages (Baked).gbc") == 0) {
    *outSize = zelda_ages_rom_size;
    return (uint8_t*)zelda_ages_rom;
  }
  if (strcmp(filename, "Aladdin (Baked).gbc") == 0) {
    *outSize = aladdin_rom_size;
    return (uint8_t*)aladdin_rom;
  }
  if (strcmp(filename, "Lego Racers (Baked).gbc") == 0) {
    *outSize = lego_racers_rom_size;
    return (uint8_t*)lego_racers_rom;
  }

#if FEATURE_SD_CARD
  // H-3: Stack buffer instead of Arduino String — avoids DRAM heap alloc on every ROM launch.
  char path[128];
  snprintf(path, sizeof(path), "/%s", filename);
  SpiArbiter::lock();
  File file = SD.open(path, FILE_READ);
  if (!file) {
    SpiArbiter::unlock();
    return nullptr;
  }

  size_t size = file.size();
  if (size == 0) {
    file.close();
    SpiArbiter::unlock();
    return nullptr;
  }
  *outSize = size;

  // Allocate strictly in PSRAM (external SPI RAM) since ROMs are up to 4MB
  uint8_t* buffer = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  if (!buffer) {
    file.close();
    SpiArbiter::unlock();
    return nullptr;
  }

  // Read entire file in multi-sector burst chunks (PERF-17: 64KB chunk bursts)
  size_t bytesRead = 0;
  while (bytesRead < size) {
    size_t toRead = (size - bytesRead > 65536) ? 65536 : (size - bytesRead);
    int chunk = file.read(buffer + bytesRead, toRead);
    if (chunk <= 0) break; // EOF or error
    bytesRead += chunk;
  }
  
  file.close();
  SpiArbiter::unlock();
  if (bytesRead != size) {
    // Never hand a truncated ROM to an emulator: it can fail much later with
    // a misleading crash or an out-of-bounds bank read.
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
    // DO NOT free baked ROM pointers residing in Flash .rodata
    if (
#if BMO_HAS_VIRTUAL_BMO_ROM
        buffer == virtual_bmo_rom ||
#endif
        buffer == mario_deluxe_rom || buffer == zelda_ages_rom || buffer == aladdin_rom ||
        buffer == lego_racers_rom) {
      return;
    }
    heap_caps_free(buffer);
  }
}

