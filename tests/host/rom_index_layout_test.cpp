#include "rom_index.h"
#include "sd_card.h"

static_assert(sizeof(BmoIndexHeader) == 44, "BMO index v3 header layout");
static_assert(sizeof(BmoIndexEntry) == 20, "BMO index v3 entry layout");
static_assert(offsetof(BmoIndexEntry, fileSize) == 8, "BMO index v3 file size offset");
static_assert(sizeof(RomFile) == (sizeof(const char*) == 4U ? 16U : 24U), "ROM pool compact entry size parity");
static_assert(offsetof(RomFile, fileSize) == sizeof(const char*), "ROM file size offset parity");
static_assert(BmoIndex::catalogFingerprint(3U, 96U, 0x0102030405ULL, 1700000000U, 0x12345678U) == 0x348E1C58U,
              "ROM catalog v3 fingerprint wire compatibility");
