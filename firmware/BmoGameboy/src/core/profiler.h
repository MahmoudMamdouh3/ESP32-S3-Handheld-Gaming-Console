#pragma once

#include <stdint.h>
#include <stddef.h>

// -----------------------------------------------------------------------------
// profiler.h — Zero-Overhead Hardware Cycle & Microsecond Telemetry Subsystem
//
// Gated at compile-time by FEATURE_PROFILER (config.h).
// When FEATURE_PROFILER == 0:
//   - All macros expand to no-ops (do {} while(0))
//   - Compiles to 0 bytes of Flash, 0 bytes of SRAM, and 0 CPU cycles.
// When FEATURE_PROFILER == 1:
//   - Uses Xtensa 240MHz hardware cycle register (RSR CCOUNT) for sub-microsecond
//     precision timing of frame execution, SPI transfers, and SDF math.
// -----------------------------------------------------------------------------

#include "config.h"

#ifndef FEATURE_PROFILER
#define FEATURE_PROFILER 0
#endif

enum ProfileZone {
  ZONE_EMU_FRAME = 0,
  ZONE_SPI_STREAM,
  ZONE_BMO_FACE_RENDER,
  ZONE_MENU_RENDER,
  ZONE_SD_READ,
  ZONE_COUNT
};

#if FEATURE_PROFILER

#include <Arduino.h>

class BmoProfiler {
public:
  static void begin();
  static void startZone(ProfileZone zone);
  static void endZone(ProfileZone zone);
  static void reportIfInterval(unsigned long intervalMs = 1000);
  static void reset();

  static uint32_t getZoneAvgUs(ProfileZone zone);
  static uint32_t getZoneMinUs(ProfileZone zone);
  static uint32_t getZoneMaxUs(ProfileZone zone);
};

#define BMO_PROFILE_BEGIN()                  BmoProfiler::begin()
#define BMO_PROFILE_START(zone)              BmoProfiler::startZone(zone)
#define BMO_PROFILE_END(zone)                BmoProfiler::endZone(zone)
#define BMO_PROFILE_REPORT_INTERVAL(ms)      BmoProfiler::reportIfInterval(ms)
#define BMO_PROFILE_RESET()                  BmoProfiler::reset()

#else

#define BMO_PROFILE_BEGIN()                  do {} while(0)
#define BMO_PROFILE_START(zone)              do {} while(0)
#define BMO_PROFILE_END(zone)                do {} while(0)
#define BMO_PROFILE_REPORT_INTERVAL(ms)      do {} while(0)
#define BMO_PROFILE_RESET()                  do {} while(0)

#endif
