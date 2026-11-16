// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "app_diagnostics.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../hal/board_timing.h"
#include "../hal/buttons.h"
#include "../storage/save_manager.h"
#include "../storage/sd_card.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include "app_state.h"

namespace {

void triggerDiagnosticsSdRefresh(BmoAppRuntime::RuntimeState& rt) {
  if (!SDCard::isMounted()) {
    const bool mounted = SDCard::begin();
    if (mounted) {
      SaveManager::begin();
      BmoAppRuntime::syncVisiblePoolCapacity();
      rt.consoleCountsDirty = true;
      rt.visibleGamesDirty = true;
    }
    DisplayEmu::showToast(mounted ? "SD CARD RETRY SUCCEEDED" : "SD CARD STILL NOT FOUND", Theme::BMO_DPAD_YELLOW,
                          3000);
    return;
  }

  DisplayEmu::showToast("RESCANNING SD LIBRARY", Theme::BMO_DPAD_YELLOW, 10000);
  SDCard::rebuildIndex();
  BmoAppRuntime::syncVisiblePoolCapacity();
  rt.consoleCountsDirty = true;
  rt.visibleGamesDirty = true;
  DisplayEmu::showToast(SDCard::getSkippedRomCount() == 0 ? "LIBRARY RESCAN COMPLETE"
                                                          : "RESCAN COMPLETE; SOME PATHS SKIPPED",
                        Theme::BMO_DPAD_YELLOW, 3000);
}

void logDiagnosticsStackWatermark(uint32_t loopHwm, uint32_t displayHwm) {
  static unsigned long s_lastStackLogMs = 0;
  if (millis() - s_lastStackLogMs >= 5000UL) {
    LOG_INFO("[STACK] loopTask HWM=%u words, displayTask HWM=%u words", static_cast<unsigned int>(loopHwm),
             static_cast<unsigned int>(displayHwm));
    s_lastStackLogMs = millis();
  }
}

void renderDiagnosticsView(BmoAppRuntime::RuntimeState& rt, bool spiStressFallback, uint16_t btnMask) {
  const uint32_t freeDram = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  const uint32_t freePsram = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  const uint32_t largestPsramBlock = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  const uint32_t freeIram = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT));
  const uint32_t loopHwm = static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
  const uint32_t displayHwm = DisplayEmu::getDisplayStackHighWaterMarkWords();
  logDiagnosticsStackWatermark(loopHwm, displayHwm);

  if (rt.spiStressPattern) {
    DisplayEmu::drawSpiStressPattern(spiStressFallback ? TFT_SPI_FALLBACK_HZ : TFT_SPI_HZ, rt.spiStressFrame++);
  } else if (rt.diagShowProfiler) {
    DisplayEmu::drawDiagnosticsProfiler(millis(), freePsram, largestPsramBlock);
  } else {
    DisplayEmu::drawDiagnosticsDashboard(millis(), freeDram, freePsram, freeIram, btnMask, largestPsramBlock,
                                         static_cast<uint32_t>(SDCard::getSkippedRomCount()), loopHwm, displayHwm,
                                         SDCard::isMounted(), SDCard::getClockHz());
  }
}

} // namespace

namespace BmoAppDiagnostics {

void handleDiagnostics() {
  BmoAppRuntime::RuntimeState& rt = BmoAppRuntime::state();
  const unsigned long diagFrameStart = millis();

  DisplayEmu::initMenuUI();
  Buttons::update(false);
  const auto& input = Buttons::inputFrame();
  const uint16_t btnMask = Buttons::rawMask();
  bool spiStressFallback = (DisplayEmu::getDiagnosticSpiClock() == TFT_SPI_FALLBACK_HZ);

  if (input.edgePressed(Buttons::B)) {
    rt.spiStressPattern = false;
    rt.diagShowProfiler = false;
    rt.currentState = BmoApp::STATE_CONSOLE_MENU;
    Buttons::suppressMenuInputUntil(static_cast<uint32_t>(millis() + 180U));
  } else if (input.edgePressed(Buttons::A)) {
    rt.spiStressPattern = !rt.spiStressPattern;
  } else if (rt.spiStressPattern && (input.edgePressed(Buttons::LEFT) || input.edgePressed(Buttons::RIGHT))) {
    spiStressFallback = !spiStressFallback;
    const uint32_t nextSpiHz = spiStressFallback ? TFT_SPI_FALLBACK_HZ : TFT_SPI_HZ;
    DisplayEmu::setDiagnosticSpiClock(nextSpiHz);
    BmoAppRuntime::persistDisplaySpiClock(nextSpiHz);
  } else if (!rt.spiStressPattern && input.edgePressed(Buttons::SELECT)) {
    rt.diagShowProfiler = !rt.diagShowProfiler;
  } else if (!rt.spiStressPattern && input.edgePressed(Buttons::START)) {
    triggerDiagnosticsSdRefresh(rt);
  }

  renderDiagnosticsView(rt, spiStressFallback, btnMask);
  BmoAppRuntime::paceMenuFrame(diagFrameStart);
}

} // namespace BmoAppDiagnostics
