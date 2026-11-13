#pragma once

#include <stddef.h>
#include <stdint.h>

// BMO Binary ROM Index Format Specification (v3.0 - BMOIDX03)
// Variable-length UTF-8 path string table + (fileSize, crc32Prefix) ROM identity
// for O(1) boot enumeration and card-swap verification.

#define BMO_INDEX_MAGIC "BMOIDX03"
#define BMO_INDEX_VERSION 3U
#define BMO_INDEX_LEGACY_MAGIC_V2 "BMOIDX02"
#define BMO_INDEX_LEGACY_VERSION_V2 2U
#define BMO_INDEX_PATH "/.bmo_index"
#define BMO_INDEX_TEMP_PATH "/.bmo_index.tmp"
#define BMO_INDEX_BACKUP_PATH "/.bmo_index.bak"

#pragma pack(push, 1)

struct BmoIndexHeader {
  char magic[8];             // "BMOIDX03"
  uint32_t version;          // 3
  uint32_t entryCount;       // Total number of ROM entries
  uint32_t entrySize;        // sizeof(BmoIndexEntry) = 20
  uint32_t stringTableBytes; // Total UTF-8 string table byte length (NUL-terminated paths)
  uint32_t crc32;            // Standard CRC32 (^ 0xFFFFFFFF) of (entries + stringTable)
  uint32_t timestamp;        // Root-directory mtime; 0 only for prebuilt host index before 1st boot stamp
  uint32_t totalBytesLow;    // Low 32 bits of total catalog file bytes
  uint32_t totalBytesHigh;   // High 32 bits of total catalog file bytes
  uint32_t fingerprint;      // BmoIndex::catalogFingerprintV3(...)
};

struct BmoIndexEntry {
  uint32_t pathOffset;  // Byte offset inside UTF-8 string table
  uint16_t pathLength;  // Path length in bytes excluding trailing NUL
  uint8_t type;         // RomType enum
  uint8_t flags;        // BmoIndex::FLAG_* bits
  uint32_t fileSize;    // File size in bytes on FAT filesystem
  uint32_t crc32Prefix; // Standard CRC32 of first min(fileSize, ROM_PREFIX_HASH_BYTES) bytes
  uint32_t reserved;    // Must be 0
};

#pragma pack(pop)

namespace BmoIndex {

inline constexpr size_t ROM_PREFIX_HASH_BYTES = 4096U;
inline constexpr size_t MAX_STRING_TABLE_BYTES = 8U * 1024U * 1024U;
inline constexpr uint8_t FLAG_FAVORITE = 0x01U;
inline constexpr uint8_t FLAG_BOX_ART = 0x02U;
inline constexpr uint8_t FLAG_POPULAR = 0x04U;
inline constexpr uint8_t KNOWN_FLAGS_MASK = FLAG_FAVORITE | FLAG_BOX_ART | FLAG_POPULAR;

constexpr uint32_t catalogFingerprint(uint32_t entryCount, uint32_t stringTableBytes, uint64_t totalBytes,
                                      uint32_t rootStamp, uint32_t payloadCrc) {
  const uint32_t words[6] = {
      entryCount, stringTableBytes, static_cast<uint32_t>(totalBytes), static_cast<uint32_t>(totalBytes >> 32),
      rootStamp,  payloadCrc,
  };
  uint32_t crc = 0xFFFFFFFFU;
  for (size_t word = 0; word < 6; ++word) {
    for (size_t byte = 0; byte < 4; ++byte) {
      crc ^= static_cast<uint8_t>(words[word] >> (byte * 8));
      for (uint8_t bit = 0; bit < 8; ++bit) {
        crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
      }
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

inline bool validateEntryRecord(const BmoIndexEntry& entry, uint32_t stringTableBytes, const char* stringTable) {
  if (entry.reserved != 0U || (entry.flags & ~KNOWN_FLAGS_MASK) != 0U) {
    return false;
  }
  if (entry.pathLength == 0U || entry.pathLength > 255U) {
    return false;
  }
  if (entry.pathOffset >= stringTableBytes) {
    return false;
  }
  const uint32_t endWithNul = entry.pathOffset + static_cast<uint32_t>(entry.pathLength) + 1U;
  if (endWithNul <= entry.pathOffset || endWithNul > stringTableBytes) {
    return false;
  }
  if (!stringTable) {
    return false;
  }
  if (stringTable[entry.pathOffset + entry.pathLength] != '\0') {
    return false;
  }
  if (stringTable[entry.pathOffset] == '\0') {
    return false;
  }
  return true;
}

} // namespace BmoIndex

static_assert(sizeof(BmoIndexHeader) == 44, "BmoIndexHeader v3 disk size changed");
static_assert(offsetof(BmoIndexHeader, crc32) == 24, "BmoIndexHeader v3 crc32 offset changed");
static_assert(offsetof(BmoIndexHeader, timestamp) == 28, "BmoIndexHeader v3 timestamp offset changed");
static_assert(sizeof(BmoIndexEntry) == 20, "BmoIndexEntry v3 disk size changed");
static_assert(offsetof(BmoIndexEntry, fileSize) == 8, "BmoIndexEntry v3 fileSize offset changed");
static_assert(offsetof(BmoIndexEntry, crc32Prefix) == 12, "BmoIndexEntry v3 crc32Prefix offset changed");
