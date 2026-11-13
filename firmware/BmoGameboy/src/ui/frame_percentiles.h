#pragma once

#include <stddef.h>
#include <stdlib.h>

namespace FramePercentiles {

constexpr size_t WINDOW_SIZE = 600;
constexpr size_t P50_NEAREST_RANK = 300;
constexpr size_t P95_NEAREST_RANK = 570;
constexpr size_t P99_NEAREST_RANK = 594;

struct Summary {
  unsigned long p50;
  unsigned long p95;
  unsigned long p99;
};

inline int compareUnsignedLong(const void* left, const void* right) {
  const unsigned long a = *(const unsigned long*)left;
  const unsigned long b = *(const unsigned long*)right;
  return (a > b) - (a < b);
}

inline bool sortAndSummarize(unsigned long* samples, size_t count, Summary& summary) {
  if (!samples || count != WINDOW_SIZE) return false;
  qsort(samples, count, sizeof(samples[0]), compareUnsignedLong);
  summary.p50 = samples[P50_NEAREST_RANK - 1];
  summary.p95 = samples[P95_NEAREST_RANK - 1];
  summary.p99 = samples[P99_NEAREST_RANK - 1];
  return true;
}

} // namespace FramePercentiles
