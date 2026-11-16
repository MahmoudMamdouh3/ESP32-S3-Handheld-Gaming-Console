// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "sd_index_io.h"

#include "../hal/config.h"
#include "../hal/spi_arbiter.h"
#include "atomic_fs.h"
#include "rom_index.h"
#include "sd_internal.h"
#if FEATURE_SD_CARD
#include <Arduino.h>
#include <SD.h>
#endif
#include <esp_heap_caps.h>
#include <string.h>
#include <time.h>

namespace BmoSdIndexIo {
namespace {

#if FEATURE_SD_CARD
bool validateIndexHeaderForVerify(File& file, size_t expectedSize, uint32_t expectedCrc, size_t& outRemaining) {
  if (file.size() != expectedSize) return false;
  BmoIndexHeader header;
  if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) return false;
  if (memcmp(header.magic, BMO_INDEX_MAGIC, sizeof(header.magic)) != 0 || header.version != BMO_INDEX_VERSION ||
      header.entrySize != sizeof(BmoIndexEntry) || header.crc32 != expectedCrc) {
    return false;
  }
  const size_t entriesBytes = static_cast<size_t>(header.entryCount) * sizeof(BmoIndexEntry);
  if (entriesBytes > expectedSize || header.stringTableBytes > expectedSize - sizeof(header) - entriesBytes) {
    return false;
  }
  outRemaining = entriesBytes + static_cast<size_t>(header.stringTableBytes);
  return true;
}

bool indexFileIsValidV3(const char* path) {
  SpiArbiter::ScopedLock spiLock;
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

bool stampHostIndexInPlace(BmoIndexHeader& header, uint64_t totalFileBytes, size_t expectedFileSize) {
  const uint32_t stamp = rootDirectoryStamp();
  if (stamp == 0U) return false;
  header.timestamp = stamp;
  header.fingerprint =
      BmoIndex::catalogFingerprint(header.entryCount, header.stringTableBytes, totalFileBytes, stamp, header.crc32);
  SpiArbiter::ScopedLock spiLock;
  File f = SD.open(BMO_INDEX_PATH, "r+");
  if (!f) return false;
  const bool ok = f.seek(0) && f.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header);
  f.flush();
  f.close();
  return ok && rootDirectoryStamp() == stamp && verifyIndexV3File(BMO_INDEX_PATH, expectedFileSize, header.crc32);
}

bool validateLoadHeader(File& f, size_t actualSize, int bakedCount, BmoIndexHeader& outHeader, bool& outHostUnstamped,
                        size_t& outExpectedSize) {
  if (actualSize < sizeof(BmoIndexHeader)) {
    char magicPeek[8] = {};
    if (actualSize >= sizeof(magicPeek) && f.read(reinterpret_cast<uint8_t*>(magicPeek), sizeof(magicPeek)) == 8 &&
        memcmp(magicPeek, BMO_INDEX_LEGACY_MAGIC_V2, 8) == 0) {
      LOG_INFO_STR("SDCard: Legacy BMOIDX02 index found; migrating to BMOIDX03 via scan");
    }
    return false;
  }
  if (f.read(reinterpret_cast<uint8_t*>(&outHeader), sizeof(outHeader)) != sizeof(outHeader)) return false;
  if (memcmp(outHeader.magic, BMO_INDEX_LEGACY_MAGIC_V2, 8) == 0) {
    LOG_INFO_STR("SDCard: Legacy BMOIDX02 index found; upgrading to BMOIDX03 via scan");
    return false;
  }
  const uint32_t currentStamp = rootDirectoryStamp();
  outHostUnstamped = (outHeader.timestamp == 0U && currentStamp != 0U);
  const bool headerFieldsValid =
      memcmp(outHeader.magic, BMO_INDEX_MAGIC, 8) == 0 && outHeader.version == BMO_INDEX_VERSION &&
      outHeader.entrySize == sizeof(BmoIndexEntry) && outHeader.entryCount > 0U &&
      outHeader.entryCount <= static_cast<uint32_t>(SDCard::MAX_ROM_POOL - bakedCount) &&
      outHeader.stringTableBytes > 0U && outHeader.stringTableBytes <= BmoIndex::MAX_STRING_TABLE_BYTES;
  if (!headerFieldsValid || (!outHostUnstamped && (outHeader.timestamp == 0U || outHeader.timestamp != currentStamp))) {
    return false;
  }
  const size_t entriesBytes = static_cast<size_t>(outHeader.entryCount) * sizeof(BmoIndexEntry);
  const size_t stringBytes = static_cast<size_t>(outHeader.stringTableBytes);
  if (entriesBytes / sizeof(BmoIndexEntry) != outHeader.entryCount ||
      entriesBytes + stringBytes > SIZE_MAX - sizeof(BmoIndexHeader)) {
    return false;
  }
  outExpectedSize = sizeof(BmoIndexHeader) + entriesBytes + stringBytes;
  return actualSize == outExpectedSize;
}

bool validateAndApplyLoadedCatalog(BmoIndexHeader& header, const BmoIndexEntry* entries, size_t entriesBytes,
                                   const char* arena, size_t stringBytes, bool hostUnstamped, size_t expectedFileSize) {
  using namespace BmoSdInternal;
  uint32_t crc = AtomicFs::crc32Update(reinterpret_cast<const uint8_t*>(entries), entriesBytes, 0xFFFFFFFFU);
  crc = AtomicFs::crc32Update(reinterpret_cast<const uint8_t*>(arena), stringBytes, crc);
  crc ^= 0xFFFFFFFFU;
  if (crc != header.crc32) return false;

  uint64_t totalFileBytes = 0U;
  for (uint32_t i = 0; i < header.entryCount; ++i) {
    if (entries[i].type < ROM_GB || entries[i].type > ROM_COLEM ||
        !BmoIndex::validateEntryRecord(entries[i], header.stringTableBytes, arena)) {
      return false;
    }
    totalFileBytes += entries[i].fileSize;
  }
  if (static_cast<uint32_t>(totalFileBytes) != header.totalBytesLow ||
      static_cast<uint32_t>(totalFileBytes >> 32) != header.totalBytesHigh ||
      BmoIndex::catalogFingerprint(header.entryCount, header.stringTableBytes, totalFileBytes, header.timestamp,
                                   header.crc32) != header.fingerprint) {
    return false;
  }
  if (hostUnstamped && !stampHostIndexInPlace(header, totalFileBytes, expectedFileSize)) {
    return false;
  }

  for (uint32_t i = 0; i < header.entryCount && numRoms < maxCapacity; ++i) {
    const BmoIndexEntry& e = entries[i];
    romList[numRoms].filename = arena + e.pathOffset;
    romList[numRoms].type = static_cast<RomType>(e.type);
    romList[numRoms].isFavorite = (e.flags & BmoIndex::FLAG_FAVORITE) ? 1U : 0U;
    if (romList[numRoms].isFavorite) favoritesCount++;
    romList[numRoms].hasBoxArt = (e.flags & BmoIndex::FLAG_BOX_ART) ? 1U : 0U;
    romList[numRoms].isPopular =
        (e.flags & BmoIndex::FLAG_POPULAR)
            ? 1U
            : (SDCard::isPopularGame(romList[numRoms].filename, romList[numRoms].type) ? 1U : 0U);
    romList[numRoms].fileSize = e.fileSize;
    romList[numRoms].crc32Prefix = e.crc32Prefix;
    if (romList[numRoms].type <= ROM_COLEM) romCountsByType[romList[numRoms].type]++;
    numRoms++;
  }
  return true;
}

bool prepareSavePayload(uint32_t entryCount, int bakedCount, BmoIndexEntry*& entriesOut, char*& stringTableOut,
                        size_t& entriesBytesOut, size_t& stringBytesOut, BmoIndexHeader& headerOut,
                        uint64_t& totalFileBytesOut) {
  using namespace BmoSdInternal;
  stringBytesOut = 0U;
  for (uint32_t i = 0; i < entryCount; ++i) {
    const char* name = romList[bakedCount + static_cast<int>(i)].filename;
    const size_t len = name ? strlen(name) : 0U;
    if (len == 0U || len > SDCard::MAX_ROM_PATH_LEN) return false;
    stringBytesOut += len + 1U;
  }
  if (stringBytesOut == 0U || stringBytesOut > BmoIndex::MAX_STRING_TABLE_BYTES) return false;

  entriesBytesOut = static_cast<size_t>(entryCount) * sizeof(BmoIndexEntry);
  entriesOut = static_cast<BmoIndexEntry*>(heap_caps_malloc(entriesBytesOut, MALLOC_CAP_SPIRAM));
  stringTableOut = static_cast<char*>(heap_caps_malloc(stringBytesOut, MALLOC_CAP_SPIRAM));
  if (!entriesOut || !stringTableOut) return false;

  size_t strPos = 0U;
  totalFileBytesOut = 0U;
  for (uint32_t i = 0; i < entryCount; ++i) {
    const RomFile& rf = romList[bakedCount + static_cast<int>(i)];
    const size_t len = strlen(rf.filename);
    memcpy(stringTableOut + strPos, rf.filename, len + 1U);
    entriesOut[i].pathOffset = static_cast<uint32_t>(strPos);
    entriesOut[i].pathLength = static_cast<uint16_t>(len);
    entriesOut[i].type = static_cast<uint8_t>(rf.type);
    entriesOut[i].flags = (rf.isFavorite ? BmoIndex::FLAG_FAVORITE : 0U) |
                          (rf.hasBoxArt ? BmoIndex::FLAG_BOX_ART : 0U) | (rf.isPopular ? BmoIndex::FLAG_POPULAR : 0U);
    entriesOut[i].fileSize = rf.fileSize;
    entriesOut[i].crc32Prefix = rf.crc32Prefix;
    entriesOut[i].reserved = 0U;
    strPos += len + 1U;
    totalFileBytesOut += rf.fileSize;
  }

  const uint32_t rootStamp = rootDirectoryStamp();
  if (rootStamp == 0U) {
    LOG_WARN_STR("SDCard: root directory timestamp unavailable; ROM index cache not saved");
    return false;
  }
  uint32_t crc = AtomicFs::crc32Update(reinterpret_cast<const uint8_t*>(entriesOut), entriesBytesOut, 0xFFFFFFFFU);
  crc = AtomicFs::crc32Update(reinterpret_cast<const uint8_t*>(stringTableOut), stringBytesOut, crc) ^ 0xFFFFFFFFU;

  memset(&headerOut, 0, sizeof(headerOut));
  memcpy(headerOut.magic, BMO_INDEX_MAGIC, 8);
  headerOut.version = BMO_INDEX_VERSION;
  headerOut.entryCount = entryCount;
  headerOut.entrySize = sizeof(BmoIndexEntry);
  headerOut.stringTableBytes = static_cast<uint32_t>(stringBytesOut);
  headerOut.crc32 = crc;
  headerOut.timestamp = 0U;
  headerOut.totalBytesLow = static_cast<uint32_t>(totalFileBytesOut);
  headerOut.totalBytesHigh = static_cast<uint32_t>(totalFileBytesOut >> 32);
  headerOut.fingerprint =
      BmoIndex::catalogFingerprint(entryCount, headerOut.stringTableBytes, totalFileBytesOut, rootStamp, crc);
  return true;
}

bool syncFinalTimestampOnDisk(BmoIndexHeader& header, uint64_t totalFileBytes, size_t expectedFileSize) {
  SpiArbiter::ScopedLock spiLock;
  header.timestamp = rootDirectoryStamp();
  header.fingerprint = BmoIndex::catalogFingerprint(header.entryCount, header.stringTableBytes, totalFileBytes,
                                                    header.timestamp, header.crc32);
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
}

bool writeAndCommitIndexFile(BmoIndexHeader& header, const BmoIndexEntry* entries, size_t entriesBytes,
                             const char* stringTable, size_t stringBytes, uint64_t totalFileBytes) {
  SpiArbiter::ScopedLock spiLock;
  if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
  File f = SD.open(BMO_INDEX_TEMP_PATH, FILE_WRITE);
  if (!f) return false;

  bool ok = f.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header) &&
            f.write(reinterpret_cast<const uint8_t*>(entries), entriesBytes) == entriesBytes &&
            f.write(reinterpret_cast<const uint8_t*>(stringTable), stringBytes) == stringBytes;
  f.flush();
  f.close();

