#pragma once
#include <stdint.h>
#include <stddef.h>

// BMO Binary ROM Index Format Specification (v2.0)
// Designed for O(1) boot enumeration, eliminating FAT32 cluster traversal stalls.

#define BMO_INDEX_MAGIC "BMOIDX02"
#define BMO_INDEX_VERSION 2
#define BMO_INDEX_PATH "/.bmo_index"
#define BMO_INDEX_TEMP_PATH "/.bmo_index.tmp"
#define BMO_INDEX_BACKUP_PATH "/.bmo_index.bak"

#pragma pack(push, 1)

struct BmoIndexHeader {
  char magic[8];         // "BMOIDX02"
  uint32_t version;      // 2
  uint32_t entryCount;   // Total number of ROM entries
  uint32_t entrySize;    // sizeof(BmoIndexEntry) = 72
  uint32_t crc32;        // CRC32 of all BmoIndexEntry records
  uint32_t timestamp;    // Root-directory modification time; 0 disables cache reuse
  uint32_t reserved[2];  // Total catalog bytes (low 32 bits), catalog fingerprint
};

struct BmoIndexEntry {
  char filename[64];     // Null-terminated ROM filename (max 63 chars + NUL)
  uint8_t type;          // RomType enum
  uint8_t isFavorite;    // 1 if starred, 0 if not
  uint8_t hasBoxArt;     // 1 if 64x64 box art exists in /boxart/ or /covers/
  uint8_t reserved;      // Alignment byte
  uint32_t fileSize;     // File size in bytes on FAT filesystem
};

#pragma pack(pop)

namespace BmoIndex {
  constexpr uint32_t catalogFingerprint(uint32_t entryCount, uint64_t totalBytes,
                                       uint32_t rootStamp, uint32_t entriesCrc) {
    const uint32_t words[5] = {
      entryCount,
      (uint32_t)totalBytes,
      (uint32_t)(totalBytes >> 32),
      rootStamp,
      entriesCrc
    };
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t word = 0; word < 5; ++word) {
      for (size_t byte = 0; byte < 4; ++byte) {
        crc ^= (uint8_t)(words[word] >> (byte * 8));
        for (uint8_t bit = 0; bit < 8; ++bit) {
          crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
        }
      }
    }
    return crc ^ 0xFFFFFFFFU;
  }
}

static_assert(sizeof(BmoIndexHeader) == 36, "BmoIndexHeader v2 disk size changed");
static_assert(offsetof(BmoIndexHeader, timestamp) == 24, "BmoIndexHeader v2 timestamp offset changed");
static_assert(sizeof(BmoIndexEntry) == 72, "BmoIndexEntry v2 disk size changed");
static_assert(offsetof(BmoIndexEntry, type) == 64, "BmoIndexEntry v2 type offset changed");
static_assert(offsetof(BmoIndexEntry, fileSize) == 68, "BmoIndexEntry v2 fileSize offset changed");
