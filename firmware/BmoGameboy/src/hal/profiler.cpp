#include "profiler.h"

#if FEATURE_PROFILER

#include <Arduino.h>

namespace {
  struct ZoneStats {
    uint32_t startCycles;
    uint32_t minCycles;
    uint32_t maxCycles;
    uint64_t totalCycles;
    uint32_t samples;
  };

  static ZoneStats stats[ZONE_COUNT];
  static unsigned long lastReportMs = 0;

  static const char* zoneNames[ZONE_COUNT] = {
    "EMU_FRAME",
    "SPI_STREAM",
    "BMO_FACE_RENDER",
    "MENU_RENDER",
    "SD_READ"
  };

  inline uint32_t getCycles() {
    uint32_t ccount;
    __asm__ __volatile__("rsr %0, ccount" : "=a"(ccount));
    return ccount;
  }
}

void BmoProfiler::begin() {
  reset();
  lastReportMs = millis();
}

void BmoProfiler::reset() {
  for (int i = 0; i < ZONE_COUNT; ++i) {
    stats[i].startCycles = 0;
    stats[i].minCycles = 0xFFFFFFFF;
    stats[i].maxCycles = 0;
    stats[i].totalCycles = 0;
    stats[i].samples = 0;
  }
}

void BmoProfiler::startZone(ProfileZone zone) {
  if (zone >= ZONE_COUNT) return;
  stats[zone].startCycles = getCycles();
}

void BmoProfiler::endZone(ProfileZone zone) {
  if (zone >= ZONE_COUNT) return;
  uint32_t now = getCycles();
  uint32_t elapsed = now - stats[zone].startCycles;
  
  if (elapsed < stats[zone].minCycles) stats[zone].minCycles = elapsed;
  if (elapsed > stats[zone].maxCycles) stats[zone].maxCycles = elapsed;
  stats[zone].totalCycles += elapsed;
  stats[zone].samples++;
}

void BmoProfiler::reportIfInterval(unsigned long intervalMs) {
  unsigned long now = millis();
  if (now - lastReportMs < intervalMs) return;

  LOG_INFO_STR("\n--- PROFILER TELEMETRY REPORT ---");
  for (int i = 0; i < ZONE_COUNT; ++i) {
    if (stats[i].samples == 0) continue;
    uint32_t avgCycles = (uint32_t)(stats[i].totalCycles / stats[i].samples);
    // 240 MHz -> 240 cycles per microsecond
    uint32_t avgUs = avgCycles / 240;
    uint32_t minUs = stats[i].minCycles / 240;
    uint32_t maxUs = stats[i].maxCycles / 240;

    LOG_INFO("Zone [%-15s]: Avg=%5u us | Min=%5u us | Max=%5u us | Samples=%u",
             zoneNames[i], avgUs, minUs, maxUs, stats[i].samples);
  }
  LOG_INFO_STR("---------------------------------\n");

  reset();
  lastReportMs = now;
}

uint32_t BmoProfiler::getZoneAvgUs(ProfileZone zone) {
  if (zone >= ZONE_COUNT || stats[zone].samples == 0) return 0;
  return (uint32_t)((stats[zone].totalCycles / stats[zone].samples) / 240);
}

uint32_t BmoProfiler::getZoneMinUs(ProfileZone zone) {
  if (zone >= ZONE_COUNT || stats[zone].minCycles == 0xFFFFFFFF) return 0;
  return stats[zone].minCycles / 240;
}

uint32_t BmoProfiler::getZoneMaxUs(ProfileZone zone) {
  if (zone >= ZONE_COUNT) return 0;
  return stats[zone].maxCycles / 240;
}

#endif
