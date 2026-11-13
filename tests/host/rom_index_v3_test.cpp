#include "rom_index.h"
#include "test_check.h"

#include <stdint.h>
#include <string.h>

namespace {

BmoIndexEntry makeValidEntry(uint32_t offset, uint16_t length, uint8_t type, uint8_t flags, uint32_t fileSize,
                             uint32_t crc32Prefix) {
  BmoIndexEntry e;
  memset(&e, 0, sizeof(e));
  e.pathOffset = offset;
  e.pathLength = length;
  e.type = type;
  e.flags = flags;
  e.fileSize = fileSize;
  e.crc32Prefix = crc32Prefix;
  e.reserved = 0U;
  return e;
}

} // namespace

int main() {
  static_assert(sizeof(BmoIndexHeader) == 44U, "BmoIndexHeader must remain 44 bytes");
  static_assert(sizeof(BmoIndexEntry) == 20U, "BmoIndexEntry must remain 20 bytes");
  BMO_CHECK(BMO_INDEX_VERSION == 3U);
  BMO_CHECK(BmoIndex::ROM_PREFIX_HASH_BYTES == 4096U);

  const char stringTable[] = "gb/zelda.gb\0nes/mario.nes";
  const uint32_t stringTableBytes = static_cast<uint32_t>(sizeof(stringTable));

  const BmoIndexEntry gbEntry =
      makeValidEntry(0U, 11U, 1U, BmoIndex::FLAG_FAVORITE | BmoIndex::FLAG_BOX_ART, 32768U, 0x12345678U);
  BMO_CHECK(BmoIndex::validateEntryRecord(gbEntry, stringTableBytes, stringTable));

  const BmoIndexEntry nesEntry = makeValidEntry(12U, 13U, 3U, BmoIndex::FLAG_POPULAR, 40976U, 0x89ABCDEFU);
  BMO_CHECK(BmoIndex::validateEntryRecord(nesEntry, stringTableBytes, stringTable));

  // Null / empty string table must fail
  BMO_CHECK(!BmoIndex::validateEntryRecord(gbEntry, stringTableBytes, nullptr));
  BMO_CHECK(!BmoIndex::validateEntryRecord(gbEntry, 0U, stringTable));

  // Zero-length or oversized paths must fail
  BMO_CHECK(!BmoIndex::validateEntryRecord(makeValidEntry(0U, 0U, 1U, 0U, 100U, 1U), stringTableBytes, stringTable));
  BMO_CHECK(!BmoIndex::validateEntryRecord(makeValidEntry(0U, 260U, 1U, 0U, 100U, 1U), stringTableBytes, stringTable));

  // Out-of-bounds offset and uint32 wraparound overflow must fail
  BMO_CHECK(!BmoIndex::validateEntryRecord(makeValidEntry(stringTableBytes, 1U, 1U, 0U, 100U, 1U), stringTableBytes,
                                           stringTable));
  BMO_CHECK(!BmoIndex::validateEntryRecord(makeValidEntry(0xFFFFFFF8U, 16U, 1U, 0U, 100U, 1U), stringTableBytes,
                                           stringTable));

  // Missing trailing NUL (points inside string instead of at NUL) and embedded NUL before pathLength must fail
  BMO_CHECK(!BmoIndex::validateEntryRecord(makeValidEntry(0U, 10U, 1U, 0U, 100U, 1U), stringTableBytes, stringTable));
  BMO_CHECK(!BmoIndex::validateEntryRecord(makeValidEntry(0U, 15U, 1U, 0U, 100U, 1U), stringTableBytes, stringTable));

  // Reserved field non-zero or unknown flags must fail
  BmoIndexEntry badReserved = gbEntry;
  badReserved.reserved = 1U;
  BMO_CHECK(!BmoIndex::validateEntryRecord(badReserved, stringTableBytes, stringTable));
  BMO_CHECK(
      !BmoIndex::validateEntryRecord(makeValidEntry(0U, 11U, 1U, 0x08U, 100U, 1U), stringTableBytes, stringTable));

  // Entry starting with NUL even if followed by NUL terminator must fail
  const char emptyEntryTable[] = "\0\0";
  BMO_CHECK(!BmoIndex::validateEntryRecord(makeValidEntry(0U, 1U, 1U, 0U, 100U, 1U), 2U, emptyEntryTable));

  // Catalog fingerprint determinism and avalanche sensitivity
  const uint32_t fpBase = BmoIndex::catalogFingerprint(2U, stringTableBytes, 73744ULL, 100U, 0xCAFEBABEU);
  BMO_CHECK(fpBase != 0U);
  BMO_CHECK(fpBase == BmoIndex::catalogFingerprint(2U, stringTableBytes, 73744ULL, 100U, 0xCAFEBABEU));
  BMO_CHECK(fpBase != BmoIndex::catalogFingerprint(3U, stringTableBytes, 73744ULL, 100U, 0xCAFEBABEU));
  BMO_CHECK(fpBase != BmoIndex::catalogFingerprint(2U, stringTableBytes + 1U, 73744ULL, 100U, 0xCAFEBABEU));
  BMO_CHECK(fpBase != BmoIndex::catalogFingerprint(2U, stringTableBytes, 73745ULL, 100U, 0xCAFEBABEU));
  BMO_CHECK(fpBase != BmoIndex::catalogFingerprint(2U, stringTableBytes, 73744ULL, 101U, 0xCAFEBABEU));
  BMO_CHECK(fpBase != BmoIndex::catalogFingerprint(2U, stringTableBytes, 73744ULL, 100U, 0xCAFEBABFU));

  return 0;
}
