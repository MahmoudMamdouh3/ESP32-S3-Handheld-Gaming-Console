// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

inline void writeLe16(uint8_t* ptr, uint16_t val) {
  ptr[0] = static_cast<uint8_t>(val & 0xFFU);
  ptr[1] = static_cast<uint8_t>((val >> 8) & 0xFFU);
}

inline void writeLe32(uint8_t* ptr, uint32_t val) {
  ptr[0] = static_cast<uint8_t>(val & 0xFFU);
  ptr[1] = static_cast<uint8_t>((val >> 8) & 0xFFU);
  ptr[2] = static_cast<uint8_t>((val >> 16) & 0xFFU);
  ptr[3] = static_cast<uint8_t>((val >> 24) & 0xFFU);
}

/** Populate a canonical 54-byte uncompressed 24-bpp BMP header. */
inline void write24BppHeader(uint8_t header[54], int32_t width, int32_t height, uint32_t dataSize) {
  if (!header) return;
  memset(header, 0, 54);
  header[0] = 'B';
  header[1] = 'M';
  writeLe32(header + 2, 54U + dataSize);
  writeLe32(header + 10, 54U);
  writeLe32(header + 14, 40U);
  writeLe32(header + 18, static_cast<uint32_t>(width));
  writeLe32(header + 22, static_cast<uint32_t>(height));
  writeLe16(header + 26, 1U);
  writeLe16(header + 28, 24U);
  writeLe32(header + 34, dataSize);
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