  const size_t expectedSize = sizeof(BmoIndexHeader) + entriesBytes + stringBytes;
  ok = ok && verifyIndexV3File(BMO_INDEX_TEMP_PATH, expectedSize, header.crc32);

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
  if (ok) ok = syncFinalTimestampOnDisk(header, totalFileBytes, expectedSize);
  if (ok) {
    if (hadBackup) SD.remove(BMO_INDEX_BACKUP_PATH);
    return syncFinalTimestampOnDisk(header, totalFileBytes, expectedSize);
  }
  if (SD.exists(BMO_INDEX_PATH)) SD.remove(BMO_INDEX_PATH);
  if (hadBackup && SD.exists(BMO_INDEX_BACKUP_PATH)) SD.rename(BMO_INDEX_BACKUP_PATH, BMO_INDEX_PATH);
  if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
  return false;
}
#endif

} // namespace

uint32_t rootDirectoryStamp() {
#if FEATURE_SD_CARD
  SpiArbiter::ScopedLock spiLock;
  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return 0;
  }
  const time_t stamp = root.getLastWrite();
  root.close();
  return stamp > 0 ? static_cast<uint32_t>(stamp) : 0;
#else
  return 0;
#endif
}

bool verifyIndexV3File(const char* path, size_t expectedSize, uint32_t expectedCrc) {
#if FEATURE_SD_CARD
  SpiArbiter::ScopedLock spiLock;
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  size_t remaining = 0U;
  if (!validateIndexHeaderForVerify(file, expectedSize, expectedCrc, remaining)) {
    file.close();
    return false;
  }
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
#else
  (void)path;
  (void)expectedSize;
  (void)expectedCrc;
  return false;
#endif
}

