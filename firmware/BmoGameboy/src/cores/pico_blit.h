// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace PicoBlit {

// Precomputes a 256-entry packed pair table: each byte `b` encodes two 4-bit PICO-8
// pixels (low nibble = even x, high nibble = odd x) mapped through screenPaletteMap[0..15]
// into panel BGR565 words (low 16 bits = even x, high 16 bits = odd x on little-endian CPU).
inline void buildPairedLut(const uint8_t* screenPaletteMap, const uint16_t* panelPalette, uint32_t* outPairs) {
  uint16_t idxColor[16];
  for (int i = 0; i < 16; ++i) {
    idxColor[i] = panelPalette[screenPaletteMap[i] & 0x8FU];
  }
  for (int byteVal = 0; byteVal < 256; ++byteVal) {
    const uint32_t p0 = idxColor[byteVal & 0x0F];
    const uint32_t p1 = idxColor[byteVal >> 4];
    outPairs[byteVal] = p0 | (p1 << 16);
  }
}

inline void buildMappedPalette16(const uint8_t* screenPaletteMap, const uint16_t* panelPalette, uint16_t* out16) {
  for (int i = 0; i < 16; ++i) {
    out16[i] = panelPalette[screenPaletteMap[i] & 0x8FU];
  }
}

namespace detail {

inline void renderMode0(const uint8_t* picoFb, const uint32_t* pairLut, uint16_t* panelFb) {
  uint32_t* out32 = reinterpret_cast<uint32_t*>(panelFb);
  for (size_t i = 0; i < 128U * 64U; ++i) {
    out32[i] = pairLut[picoFb[i]];
  }
}

inline void renderScaledUnrotated(const uint8_t* picoFb, uint8_t drawMode, const uint16_t* c16, uint16_t* panelFb) {
  const int xShift = (drawMode == 1 || drawMode == 3) ? 1 : 0;
  const int yShift = (drawMode == 2 || drawMode == 3) ? 1 : 0;
  for (int y = 0; y < 128; ++y) {
    const uint8_t* rowIn = picoFb + static_cast<size_t>(y >> yShift) * 64U;
    uint16_t* rowOut = panelFb + static_cast<size_t>(y) * 128U;
    if (xShift == 0) {
      for (int bx = 0; bx < 64; ++bx) {
        const uint8_t b = rowIn[bx];
        rowOut[bx * 2] = c16[b & 0x0FU];
        rowOut[bx * 2 + 1] = c16[b >> 4];
      }
    } else {
      for (int bx = 0; bx < 32; ++bx) {
        const uint8_t b = rowIn[bx];
        const uint16_t p0 = c16[b & 0x0FU];
        const uint16_t p1 = c16[b >> 4];
        const int base = bx * 4;
        rowOut[base] = p0;
        rowOut[base + 1] = p0;
        rowOut[base + 2] = p1;
        rowOut[base + 3] = p1;
      }
    }
  }
}

inline void renderMirroredOrRotated(const uint8_t* picoFb, uint8_t drawMode, const uint16_t* c16, uint16_t* panelFb) {
  for (int y = 0; y < 128; ++y) {
    uint16_t* rowOut = panelFb + static_cast<size_t>(y) * 128U;
    for (int x = 0; x < 128; ++x) {
      int sx = x;
      int sy = y;
      switch (drawMode) {
      case 129:
        sx = 127 - x;
        break;
      case 130:
        sy = 127 - y;
        break;
      case 131:
      case 134:
        sx = 127 - x;
        sy = 127 - y;
        break;
      case 133:
        sx = y;
        sy = 127 - x;
        break;
      case 135:
        sx = 127 - y;
        sy = x;
        break;
      default:
        break;
      }
      const uint8_t packed = picoFb[static_cast<size_t>(sy) * 64U + static_cast<size_t>(sx >> 1)];
      rowOut[x] = c16[(sx & 1) ? (packed >> 4) : (packed & 0x0FU)];
    }
  }
}

} // namespace detail

inline void renderFrame(const uint8_t* picoFb, const uint8_t* screenPaletteMap, const uint16_t* panelPalette,
                        uint8_t drawMode, uint16_t* panelFb) {
  if (!picoFb || !screenPaletteMap || !panelPalette || !panelFb) return;
  if (drawMode == 0) {
    uint32_t pairLut[256];
    buildPairedLut(screenPaletteMap, panelPalette, pairLut);
    detail::renderMode0(picoFb, pairLut, panelFb);
    return;
  }
  uint16_t c16[16];
  buildMappedPalette16(screenPaletteMap, panelPalette, c16);
  if (drawMode <= 3) {
    detail::renderScaledUnrotated(picoFb, drawMode, c16, panelFb);
  } else {
    detail::renderMirroredOrRotated(picoFb, drawMode, c16, panelFb);
  }
}

} // namespace PicoBlit
