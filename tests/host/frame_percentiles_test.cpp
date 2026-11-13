#include <stddef.h>

#include "frame_percentiles.h"

static bool isExpected(unsigned long* samples, size_t count, unsigned long p50, unsigned long p95, unsigned long p99) {
  FramePercentiles::Summary actual;
  return FramePercentiles::sortAndSummarize(samples, count, actual) && actual.p50 == p50 && actual.p95 == p95 &&
         actual.p99 == p99;
}

int main() {
  unsigned long values[FramePercentiles::WINDOW_SIZE];
  for (size_t i = 0; i < FramePercentiles::WINDOW_SIZE; ++i)
    values[i] = (unsigned long)i;
  if (!isExpected(values, FramePercentiles::WINDOW_SIZE, 299, 569, 593)) return 1;

  for (size_t i = 0; i < FramePercentiles::WINDOW_SIZE; ++i)
    values[i] = (unsigned long)(FramePercentiles::WINDOW_SIZE - i - 1);
  if (!isExpected(values, FramePercentiles::WINDOW_SIZE, 299, 569, 593)) return 2;

  for (size_t i = 0; i < FramePercentiles::WINDOW_SIZE; ++i)
    values[i] = 42UL;
  if (!isExpected(values, FramePercentiles::WINDOW_SIZE, 42, 42, 42)) return 3;

  for (size_t i = 0; i < FramePercentiles::WINDOW_SIZE; ++i)
    values[i] = 7UL;
  values[598] = 9000;
  values[599] = 10000;
  if (!isExpected(values, FramePercentiles::WINDOW_SIZE, 7, 7, 7)) return 4;

  FramePercentiles::Summary invalid;
  if (FramePercentiles::sortAndSummarize(nullptr, FramePercentiles::WINDOW_SIZE, invalid)) return 5;
  if (FramePercentiles::sortAndSummarize(values, FramePercentiles::WINDOW_SIZE - 1, invalid)) return 6;
  return 0;
}
