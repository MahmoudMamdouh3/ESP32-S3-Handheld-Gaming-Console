#include "display_internal.h"
#include "../hal/battery.h"
#include "../hal/config.h"
#include "../hal/profiler.h"
#include <Fonts/FreeSans9pt7b.h>
#include <esp_heap_caps.h>

void DisplayEmu::drawDiagnosticsDashboard(unsigned long uptimeMs, uint32_t freeDram, uint32_t freePsram,
                                          uint32_t freeIram, uint16_t buttonMask, uint32_t largestPsramBlock,
                                          uint32_t skippedRomCount, uint32_t loopStackHighWaterWords,
                                          uint32_t displayStackHighWaterWords, bool sdMounted, uint32_t sdClockHz) {
  using namespace BmoDisplayInternal;
  if (!menuCanvas) return;
#if defined(ESP32) || defined(ARDUINO)
  if (largestPsramBlock == 0) {
    largestPsramBlock = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  }
#endif
  menuCanvas->fillScreen(UI_BLACK);
  menuCanvas->drawRoundRect(8, 8, 304, 224, 8, UI_TEAL);
  menuCanvas->fillRect(0, 0, 320, 36, UI_DEEP_TEAL);

  menuCanvas->setFont(&FreeSans9pt7b);
  drawCentered("HARDWARE DIAGNOSTICS", 24, UI_YELLOW);

  menuCanvas->setFont();
  menuCanvas->setTextColor(UI_WHITE);

  menuCanvas->setCursor(18, 46);
  if (TFT_SPI_HZ > 62500000UL) {
    menuCanvas->printf("TFT %lu MHz (> 62.5 MHz datasheet limit,", (unsigned long)(TFT_SPI_HZ / 1000000UL));
    menuCanvas->setCursor(18, 58);
    menuCanvas->print("unmeasured)");
  } else {
    menuCanvas->printf("TFT %lu MHz (within published limit; unmeasured)", (unsigned long)(TFT_SPI_HZ / 1000000UL));
  }
  menuCanvas->setCursor(18, 70);
  const unsigned long sec = uptimeMs / 1000;
  if (sdMounted) {
    menuCanvas->printf("Uptime: %lum %lus  |  SD: MOUNTED %luMHz", sec / 60, sec % 60,
                       (unsigned long)(sdClockHz / 1000000UL));
  } else {
    menuCanvas->printf("Uptime: %lum %lus  |  SD: NOT MOUNTED", sec / 60, sec % 60);
  }

  menuCanvas->setCursor(18, 82);
  menuCanvas->setTextColor(UI_YELLOW);
  menuCanvas->print("--- MEMORY TELEMETRY ---");
  menuCanvas->setTextColor(UI_WHITE);

  menuCanvas->setCursor(18, 94);
  menuCanvas->printf("Internal DRAM Free : %6u / 327,680 B (%u%%)", (unsigned int)freeDram,
                     (unsigned int)(freeDram * 100 / 327680));
  menuCanvas->setCursor(18, 106);
  menuCanvas->printf("PSRAM Free / MaxBlk: %6u / %6u B", (unsigned int)freePsram, (unsigned int)largestPsramBlock);
  menuCanvas->setCursor(18, 118);
  menuCanvas->printf("IRAM Execution Free: %6u B", (unsigned int)freeIram);
  menuCanvas->setCursor(18, 126);
#if FEATURE_BATTERY_MONITOR
  menuCanvas->printf("Battery: %d%% (%d mV)", Battery::getPercentage(), Battery::getVoltageMv());
#else
  menuCanvas->print("Battery: NOT FITTED");
#endif
  menuCanvas->setCursor(148, 126);
  menuCanvas->printf("HWM loop/disp: %u/%u w", (unsigned int)loopStackHighWaterWords,
                     (unsigned int)displayStackHighWaterWords);

  menuCanvas->setCursor(18, 134);
  menuCanvas->setTextColor(UI_YELLOW);
  menuCanvas->print("--- LIVE BUTTON MATRIX TESTER ---");
  menuCanvas->setTextColor(UI_WHITE);
  menuCanvas->setCursor(18, 142);
  menuCanvas->printf("BTN MASK: %03X | ROM paths skipped: %u", (unsigned int)(buttonMask & 0x0FFFU),
                     (unsigned int)skippedRomCount);

  struct KeyBox {
    int x;
    int y;
    int w;
    int h;
    int tx;
    int ty;
    int bit;
    const char* label;
  };
  static constexpr KeyBox kKeys[] = {
      {42, 146, 16, 14, 47, 157, 0, "U"},     {24, 162, 16, 14, 29, 173, 2, "L"},
      {60, 162, 16, 14, 65, 173, 3, "R"},     {42, 178, 16, 14, 47, 189, 1, "D"},
      {104, 168, 36, 16, 110, 180, 6, "SEL"}, {148, 168, 36, 16, 154, 180, 7, "STA"},
      {212, 166, 22, 20, 219, 180, 5, "B"},   {244, 154, 22, 20, 251, 168, 4, "A"},
      {24, 196, 26, 14, 29, 207, 10, "LT"},   {54, 196, 26, 14, 59, 207, 11, "RT"},
      {212, 196, 22, 14, 219, 207, 9, "Y"},   {244, 196, 22, 14, 251, 207, 8, "X"},
  };
  for (const auto& k : kKeys) {
    const bool pressed = (buttonMask & (1U << k.bit)) != 0U;
    menuCanvas->fillRect(k.x, k.y, k.w, k.h, pressed ? UI_YELLOW : UI_DEEP_TEAL);
    menuCanvas->drawRect(k.x, k.y, k.w, k.h, UI_WHITE);
    menuCanvas->setCursor(k.tx, k.ty);
    menuCanvas->setTextColor(pressed ? UI_BLACK : UI_WHITE);
    menuCanvas->print(k.label);
  }

  drawFooter("A SPI | SEL PROF | B EXIT");
  writeMenuCanvas();
}

