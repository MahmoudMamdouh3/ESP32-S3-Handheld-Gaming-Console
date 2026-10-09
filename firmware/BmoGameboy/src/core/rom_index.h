#pragma once
#include <stdint.h>
#include <stddef.h>

// BMO Binary ROM Index Format Specification (v1.0)
// Designed for O(1) boot enumeration, eliminating FAT32 cluster traversal stalls.

#define BMO_INDEX_MAGIC "BMOIDX01"
#define BMO_INDEX_VERSION 1
#define BMO_INDEX_PATH "/.bmo_index"

#pragma pack(push, 1)

struct BmoIndexHeader {
  char magic[8];         // "BMOIDX01"
  uint32_t version;      // 1
  uint32_t entryCount;   // Total number of ROM entries
  uint32_t entrySize;    // sizeof(BmoIndexEntry) = 72
  uint32_t crc32;        // CRC32 of all BmoIndexEntry records
  uint32_t timestamp;    // Generation timestamp / epoch
  uint32_t reserved[2];  // Future expansion (padding to 36 bytes)
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
