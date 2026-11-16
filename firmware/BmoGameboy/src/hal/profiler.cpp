// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "profiler.h"

#if FEATURE_PROFILER

#include <Arduino.h>
#include "feature_flags.h"
#include "psram_alloc.h"
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

namespace {

static BmoProfilerCore::ProfilerState* s_state = nullptr;
static uint32_t* s_sortScratch = nullptr;
static int64_t s_lastReportUs = 0;
static bool s_csvHeaderPrinted = false;
static portMUX_TYPE s_profMux = portMUX_INITIALIZER_UNLOCKED;

static const char* kZoneNames[ZONE_COUNT] = {
    "EMU_FRAME", "SPI_STREAM", "BMO_FACE_RENDER", "MENU_RENDER", "SD_READ",
};

struct ZoneReportSnapshot {
  uint32_t zoneMinUs[ZONE_COUNT];
  uint32_t zoneMaxUs[ZONE_COUNT];
  uint64_t zoneTotalUs[ZONE_COUNT];
  uint32_t zoneSamples[ZONE_COUNT];
  uint32_t ringCount;
  uint32_t totalFrames;
};

bool ensureProfilerBuffers() {
  if (!s_state) {
    s_state =
        static_cast<BmoProfilerCore::ProfilerState*>(bmo_psram_calloc(1U, sizeof(BmoProfilerCore::ProfilerState)));
    if (s_state) BmoProfilerCore::resetAll(*s_state);
  }
  if (!s_sortScratch) {
    s_sortScratch = static_cast<uint32_t*>(bmo_psram_calloc(BmoProfilerCore::FRAME_RING_CAPACITY, sizeof(uint32_t)));
  }
  return (s_state != nullptr) && (s_sortScratch != nullptr);
}

void copyFrameRingToScratchLocked(uint32_t& outCount, uint32_t& outTotalFrames) {
  outCount = s_state->ringCount;
  outTotalFrames = s_state->totalFrames;
  if (outCount > BmoProfilerCore::FRAME_RING_CAPACITY) {
    outCount = BmoProfilerCore::FRAME_RING_CAPACITY;
  }
  if (outCount > 0U && s_sortScratch != nullptr) {
    memcpy(s_sortScratch, s_state->frameRing, static_cast<size_t>(outCount) * sizeof(uint32_t));
  }
}

} // namespace

void BmoProfiler::begin() {
  ensureProfilerBuffers();
  reset();
}

void BmoProfiler::reset() {
  if (!ensureProfilerBuffers()) return;
  portENTER_CRITICAL(&s_profMux);
  BmoProfilerCore::resetAll(*s_state);
  s_lastReportUs = esp_timer_get_time();
  s_csvHeaderPrinted = false;
  portEXIT_CRITICAL(&s_profMux);
}

void BmoProfiler::startZone(ProfileZone zone) {
  if (zone < 0 || zone >= ZONE_COUNT) return;
  if (!s_state && !ensureProfilerBuffers()) return;
  const int64_t nowUs = esp_timer_get_time();
  portENTER_CRITICAL(&s_profMux);
  s_state->zoneStartUs[zone] = nowUs;
  portEXIT_CRITICAL(&s_profMux);
}

void BmoProfiler::endZone(ProfileZone zone) {
  if (zone < 0 || zone >= ZONE_COUNT || !s_state) return;
  const int64_t nowUs = esp_timer_get_time();
  portENTER_CRITICAL(&s_profMux);
  const int64_t startUs = s_state->zoneStartUs[zone];
  if (startUs >= 0 && nowUs >= startUs) {
    const int64_t delta = nowUs - startUs;
    const uint32_t elapsedUs = (delta > 0xFFFFFFFFLL) ? 0xFFFFFFFFU : static_cast<uint32_t>(delta);
    BmoProfilerCore::recordZoneElapsed(*s_state, zone, elapsedUs);
  }
  s_state->zoneStartUs[zone] = -1;
  portEXIT_CRITICAL(&s_profMux);
}

bool BmoProfiler::getFrameSummary(BmoProfilerTypes::FrameSummary& out) {
  if (!s_state || !s_sortScratch) {
    out = {};
    return false;
  }
  uint32_t n = 0U;
  uint32_t totalFrames = 0U;
  portENTER_CRITICAL(&s_profMux);
  copyFrameRingToScratchLocked(n, totalFrames);
  portEXIT_CRITICAL(&s_profMux);
  return BmoProfilerCore::summarizeCopiedSamples(n, totalFrames, s_sortScratch, BmoProfilerCore::FRAME_RING_CAPACITY,
                                                 out);
}

