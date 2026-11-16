// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace BmoTextFit {

// Fits `source` into `outBuf` within `maxWidth` pixels using binary search on ellipsis length:
// 1 measurement when the string already fits, otherwise O(log L) measurements (<= 6 for 64 B).
template <typename MeasureWidthFn>
inline void fitCenteredWithEllipsis(const char* source, int maxWidth, char* outBuf, size_t outCap,
                                    MeasureWidthFn measureWidth) {
  if (!outBuf || outCap == 0U) return;
  char fullBuf[64];
  int len = snprintf(fullBuf, sizeof(fullBuf), "%s", source ? source : "Unknown game");
  if (len < 0) len = 0;
  if (len >= static_cast<int>(sizeof(fullBuf))) {
    len = static_cast<int>(sizeof(fullBuf)) - 1;
  }
  if (len >= static_cast<int>(outCap)) {
    len = static_cast<int>(outCap) - 1;
  }
  memcpy(outBuf, fullBuf, static_cast<size_t>(len));
  outBuf[len] = '\0';

  if (len <= 4 || measureWidth(outBuf) <= maxWidth) {
    return;
  }

  int lo = 4;
  int hi = len - 1;
  int best = 4;
  while (lo <= hi) {
    const int mid = lo + (hi - lo) / 2;
    memcpy(outBuf, fullBuf, static_cast<size_t>(mid - 3));
    outBuf[mid - 3] = '.';
    outBuf[mid - 2] = '.';
    outBuf[mid - 1] = '.';
    outBuf[mid] = '\0';
    if (measureWidth(outBuf) <= maxWidth) {
      best = mid;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }

  memcpy(outBuf, fullBuf, static_cast<size_t>(best - 3));
  outBuf[best - 3] = '.';
  outBuf[best - 2] = '.';
  outBuf[best - 1] = '.';
  outBuf[best] = '\0';
}

} // namespace BmoTextFit
