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

} // namespace Ui