void recoverIndexTransaction() {
#if FEATURE_SD_CARD
  SpiArbiter::ScopedLock spiLock;
  const bool hasIndex = SD.exists(BMO_INDEX_PATH);
  const bool hasBackup = SD.exists(BMO_INDEX_BACKUP_PATH);
  if (hasIndex && indexFileIsValidV3(BMO_INDEX_PATH)) {
    if (hasBackup) SD.remove(BMO_INDEX_BACKUP_PATH);
  } else if (hasBackup && indexFileIsValidV3(BMO_INDEX_BACKUP_PATH)) {
    if (hasIndex) SD.remove(BMO_INDEX_PATH);
    SD.rename(BMO_INDEX_BACKUP_PATH, BMO_INDEX_PATH);
  }
  if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
#endif
}

} // namespace BmoSdIndexIo

bool SDCard::loadIndex() {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  SpiArbiter::ScopedLock spiLock;
  if (!SD.exists(BMO_INDEX_PATH)) return false;

  File f = SD.open(BMO_INDEX_PATH, FILE_READ);
  if (!f) return false;

  BmoIndexHeader header{};
  bool hostUnstamped = false;
  size_t expectedFileSize = 0U;
  if (!BmoSdIndexIo::validateLoadHeader(f, f.size(), bakedRomCount(), header, hostUnstamped, expectedFileSize)) {
    f.close();
    return false;
  }

  const size_t entriesBytes = static_cast<size_t>(header.entryCount) * sizeof(BmoIndexEntry);
  const size_t stringBytes = static_cast<size_t>(header.stringTableBytes);
  const int targetCap = computePoolCapacity(static_cast<int>(header.entryCount) + bakedRomCount());
  if (!ensurePoolCapacity(targetCap) || !reserveExactPathArena(stringBytes)) {
    f.close();
    return false;
  }

  BmoIndexEntry* entries = static_cast<BmoIndexEntry*>(heap_caps_malloc(entriesBytes, MALLOC_CAP_SPIRAM));
  char* arena = const_cast<char*>(arenaBase());
  const bool readOk = entries && arena && f.read(reinterpret_cast<uint8_t*>(entries), entriesBytes) == entriesBytes &&
                      f.read(reinterpret_cast<uint8_t*>(arena), stringBytes) == stringBytes;
  f.close();

  const bool loaded = readOk && BmoSdIndexIo::validateAndApplyLoadedCatalog(
                                    header, entries, entriesBytes, arena, stringBytes, hostUnstamped, expectedFileSize);
  if (entries) heap_caps_free(entries);
  if (!loaded) resetPathArena();
  return loaded;
#else
  return false;
#endif
}

bool SDCard::saveIndex() {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  SpiArbiter::ScopedLock spiLock;
  const int bakedCount = bakedRomCount();
  if (numRoms <= bakedCount) return false;
  const uint32_t entryCount = static_cast<uint32_t>(numRoms - bakedCount);

  BmoIndexEntry* entries = nullptr;
  char* stringTable = nullptr;
  size_t entriesBytes = 0U;
  size_t stringBytes = 0U;
  BmoIndexHeader header{};
  uint64_t totalFileBytes = 0U;
  const bool prepared = BmoSdIndexIo::prepareSavePayload(entryCount, bakedCount, entries, stringTable, entriesBytes,
                                                         stringBytes, header, totalFileBytes);
  const bool ok = prepared && BmoSdIndexIo::writeAndCommitIndexFile(header, entries, entriesBytes, stringTable,
                                                                    stringBytes, totalFileBytes);
  if (entries) heap_caps_free(entries);
  if (stringTable) heap_caps_free(stringTable);
  return ok;
#else
  return false;
#endif
}
