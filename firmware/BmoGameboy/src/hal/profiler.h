#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// -----------------------------------------------------------------------------
// profiler.h — Microsecond Telemetry & 600-Frame Ring Percentile Subsystem
//
// Gated at compile-time by FEATURE_PROFILER (or BMO_PROFILE alias).
// When FEATURE_PROFILER == 0:
//   - All BMO_PROFILE_* macros expand to no-ops (do {} while(0))
//   - Compiles to 0 bytes of Flash, 0 bytes of SRAM, and 0 CPU cycles.
// When FEATURE_PROFILER == 1:
//   - Uses esp_timer_get_time() (monotonic 64-bit microseconds) for wrap-free
//     sub-millisecond timing of Core 0/Core 1 zones and a 600-frame sliding
//     window (min/avg/max/p50/p95/p99) reported via Diagnostics and Serial CSV.
// -----------------------------------------------------------------------------

#ifndef FEATURE_PROFILER
#ifdef BMO_PROFILE
#define FEATURE_PROFILER BMO_PROFILE
#else
#define FEATURE_PROFILER 0
#endif
#endif

#ifndef BMO_PROFILE
#define BMO_PROFILE FEATURE_PROFILER
#endif

#if defined(ESP32) || defined(ARDUINO)
#include "config.h"
#endif

enum ProfileZone {
  ZONE_EMU_FRAME = 0,
  ZONE_SPI_STREAM,
  ZONE_BMO_FACE_RENDER,
  ZONE_MENU_RENDER,
  ZONE_SD_READ,
  ZONE_COUNT
};

namespace BmoProfilerTypes {

struct FrameSummary {
  uint32_t sampleCount;
  uint32_t totalFrames;
  uint32_t minUs;
  uint32_t avgUs;
  uint32_t maxUs;
  uint32_t p50Us;
  uint32_t p95Us;
  uint32_t p99Us;
};

} // namespace BmoProfilerTypes

namespace BmoProfilerCore {

constexpr size_t FRAME_RING_CAPACITY = 600U;

struct ProfilerState {
  uint32_t frameRing[FRAME_RING_CAPACITY];
  uint32_t ringWriteIdx;
  uint32_t ringCount;
  uint32_t totalFrames;
  int64_t zoneStartUs[ZONE_COUNT];
  uint32_t zoneMinUs[ZONE_COUNT];
  uint32_t zoneMaxUs[ZONE_COUNT];
  uint64_t zoneTotalUs[ZONE_COUNT];
  uint32_t zoneSamples[ZONE_COUNT];
};

inline int compareU32(const void* left, const void* right) {
  const uint32_t a = *static_cast<const uint32_t*>(left);
  const uint32_t b = *static_cast<const uint32_t*>(right);
  return (a > b) - (a < b);
}

inline void resetZones(ProfilerState& state) {
  for (int i = 0; i < ZONE_COUNT; ++i) {
    state.zoneStartUs[i] = -1;
    state.zoneMinUs[i] = 0xFFFFFFFFU;
    state.zoneMaxUs[i] = 0U;
    state.zoneTotalUs[i] = 0ULL;
    state.zoneSamples[i] = 0U;
  }
}

inline void resetAll(ProfilerState& state) {
  for (size_t i = 0; i < FRAME_RING_CAPACITY; ++i) {
    state.frameRing[i] = 0U;
  }
  state.ringWriteIdx = 0U;
  state.ringCount = 0U;
  state.totalFrames = 0U;
  resetZones(state);
}

inline void recordZoneElapsed(ProfilerState& state, ProfileZone zone, uint32_t elapsedUs) {
  if (zone < 0 || zone >= ZONE_COUNT) return;
  if (elapsedUs < state.zoneMinUs[zone]) state.zoneMinUs[zone] = elapsedUs;
  if (elapsedUs > state.zoneMaxUs[zone]) state.zoneMaxUs[zone] = elapsedUs;
  state.zoneTotalUs[zone] += elapsedUs;
  state.zoneSamples[zone]++;

  if (zone == ZONE_EMU_FRAME) {
    state.frameRing[state.ringWriteIdx] = elapsedUs;
    state.ringWriteIdx = (state.ringWriteIdx + 1U) % static_cast<uint32_t>(FRAME_RING_CAPACITY);
    if (state.ringCount < static_cast<uint32_t>(FRAME_RING_CAPACITY)) {
      state.ringCount++;
    }
    state.totalFrames++;
  }
}

inline bool summarizeFrames(const ProfilerState& state, BmoProfilerTypes::FrameSummary& out, uint32_t* sortScratch,
                            size_t scratchCap) {
  out = {};
  if (state.ringCount == 0U || !sortScratch || scratchCap < state.ringCount) {
    return false;
  }

  const uint32_t n = state.ringCount;
  const uint32_t oldestIdx = (n < static_cast<uint32_t>(FRAME_RING_CAPACITY)) ? 0U : state.ringWriteIdx;

  uint64_t sumUs = 0ULL;
  uint32_t minVal = 0xFFFFFFFFU;
  uint32_t maxVal = 0U;
  for (uint32_t i = 0; i < n; ++i) {
    const uint32_t slot = (oldestIdx + i) % static_cast<uint32_t>(FRAME_RING_CAPACITY);
    const uint32_t v = state.frameRing[slot];
    sortScratch[i] = v;
    sumUs += v;
    if (v < minVal) minVal = v;
    if (v > maxVal) maxVal = v;
  }

  qsort(sortScratch, n, sizeof(sortScratch[0]), compareU32);

  // Nearest-rank indices match FramePercentiles (300/570/594 at N=600).
  const uint32_t p50Rank = ((n * 50U) + 99U) / 100U;
  const uint32_t p95Rank = ((n * 95U) + 99U) / 100U;
  const uint32_t p99Rank = ((n * 99U) + 99U) / 100U;

  out.sampleCount = n;
  out.totalFrames = state.totalFrames;
  out.minUs = minVal;
  out.avgUs = static_cast<uint32_t>(sumUs / n);
  out.maxUs = maxVal;
  out.p50Us = sortScratch[p50Rank > 0U ? (p50Rank - 1U) : 0U];
  out.p95Us = sortScratch[p95Rank > 0U ? (p95Rank - 1U) : 0U];
  out.p99Us = sortScratch[p99Rank > 0U ? (p99Rank - 1U) : 0U];
  return true;
}

inline const char* csvHeaderLine() {
  return "[PROFILE_CSV_HDR] emu,n,target_us,fps,min_us,avg_us,max_us,p50_us,p95_us,p99_us,dropped,spi_avg_us";
}

inline bool formatCsvRow(char* dst, size_t dstLen, int emuIndex, uint32_t targetUs, uint32_t fps,
                         uint32_t droppedFrames, const BmoProfilerTypes::FrameSummary& summary, uint32_t spiAvgUs) {
  if (!dst || dstLen == 0U) return false;
  const int written = snprintf(dst, dstLen, "[PROFILE_CSV] %d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u", emuIndex,
                               (unsigned int)summary.sampleCount, (unsigned int)targetUs, (unsigned int)fps,
                               (unsigned int)summary.minUs, (unsigned int)summary.avgUs, (unsigned int)summary.maxUs,
                               (unsigned int)summary.p50Us, (unsigned int)summary.p95Us, (unsigned int)summary.p99Us,
                               (unsigned int)droppedFrames, (unsigned int)spiAvgUs);
  return written > 0 && static_cast<size_t>(written) < dstLen;
}

} // namespace BmoProfilerCore

