#include "rom_index.h"
#include "sd_card.h"

static_assert(sizeof(BmoIndexHeader) == 36, "BMO index header layout");
static_assert(sizeof(BmoIndexEntry) == 72, "BMO index entry layout");
static_assert(offsetof(BmoIndexEntry, fileSize) == 68, "BMO index file size offset");
static_assert(sizeof(RomFile) == sizeof(BmoIndexEntry), "ROM pool and index entry size parity");
static_assert(offsetof(RomFile, fileSize) == offsetof(BmoIndexEntry, fileSize), "ROM file size offset parity");
static_assert(BmoIndex::catalogFingerprint(3, 0x0102030405ULL, 1700000000U, 0x12345678U) ==
              0x4029590AU, "ROM catalog fingerprint wire compatibility");
