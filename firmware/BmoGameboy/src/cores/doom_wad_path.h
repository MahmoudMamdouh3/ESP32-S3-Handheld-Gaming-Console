#pragma once

#include <stddef.h>
#include <stdio.h>

namespace DoomWadPath {

inline constexpr size_t MAX_WAD_PATH_BYTES = 264;

inline bool build(const char* romFilename, char* out, size_t outSize) {
  if (!romFilename || !romFilename[0] || !out || outSize < 6U) return false;
  const int wadLen = snprintf(out, outSize, "/sd/%s", romFilename);
  return wadLen > 4 && static_cast<size_t>(wadLen) < outSize;
}

} // namespace DoomWadPath