class BmoProfiler {
public:
  static constexpr bool isEnabled() {
#if FEATURE_PROFILER
    return true;
#else
    return false;
#endif
  }

#if FEATURE_PROFILER
  static void begin();
  static void startZone(ProfileZone zone);
  static void endZone(ProfileZone zone);
  static void reportIfInterval(unsigned long intervalMs = 1000, int emuIndex = -1, uint32_t targetUs = 16667U,
                               uint32_t fps = 0U, uint32_t droppedFrames = 0U);
  static void reset();

  static bool getFrameSummary(BmoProfilerTypes::FrameSummary& out);
  static uint32_t getZoneAvgUs(ProfileZone zone);
  static uint32_t getZoneMinUs(ProfileZone zone);
  static uint32_t getZoneMaxUs(ProfileZone zone);
#else
  static void begin() {}
  static void startZone(ProfileZone) {}
  static void endZone(ProfileZone) {}
  static void reportIfInterval(unsigned long = 1000, int = -1, uint32_t = 16667U, uint32_t = 0U, uint32_t = 0U) {}
  static void reset() {}

  static bool getFrameSummary(BmoProfilerTypes::FrameSummary& out) {
    out = {};
    return false;
  }
  static uint32_t getZoneAvgUs(ProfileZone) { return 0U; }
  static uint32_t getZoneMinUs(ProfileZone) { return 0U; }
  static uint32_t getZoneMaxUs(ProfileZone) { return 0U; }
#endif
};

#if FEATURE_PROFILER
#define BMO_PROFILE_BEGIN() BmoProfiler::begin()
#define BMO_PROFILE_START(zone) BmoProfiler::startZone(zone)
#define BMO_PROFILE_END(zone) BmoProfiler::endZone(zone)
#define BMO_PROFILE_REPORT_INTERVAL(ms) BmoProfiler::reportIfInterval(ms)
#define BMO_PROFILE_REPORT_FRAME_INTERVAL(ms, emu, target, fps, dropped)                                               \
  BmoProfiler::reportIfInterval((ms), (emu), (target), (fps), (dropped))
#define BMO_PROFILE_RESET() BmoProfiler::reset()
#else
#define BMO_PROFILE_BEGIN()                                                                                            \
  do {                                                                                                                 \
  } while (0)
#define BMO_PROFILE_START(zone)                                                                                        \
  do {                                                                                                                 \
  } while (0)
#define BMO_PROFILE_END(zone)                                                                                          \
  do {                                                                                                                 \
  } while (0)
#define BMO_PROFILE_REPORT_INTERVAL(ms)                                                                                \
  do {                                                                                                                 \
  } while (0)
#define BMO_PROFILE_REPORT_FRAME_INTERVAL(ms, emu, target, fps, dropped)                                               \
  do {                                                                                                                 \
  } while (0)
#define BMO_PROFILE_RESET()                                                                                            \
  do {                                                                                                                 \
  } while (0)
#endif
