#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Ui {

inline constexpr uint16_t packDoomColor565Swapped(uint8_t r, uint8_t g, uint8_t b) {
  const uint16_t p = static_cast<uint16_t>(((static_cast<uint16_t>(b) & 0xF8U) << 8) |
                                           ((static_cast<uint16_t>(g) & 0xFCU) << 3) | (static_cast<uint16_t>(r) >> 3));
  return static_cast<uint16_t>((p >> 8) | (p << 8));
}

template <typename ColorT>
inline void rebuildDoomPalette(const ColorT* srcColors, uint16_t* dstPalette, size_t count = 256U) {
  if (!srcColors || !dstPalette) return;
  for (size_t i = 0; i < count; ++i) {
    dstPalette[i] = packDoomColor565Swapped(static_cast<uint8_t>(srcColors[i].r), static_cast<uint8_t>(srcColors[i].g),
                                            static_cast<uint8_t>(srcColors[i].b));
  }
}

template <typename ColorT> inline uint32_t hashDoomPaletteSource(const ColorT* srcColors, size_t count = 256U) {
  uint32_t h = 2166136261U;
  for (size_t i = 0; i < count; ++i) {
    h ^= static_cast<uint32_t>(static_cast<uint8_t>(srcColors[i].r)) |
         (static_cast<uint32_t>(static_cast<uint8_t>(srcColors[i].g)) << 8) |
         (static_cast<uint32_t>(static_cast<uint8_t>(srcColors[i].b)) << 16);
    h *= 16777619U;
  }
  return (h == 0U) ? 1U : h;
}

template <typename ColorT>
inline bool rebuildDoomPaletteIfChanged(const ColorT* srcColors, uint16_t* dstPalette, uint32_t& inOutSignature,
                                        size_t count = 256U) {
  if (!srcColors || !dstPalette) return false;
  const uint32_t sig = hashDoomPaletteSource(srcColors, count);
  if (sig == inOutSignature) return false;
  rebuildDoomPalette(srcColors, dstPalette, count);
  inOutSignature = sig;
  return true;
}

} // namespace Ui
