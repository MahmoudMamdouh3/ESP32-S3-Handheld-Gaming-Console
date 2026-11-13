#pragma once

#include <stddef.h>

namespace DoomEmu {
constexpr size_t DOOM_ZONE_MARGIN_BYTES = 512 * 1024;
constexpr size_t DOOM_ZONE_FLOOR_BYTES = 2560 * 1024;
constexpr size_t DOOM_ZONE_CAP_BYTES = 6144 * 1024;

constexpr size_t computeZoneBytes(size_t largestFreePsramBlock) {
  if (largestFreePsramBlock < DOOM_ZONE_FLOOR_BYTES + DOOM_ZONE_MARGIN_BYTES) {
    return 0;
  }
  size_t candidate = largestFreePsramBlock - DOOM_ZONE_MARGIN_BYTES;
  candidate &= ~static_cast<size_t>(65535);
  if (candidate < DOOM_ZONE_FLOOR_BYTES) return 0;
  if (candidate > DOOM_ZONE_CAP_BYTES) return DOOM_ZONE_CAP_BYTES;
  return candidate;
}
} // namespace DoomEmu
