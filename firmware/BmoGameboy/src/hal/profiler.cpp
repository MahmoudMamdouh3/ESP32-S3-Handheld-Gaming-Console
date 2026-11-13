#include "profiler.h"

#if FEATURE_PROFILER

#include <Arduino.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

static BmoProfilerCore::ProfilerState s_state;
static uint32_t s_sortScratch[BmoProfilerCore::FRAME_RING_CAPACITY];
static int64_t s_lastReportUs = 0;
static bool s_csvHeaderPrinted = false;
static portMUX_TYPE s_profMux = portMUX_INITIALIZER_UNLOCKED;

static const char* kZoneNames[ZONE_COUNT] = {
    "EMU_FRAME", "SPI_STREAM", "BMO_FACE_RENDER", "MENU_RENDER", "SD_READ",
};

} // namespace

void BmoProfiler::begin() {
  reset();
}

void BmoProfiler::reset() {
  portENTER_CRITICAL(&s_profMux);
  BmoProfilerCore::resetAll(s_state);
  s_lastReportUs = esp_timer_get_time();
  s_csvHeaderPrinted = false;
  portEXIT_CRITICAL(&s_profMux);
}

void BmoProfiler::startZone(ProfileZone zone) {
  if (zone < 0 || zone >= ZONE_COUNT) return;
  const int64_t nowUs = esp_timer_get_time();
  portENTER_CRITICAL(&s_profMux);
  s_state.zoneStartUs[zone] = nowUs;
  portEXIT_CRITICAL(&s_profMux);
}

void BmoProfiler::endZone(ProfileZone zone) {
  if (zone < 0 || zone >= ZONE_COUNT) return;
  const int64_t nowUs = esp_timer_get_time();
  portENTER_CRITICAL(&s_profMux);
  const int64_t startUs = s_state.zoneStartUs[zone];
  if (startUs >= 0 && nowUs >= startUs) {
    const int64_t delta = nowUs - startUs;
    const uint32_t elapsedUs = (delta > 0xFFFFFFFFLL) ? 0xFFFFFFFFU : static_cast<uint32_t>(delta);
    BmoProfilerCore::recordZoneElapsed(s_state, zone, elapsedUs);
  }
  s_state.zoneStartUs[zone] = -1;
  portEXIT_CRITICAL(&s_profMux);
}

bool BmoProfiler::getFrameSummary(BmoProfilerTypes::FrameSummary& out) {
  BmoProfilerCore::ProfilerState snapshot;
  portENTER_CRITICAL(&s_profMux);
  snapshot = s_state;
  portEXIT_CRITICAL(&s_profMux);
  return BmoProfilerCore::summarizeFrames(snapshot, out, s_sortScratch, BmoProfilerCore::FRAME_RING_CAPACITY);
}

void BmoProfiler::reportIfInterval(unsigned long intervalMs, int emuIndex, uint32_t targetUs, uint32_t fps,
                                   uint32_t droppedFrames) {
  const int64_t nowUs = esp_timer_get_time();
  const int64_t intervalUs = static_cast<int64_t>(intervalMs) * 1000LL;

  BmoProfilerCore::ProfilerState snapshot;
  bool emitHeader = false;
  portENTER_CRITICAL(&s_profMux);
  if ((nowUs - s_lastReportUs) < intervalUs) {
    portEXIT_CRITICAL(&s_profMux);
    return;
  }
  snapshot = s_state;
  BmoProfilerCore::resetZones(s_state);
  s_lastReportUs = nowUs;
  if (!s_csvHeaderPrinted) {
    emitHeader = true;
    s_csvHeaderPrinted = true;
  }
  portEXIT_CRITICAL(&s_profMux);

  BmoProfilerTypes::FrameSummary summary;
  const bool hasFrames =
      BmoProfilerCore::summarizeFrames(snapshot, summary, s_sortScratch, BmoProfilerCore::FRAME_RING_CAPACITY);

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
  if (zone < 0 || zone >= ZONE_COUNT) return 0U;
  portENTER_CRITICAL(&s_profMux);
  const uint32_t samples = s_state.zoneSamples[zone];
  const uint64_t total = s_state.zoneTotalUs[zone];
  portEXIT_CRITICAL(&s_profMux);
  if (samples == 0U) return 0U;
  return static_cast<uint32_t>(total / samples);
}

uint32_t BmoProfiler::getZoneMinUs(ProfileZone zone) {
  if (zone < 0 || zone >= ZONE_COUNT) return 0U;
  portENTER_CRITICAL(&s_profMux);
  const uint32_t minVal = s_state.zoneMinUs[zone];
  portEXIT_CRITICAL(&s_profMux);
  return (minVal == 0xFFFFFFFFU) ? 0U : minVal;
}

uint32_t BmoProfiler::getZoneMaxUs(ProfileZone zone) {
  if (zone < 0 || zone >= ZONE_COUNT) return 0U;
  portENTER_CRITICAL(&s_profMux);
  const uint32_t maxVal = s_state.zoneMaxUs[zone];
  portEXIT_CRITICAL(&s_profMux);
  return maxVal;
}

#endif
