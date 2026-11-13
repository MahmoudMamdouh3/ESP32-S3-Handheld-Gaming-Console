#include "save_state_format.h"
#include "test_check.h"

#include <string.h>

int main() {
  static_assert(sizeof(SaveStateFormat::HeaderV1) == 60U, "HeaderV1 must remain 60 bytes");
  static_assert(sizeof(SaveStateFormat::HeaderV2) == 72U, "HeaderV2 must remain 72 bytes");

  // Valid BSS2 header roundtrip
  SaveStateFormat::HeaderV2 v2;
  memset(&v2, 0, sizeof(v2));
  v2.magic = SaveStateFormat::MAGIC_V2;
  v2.version = SaveStateFormat::VERSION_V2;
  v2.headerSize = sizeof(SaveStateFormat::HeaderV2);
  v2.timestamp = 12345U;
  v2.coreId = 2U;
  v2.pathKeyHash = 0x11223344U;
  v2.romSize = 131072U;
  v2.stateSize = 4096U;
  v2.ramSize = 8192U;
  v2.crc32 = 0xAABBCCDDU;
  memcpy(v2.romTitle, "Zelda", 6);

  const size_t v2FileSize = sizeof(v2) + v2.stateSize + v2.ramSize;
  SaveStateFormat::DecodedHeader decoded;
  memset(&decoded, 0, sizeof(decoded));
  BMO_CHECK(SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&v2), sizeof(v2), v2FileSize, 2U,
                                          0x11223344U, 131072U, 8192U, 16384U, &decoded));
  BMO_CHECK(!decoded.isLegacyV1);
  BMO_CHECK(decoded.payloadOffset == sizeof(SaveStateFormat::HeaderV2));
  BMO_CHECK(decoded.header.stateSize == 4096U);
  BMO_CHECK(decoded.header.ramSize == 8192U);

  SaveStateFormat::HeaderV2 summary;
  memset(&summary, 0, sizeof(summary));
  BMO_CHECK(
      SaveStateFormat::inspectSummaryHeader(reinterpret_cast<const uint8_t*>(&v2), sizeof(v2), v2FileSize, &summary));
  BMO_CHECK(summary.coreId == 2U);
  BMO_CHECK(summary.pathKeyHash == 0x11223344U);

  // Rejection cases for BSS2
  BMO_CHECK(!SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&v2), sizeof(SaveStateFormat::HeaderV1),
                                           v2FileSize, 2U, 0x11223344U, 131072U, 8192U, 16384U, &decoded));
  BMO_CHECK(!SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&v2), sizeof(v2), v2FileSize, 3U,
                                           0x11223344U, 131072U, 8192U, 16384U, &decoded));
  BMO_CHECK(!SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&v2), sizeof(v2), v2FileSize, 2U,
                                           0x11223345U, 131072U, 8192U, 16384U, &decoded));
  BMO_CHECK(!SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&v2), sizeof(v2), v2FileSize, 2U,
                                           0x11223344U, 262144U, 8192U, 16384U, &decoded));
  BMO_CHECK(!SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&v2), sizeof(v2), v2FileSize - 1U, 2U,
                                           0x11223344U, 131072U, 8192U, 16384U, &decoded));
  BMO_CHECK(!SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&v2), sizeof(v2), v2FileSize, 2U,
                                           0x11223344U, 131072U, 2048U, 16384U, &decoded));
  BMO_CHECK(!SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&v2), sizeof(v2), v2FileSize, 2U,
                                           0x11223344U, 131072U, 8192U, 4096U, &decoded));

  SaveStateFormat::HeaderV2 badMagic = v2;
  badMagic.magic = 0xDEADBEEFU;
  BMO_CHECK(!SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&badMagic), sizeof(badMagic), v2FileSize,
                                           2U, 0x11223344U, 131072U, 8192U, 16384U, &decoded));

  // Legacy BSS1 header compatibility
  SaveStateFormat::HeaderV1 v1;
  memset(&v1, 0, sizeof(v1));
  v1.magic = SaveStateFormat::MAGIC_V1;
  v1.version = SaveStateFormat::VERSION_V1;
  v1.timestamp = 999U;
  v1.coreId = 2U;
  v1.stateSize = 2048U;
  v1.ramSize = 0U;
  v1.crc32 = 0xCAFE0001U;
  memcpy(v1.romTitle, "OldSave", 8);

  const size_t v1FileSize = sizeof(v1) + v1.stateSize + v1.ramSize;
  memset(&decoded, 0, sizeof(decoded));
  BMO_CHECK(SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&v1), sizeof(v1), v1FileSize, 2U,
                                          0x11223344U, 131072U, 8192U, 16384U, &decoded));
  BMO_CHECK(decoded.isLegacyV1);
  BMO_CHECK(decoded.payloadOffset == sizeof(SaveStateFormat::HeaderV1));
  BMO_CHECK(decoded.header.stateSize == 2048U);
  BMO_CHECK(decoded.header.ramSize == 0U);

  memset(&summary, 0, sizeof(summary));
  BMO_CHECK(
      SaveStateFormat::inspectSummaryHeader(reinterpret_cast<const uint8_t*>(&v1), sizeof(v1), v1FileSize, &summary));
  BMO_CHECK(summary.stateSize == 2048U);

  return 0;
}