void BmoProfiler::reportIfInterval(unsigned long intervalMs, int emuIndex, uint32_t targetUs, uint32_t fps,
                                   uint32_t droppedFrames) {
  if (!s_state || !s_sortScratch) return;
  const int64_t nowUs = esp_timer_get_time();
  const int64_t intervalUs = static_cast<int64_t>(intervalMs) * 1000LL;

  ZoneReportSnapshot snapshot{};
  bool emitHeader = false;
  portENTER_CRITICAL(&s_profMux);
  if ((nowUs - s_lastReportUs) < intervalUs) {
    portEXIT_CRITICAL(&s_profMux);
    return;
  }
  memcpy(snapshot.zoneMinUs, s_state->zoneMinUs, sizeof(snapshot.zoneMinUs));
  memcpy(snapshot.zoneMaxUs, s_state->zoneMaxUs, sizeof(snapshot.zoneMaxUs));
  memcpy(snapshot.zoneTotalUs, s_state->zoneTotalUs, sizeof(snapshot.zoneTotalUs));
  memcpy(snapshot.zoneSamples, s_state->zoneSamples, sizeof(snapshot.zoneSamples));
  copyFrameRingToScratchLocked(snapshot.ringCount, snapshot.totalFrames);
  BmoProfilerCore::resetZones(*s_state);
  s_lastReportUs = nowUs;
  if (!s_csvHeaderPrinted) {
    emitHeader = true;
    s_csvHeaderPrinted = true;
  }
  portEXIT_CRITICAL(&s_profMux);

  BmoProfilerTypes::FrameSummary summary;
  const bool hasFrames = BmoProfilerCore::summarizeCopiedSamples(
      snapshot.ringCount, snapshot.totalFrames, s_sortScratch, BmoProfilerCore::FRAME_RING_CAPACITY, summary);

  uint32_t spiAvgUs = 0U;
  if (snapshot.zoneSamples[ZONE_SPI_STREAM] > 0U) {
    spiAvgUs = static_cast<uint32_t>(snapshot.zoneTotalUs[ZONE_SPI_STREAM] / snapshot.zoneSamples[ZONE_SPI_STREAM]);
  }

  LOG_INFO_STR("--- PROFILER TELEMETRY REPORT ---");
  for (int i = 0; i < ZONE_COUNT; ++i) {
    if (snapshot.zoneSamples[i] == 0U) continue;
    const uint32_t avgUs = static_cast<uint32_t>(snapshot.zoneTotalUs[i] / snapshot.zoneSamples[i]);
    LOG_INFO("Zone [%-15s]: Avg=%5u us | Min=%5u us | Max=%5u us | Samples=%u", kZoneNames[i], (unsigned int)avgUs,
             (unsigned int)snapshot.zoneMinUs[i], (unsigned int)snapshot.zoneMaxUs[i],
             (unsigned int)snapshot.zoneSamples[i]);
  }

  if (hasFrames) {
    if (emitHeader) {
      Serial.println(BmoProfilerCore::csvHeaderLine());
    }
    char csvLine[160];
    if (BmoProfilerCore::formatCsvRow(csvLine, sizeof(csvLine), emuIndex, targetUs, fps, droppedFrames, summary,
                                      spiAvgUs)) {
      Serial.println(csvLine);
    }
  }
}

uint32_t BmoProfiler::getZoneAvgUs(ProfileZone zone) {
  if (zone < 0 || zone >= ZONE_COUNT || !s_state) return 0U;
  portENTER_CRITICAL(&s_profMux);
  const uint32_t samples = s_state->zoneSamples[zone];
  const uint64_t total = s_state->zoneTotalUs[zone];
  portEXIT_CRITICAL(&s_profMux);
  if (samples == 0U) return 0U;
  return static_cast<uint32_t>(total / samples);
}

uint32_t BmoProfiler::getZoneMinUs(ProfileZone zone) {
  if (zone < 0 || zone >= ZONE_COUNT || !s_state) return 0U;
  portENTER_CRITICAL(&s_profMux);
  const uint32_t minVal = s_state->zoneMinUs[zone];
  portEXIT_CRITICAL(&s_profMux);
  return (minVal == 0xFFFFFFFFU) ? 0U : minVal;
}

uint32_t BmoProfiler::getZoneMaxUs(ProfileZone zone) {
  if (zone < 0 || zone >= ZONE_COUNT || !s_state) return 0U;
  portENTER_CRITICAL(&s_profMux);
  const uint32_t maxVal = s_state->zoneMaxUs[zone];
  portEXIT_CRITICAL(&s_profMux);
  return maxVal;
}

#endif
