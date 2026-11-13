#include "profiler.h"
#include "test_check.h"

#include <string.h>

int main() {
  BmoProfilerCore::ProfilerState state;
  BmoProfilerCore::resetAll(state);
  uint32_t scratch[BmoProfilerCore::FRAME_RING_CAPACITY];

  BmoProfilerTypes::FrameSummary emptySummary;
  BMO_CHECK(!BmoProfilerCore::summarizeFrames(state, emptySummary, scratch, BmoProfilerCore::FRAME_RING_CAPACITY));
  BMO_CHECK(
      strcmp(BmoProfilerCore::csvHeaderLine(),
             "[PROFILE_CSV_HDR] emu,n,target_us,fps,min_us,avg_us,max_us,p50_us,p95_us,p99_us,dropped,spi_avg_us") ==
      0);

  // Fill exact 600-frame window with values 1001..1600 us
  for (uint32_t i = 1; i <= 600U; ++i) {
    BmoProfilerCore::recordZoneElapsed(state, ZONE_EMU_FRAME, 1000U + i);
  }
  BmoProfilerTypes::FrameSummary s600;
  BMO_CHECK(BmoProfilerCore::summarizeFrames(state, s600, scratch, BmoProfilerCore::FRAME_RING_CAPACITY));
  BMO_CHECK(s600.sampleCount == 600U);
  BMO_CHECK(s600.totalFrames == 600U);
  BMO_CHECK(s600.minUs == 1001U);
  BMO_CHECK(s600.maxUs == 1600U);
  BMO_CHECK(s600.avgUs == 1300U); // floor(780300 / 600)
  BMO_CHECK(s600.p50Us == 1300U);
  BMO_CHECK(s600.p95Us == 1570U);
  BMO_CHECK(s600.p99Us == 1594U);

  // Advance 400 more frames (2001..2400 us) -> ring wraps cleanly keeping 1401..1600 and 2001..2400
  for (uint32_t i = 1; i <= 400U; ++i) {
    BmoProfilerCore::recordZoneElapsed(state, ZONE_EMU_FRAME, 2000U + i);
  }
  BmoProfilerTypes::FrameSummary sWrap;
  BMO_CHECK(BmoProfilerCore::summarizeFrames(state, sWrap, scratch, BmoProfilerCore::FRAME_RING_CAPACITY));
  BMO_CHECK(sWrap.sampleCount == 600U);
  BMO_CHECK(sWrap.totalFrames == 1000U);
  BMO_CHECK(sWrap.minUs == 1401U);
  BMO_CHECK(sWrap.maxUs == 2400U);
  BMO_CHECK(sWrap.p50Us == 2100U); // 200 items in 1401..1600, 100th in 2001..2400 -> 2100
  BMO_CHECK(sWrap.p95Us == 2370U);
  BMO_CHECK(sWrap.p99Us == 2394U);

  // Format CSV row
  char csv[160];
  BMO_CHECK(BmoProfilerCore::formatCsvRow(csv, sizeof(csv), 1, 16742U, 60U, 0U, sWrap, 3120U));
  BMO_CHECK(strcmp(csv, "[PROFILE_CSV] 1,600,16742,60,1401,1967,2400,2100,2370,2394,0,3120") == 0);

  // Zone accumulation & resetZones preserve the sliding frame ring while resetting per-second zones
  BmoProfilerCore::recordZoneElapsed(state, ZONE_SPI_STREAM, 2500U);
  BmoProfilerCore::recordZoneElapsed(state, ZONE_SPI_STREAM, 3500U);
  BMO_CHECK(state.zoneSamples[ZONE_SPI_STREAM] == 2U);
  BMO_CHECK(state.zoneMinUs[ZONE_SPI_STREAM] == 2500U);
  BMO_CHECK(state.zoneMaxUs[ZONE_SPI_STREAM] == 3500U);
  BMO_CHECK(state.zoneTotalUs[ZONE_SPI_STREAM] == 6000ULL);
  BmoProfilerCore::resetZones(state);
  BMO_CHECK(state.zoneSamples[ZONE_SPI_STREAM] == 0U);
  BMO_CHECK(state.ringCount == 600U);

  return 0;
}
