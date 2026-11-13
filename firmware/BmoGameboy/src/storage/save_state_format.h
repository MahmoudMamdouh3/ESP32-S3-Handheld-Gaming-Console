#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace SaveStateFormat {

inline constexpr uint32_t MAGIC_V1 = 0x31535342U; // "BSS1"
inline constexpr uint32_t VERSION_V1 = 1U;
inline constexpr uint32_t MAGIC_V2 = 0x32535342U; // "BSS2"
inline constexpr uint32_t VERSION_V2 = 2U;

struct HeaderV1 {
  uint32_t magic;
  uint32_t version;
  uint32_t timestamp;
  uint32_t coreId;
  uint32_t stateSize;
  uint32_t ramSize;
  uint32_t crc32;
  char romTitle[32];
};

struct HeaderV2 {
  uint32_t magic;
  uint32_t version;
  uint32_t headerSize;
  uint32_t timestamp;
  uint32_t coreId;
  uint32_t pathKeyHash;
  uint32_t romSize;
  uint32_t stateSize;
  uint32_t ramSize;
  uint32_t crc32;
  char romTitle[32];
};

static_assert(sizeof(HeaderV1) == 60U, "BSS1 legacy header size must stay 60 bytes");
static_assert(sizeof(HeaderV2) == 72U, "BSS2 header size must stay 72 bytes");

struct DecodedHeader {
  HeaderV2 header;
  size_t payloadOffset;
  bool isLegacyV1;
};

inline bool decodeHeader(const uint8_t* rawHeader, size_t bytesRead, size_t fileSize, uint32_t expectedCoreId,
                         uint32_t expectedPathKeyHash, uint32_t expectedRomSize, size_t maxStateSize, size_t maxRamSize,
                         DecodedHeader* out) {
  if (!rawHeader || !out || bytesRead < sizeof(HeaderV1) || fileSize < sizeof(HeaderV1)) {
    return false;
  }

  memset(out, 0, sizeof(*out));
  HeaderV1 v1{};
  memcpy(&v1, rawHeader, sizeof(v1));

  if (v1.magic == MAGIC_V1 && v1.version == VERSION_V1) {
    if (v1.coreId != expectedCoreId || v1.stateSize == 0U || v1.stateSize > maxStateSize) {
      return false;
    }
    if (v1.ramSize > maxRamSize) {
      return false;
    }
    const uint64_t expectedFileSize = static_cast<uint64_t>(sizeof(HeaderV1)) + static_cast<uint64_t>(v1.stateSize) +
                                      static_cast<uint64_t>(v1.ramSize);
    if (expectedFileSize != static_cast<uint64_t>(fileSize)) {
      return false;
    }

    out->header.magic = v1.magic;
    out->header.version = v1.version;
    out->header.headerSize = static_cast<uint32_t>(sizeof(HeaderV1));
    out->header.timestamp = v1.timestamp;
    out->header.coreId = v1.coreId;
    out->header.pathKeyHash = 0U;
    out->header.romSize = 0U;
    out->header.stateSize = v1.stateSize;
    out->header.ramSize = v1.ramSize;
    out->header.crc32 = v1.crc32;
    memcpy(out->header.romTitle, v1.romTitle, sizeof(out->header.romTitle));
    out->header.romTitle[sizeof(out->header.romTitle) - 1] = '\0';
    out->payloadOffset = sizeof(HeaderV1);
    out->isLegacyV1 = true;
    return true;
  }

  if (bytesRead < sizeof(HeaderV2) || fileSize < sizeof(HeaderV2)) {
    return false;
  }

  HeaderV2 v2{};
  memcpy(&v2, rawHeader, sizeof(v2));
  if (v2.magic != MAGIC_V2 || v2.version != VERSION_V2 || v2.headerSize != sizeof(HeaderV2)) {
    return false;
  }
  if (v2.coreId != expectedCoreId) {
    return false;
  }
  if (expectedPathKeyHash != 0U && v2.pathKeyHash != expectedPathKeyHash) {
    return false;
  }
  if (expectedRomSize != 0U && v2.romSize != 0U && v2.romSize != expectedRomSize) {
    return false;
  }
  if (v2.stateSize == 0U || v2.stateSize > maxStateSize || v2.ramSize > maxRamSize) {
    return false;
  }
  const uint64_t expectedFileSize =
      static_cast<uint64_t>(sizeof(HeaderV2)) + static_cast<uint64_t>(v2.stateSize) + static_cast<uint64_t>(v2.ramSize);
  if (expectedFileSize != static_cast<uint64_t>(fileSize)) {
    return false;
  }

  v2.romTitle[sizeof(v2.romTitle) - 1] = '\0';
  out->header = v2;
  out->payloadOffset = sizeof(HeaderV2);
  out->isLegacyV1 = false;
  return true;
}

inline bool inspectSummaryHeader(const uint8_t* rawHeader, size_t bytesRead, size_t fileSize, HeaderV2* outHeader) {
  if (!rawHeader || !outHeader || bytesRead < sizeof(HeaderV1) || fileSize < sizeof(HeaderV1)) {
    return false;
  }
  HeaderV1 v1{};
  memcpy(&v1, rawHeader, sizeof(v1));
  if (v1.magic == MAGIC_V1 && v1.version == VERSION_V1) {
    const uint64_t expectedSize = static_cast<uint64_t>(sizeof(HeaderV1)) + static_cast<uint64_t>(v1.stateSize) +
                                  static_cast<uint64_t>(v1.ramSize);
    if (expectedSize != static_cast<uint64_t>(fileSize)) return false;
    memset(outHeader, 0, sizeof(*outHeader));
    outHeader->magic = v1.magic;
    outHeader->version = v1.version;
    outHeader->headerSize = static_cast<uint32_t>(sizeof(HeaderV1));
    outHeader->timestamp = v1.timestamp;
    outHeader->coreId = v1.coreId;
    outHeader->stateSize = v1.stateSize;
    outHeader->ramSize = v1.ramSize;
    outHeader->crc32 = v1.crc32;
    memcpy(outHeader->romTitle, v1.romTitle, sizeof(outHeader->romTitle));
    outHeader->romTitle[sizeof(outHeader->romTitle) - 1] = '\0';
    return true;
  }
  if (bytesRead < sizeof(HeaderV2) || fileSize < sizeof(HeaderV2)) return false;
  HeaderV2 v2{};
  memcpy(&v2, rawHeader, sizeof(v2));
  if (v2.magic != MAGIC_V2 || v2.version != VERSION_V2 || v2.headerSize != sizeof(HeaderV2)) return false;
  const uint64_t expectedSize =
      static_cast<uint64_t>(sizeof(HeaderV2)) + static_cast<uint64_t>(v2.stateSize) + static_cast<uint64_t>(v2.ramSize);
  if (expectedSize != static_cast<uint64_t>(fileSize)) return false;
  v2.romTitle[sizeof(v2.romTitle) - 1] = '\0';
  *outHeader = v2;
  return true;
}

} // namespace SaveStateFormat