void DisplayEmu::drawDiagnosticsProfiler(unsigned long uptimeMs, uint32_t freePsram, uint32_t largestPsramBlock) {
  using namespace BmoDisplayInternal;
  if (!menuCanvas) return;

  menuCanvas->fillScreen(UI_BLACK);
  menuCanvas->drawRoundRect(8, 8, 304, 224, 8, UI_TEAL);
  menuCanvas->fillRect(0, 0, 320, 36, UI_DEEP_TEAL);

  menuCanvas->setFont(&FreeSans9pt7b);
  drawCentered("FRAME PROFILER", 24, UI_YELLOW);

  menuCanvas->setFont();
  menuCanvas->setTextColor(UI_WHITE);
  menuCanvas->setCursor(18, 46);
  menuCanvas->printf("Build Flag : %s", BmoProfiler::isEnabled() ? "BMO_PROFILE=1 (ACTIVE)" : "BMO_PROFILE=0 (OFF)");
  menuCanvas->setCursor(18, 58);
  const unsigned long sec = uptimeMs / 1000UL;
  menuCanvas->printf("Uptime: %lum %lus | PSRAM: %u / %u B", sec / 60UL, sec % 60UL, (unsigned int)freePsram,
                     (unsigned int)largestPsramBlock);

  BmoProfilerTypes::FrameSummary summary;
  const bool hasSummary = BmoProfiler::getFrameSummary(summary);

  menuCanvas->setCursor(18, 76);
  menuCanvas->setTextColor(UI_YELLOW);
  menuCanvas->print("--- 600-FRAME WINDOW (EMU_FRAME) ---");
  menuCanvas->setTextColor(UI_WHITE);
  if (hasSummary) {
    menuCanvas->setCursor(18, 88);
    menuCanvas->printf("Samples: %u / 600 (total %u)", (unsigned int)summary.sampleCount,
                       (unsigned int)summary.totalFrames);
    menuCanvas->setCursor(18, 100);
    menuCanvas->printf("Min / Avg / Max: %u / %u / %u us", (unsigned int)summary.minUs, (unsigned int)summary.avgUs,
                       (unsigned int)summary.maxUs);
    menuCanvas->setCursor(18, 112);
    menuCanvas->printf("p50 / p95 / p99: %u / %u / %u us", (unsigned int)summary.p50Us, (unsigned int)summary.p95Us,
                       (unsigned int)summary.p99Us);
  } else {
    menuCanvas->setCursor(18, 88);
    menuCanvas->print(BmoProfiler::isEnabled() ? "No emulator frames sampled yet."
                                               : "Compile with -DBMO_PROFILE=1 for telemetry.");
    menuCanvas->setCursor(18, 100);
    menuCanvas->print("Min / Avg / Max: N/A (NOT_TESTED)");
    menuCanvas->setCursor(18, 112);
    menuCanvas->print("p50 / p95 / p99: N/A (NOT_TESTED)");
  }

  menuCanvas->setCursor(18, 130);
  menuCanvas->setTextColor(UI_YELLOW);
  menuCanvas->print("--- ACTIVE PROFILER ZONES (us) ---");
  menuCanvas->setTextColor(UI_WHITE);

  const char* kLabels[ZONE_COUNT] = {"EMU_FRAME", "SPI_STREAM", "BMO_FACE", "MENU_DRAW", "SD_READ"};
  for (int i = 0; i < ZONE_COUNT; ++i) {
    const ProfileZone z = static_cast<ProfileZone>(i);
    menuCanvas->setCursor(18, 144 + i * 13);
    menuCanvas->printf("%-10s min=%5u avg=%5u max=%5u", kLabels[i], (unsigned int)BmoProfiler::getZoneMinUs(z),
                       (unsigned int)BmoProfiler::getZoneAvgUs(z), (unsigned int)BmoProfiler::getZoneMaxUs(z));
  }

  drawFooter("SEL MATRIX  |  B EXIT");
  writeMenuCanvas();
}

void DisplayEmu::drawSpiStressPattern(uint32_t spiClockHz, uint32_t frameNumber) {
  using namespace BmoDisplayInternal;
  if (!menuCanvas) return;

  static const uint16_t patternColors[] = {UI_BLACK, UI_WHITE, UI_MINT, UI_DEEP_TEAL, UI_YELLOW, UI_CORAL, UI_BLUE};
  constexpr size_t colorCount = sizeof(patternColors) / sizeof(patternColors[0]);
  const uint32_t phase = frameNumber & 1U;

  for (int y = 0; y < 240; y += 20) {
    for (int x = 0; x < 320; x += 20) {
      const size_t colorIndex = (size_t)((x / 20 + y / 20 + phase) % colorCount);
      menuCanvas->fillRect(x, y, 20, 20, patternColors[colorIndex]);
    }
  }

  menuCanvas->fillRect(0, 92, 320, 56, UI_BLACK);
  menuCanvas->setFont();
  menuCanvas->setTextColor(UI_WHITE);
  menuCanvas->setCursor(88, 112);
  menuCanvas->print("SPI STRESS PATTERN");
  menuCanvas->setTextColor(UI_YELLOW);
  menuCanvas->setCursor(104, 130);
  menuCanvas->printf("CLOCK: %u MHz", (unsigned int)(spiClockHz / 1000000U));
  drawFooter("LEFT/RIGHT CLOCK  |  A RETURN  |  B EXIT");
  writeMenuCanvas();
}
