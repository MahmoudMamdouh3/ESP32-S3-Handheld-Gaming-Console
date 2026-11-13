#pragma once

#include <stddef.h>
#include <stdint.h>

namespace BmpHeader {

struct Info {
  uint32_t dataOffset = 0U;
  uint16_t bpp = 0U;
  bool bottomUp = true;
};

inline uint16_t readLe16(const uint8_t* ptr) {
  return static_cast<uint16_t>(static_cast<uint16_t>(ptr[0]) | (static_cast<uint16_t>(ptr[1]) << 8));
}

inline uint32_t readLe32(const uint8_t* ptr) {
  return static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) | (static_cast<uint32_t>(ptr[2]) << 16) |
         (static_cast<uint32_t>(ptr[3]) << 24);
}

/**
 * Validate a 54-byte Windows BMP header for 64x64 16bpp/24bpp cover art without
 * signed-shift or INT32_MIN negation undefined behavior.
 */
inline bool parse64x64(const uint8_t header[54], uint32_t fileSize, Info* outInfo) {
  if (!header || header[0] != 'B' || header[1] != 'M') return false;

  const uint32_t dataOffset = readLe32(header + 10);
  const uint32_t dibSize = readLe32(header + 14);
  const int32_t width = static_cast<int32_t>(readLe32(header + 18));
  const uint32_t rawHeight = readLe32(header + 22);
  const int32_t height = static_cast<int32_t>(rawHeight);
  const uint16_t planes = readLe16(header + 26);
  const uint16_t bpp = readLe16(header + 28);
  const uint32_t compression = readLe32(header + 30);

  if (dibSize < 40U || planes != 1U) return false;

  const bool bottomUp = (height > 0);
  const uint32_t absHeight = bottomUp ? static_cast<uint32_t>(height) : (0U - rawHeight);
  if (width != 64 || absHeight != 64U) return false;

  if (bpp != 24U && bpp != 16U) return false;
  if (!(compression == 0U || (bpp == 16U && compression == 3U))) return false;

  const uint32_t pixelBytes = 64U * 64U * (bpp / 8U);
  if (dataOffset < 54U || fileSize < dataOffset) return false;
  if ((fileSize - dataOffset) < pixelBytes) return false;

  if (outInfo) {
    outInfo->dataOffset = dataOffset;
    outInfo->bpp = bpp;
    outInfo->bottomUp = bottomUp;
  }
  return true;
}

} // namespace BmpHeader
