// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "bmo_app.h"

#include <Arduino.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "../hal/battery.h"
#include "../hal/board_timing.h"
#include "../hal/buttons.h"
#include "../hal/feature_flags.h"
#include "../hal/pin_map.h"
#include "../hal/profiler.h"
#include "../hal/spi_arbiter.h"
#include "../storage/save_manager.h"
#include "../storage/sd_card.h"
#ifdef ENABLE_UNIT_TESTS
#include "../tests/unit_tests.h"
#endif
#include "../ui/bmo_face.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include "app_diagnostics.h"
#include "app_emulator_loop.h"
#include "app_menus.h"
#include "app_state.h"
#include "boot_safe_mode.h"
#include "version.h"

namespace {

void logBootBannerAndResetReason(int* outResetReason, bool* outAbnormalReset) {
  LOG_INFO_STR("\n\n--- BOOTING ---");
  LOG_INFO("BMO Handheld v%s (%s)", BMO_VERSION_STRING, BMO_GIT_SHA);
  const int resetReason = static_cast<int>(esp_reset_reason());
  const bool abnormalReset = BmoApp::isAbnormalResetReason(resetReason);
  const BmoApp::BootBreadcrumb prevBreadcrumb = BmoAppRuntime::readBootBreadcrumb();
  LOG_INFO("Reset reason: %s (%d)", BmoApp::resetReasonName(resetReason), resetReason);
  if (abnormalReset &&
      BmoApp::isValidBootBreadcrumb(prevBreadcrumb, BmoApp::kBreadcrumbMaxStateCount, static_cast<int8_t>(15))) {
    LOG_WARN("Crash/brownout recovery breadcrumb: prev_state=%u prev_emu=%d",
             static_cast<unsigned>(prevBreadcrumb.state), static_cast<int>(prevBreadcrumb.emulatorIndex));
  } else if (abnormalReset) {
    LOG_WARN("Abnormal reset with invalid/cold-boot RTC breadcrumb (magic=0x%08lX)",
             static_cast<unsigned long>(prevBreadcrumb.magic));
  }
  BmoAppRuntime::updateBootBreadcrumb();
  LOG_INFO_STR("Milestone 4: Game Selection UI");
  LOG_INFO("Features: SD=%d, Audio=%d, Battery=%d", FEATURE_SD_CARD, FEATURE_AUDIO, FEATURE_BATTERY_MONITOR);
  *outResetReason = resetReason;
  *outAbnormalReset = abnormalReset;
}

void initDisplayAndSafeMode(int resetReason, bool abnormalReset) {
  Buttons::begin();
  Buttons::update(false);
  const BmoApp::BootSpiDecision spiDecision =
      BmoApp::resolveBootDisplaySpiClock(Buttons::get(Buttons::SELECT).pressed, Buttons::get(Buttons::B).pressed,
                                         BmoAppRuntime::loadSavedDisplaySpiClock(), TFT_SPI_HZ, TFT_SPI_FALLBACK_HZ);

  if (spiDecision.safeModeForced) {
    BmoAppRuntime::persistDisplaySpiClock(TFT_SPI_FALLBACK_HZ);
    LOG_WARN("[SAFE_MODE] SELECT/B held at boot; forced display SPI clock to %lu MHz",
             static_cast<unsigned long>(TFT_SPI_FALLBACK_HZ / 1000000UL));
  }
  DisplayEmu::begin(spiDecision.spiHz);
  if (spiDecision.safeModeForced) {
    DisplayEmu::showToast("SAFE MODE: 40 MHz SPI", Theme::BMO_DPAD_YELLOW, 3500);
  } else if (abnormalReset) {
    DisplayEmu::showToast(BmoApp::resetReasonName(resetReason), Theme::BMO_CORAL_RED, 3500);
  }
}

void mountStorageAndLogMemory() {
  BMO_PROFILE_START(ZONE_SD_READ);
  const bool sdOk = SDCard::begin();
  BMO_PROFILE_END(ZONE_SD_READ);
  if (!sdOk) {
    LOG_ERROR_STR("Failed to mount SD card!");
  } else {
    LOG_INFO("SD Card mounted. Found %d ROMs.", SDCard::getRomCount());
    SaveManager::begin();
  }
  BmoAppRuntime::syncVisiblePoolCapacity();

  LOG_INFO("PSRAM boot state: free=%u B, largest_block=%u B, pool_cap=%d",
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
           static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)), SDCard::getPoolCapacity());
  LOG_INFO("IRAM free: %u bytes", heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT));
}

} // namespace

void BmoApp::setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);

#if defined(CORE_DEBUG_LEVEL) && CORE_DEBUG_LEVEL > 0
  delay(3000);
#else
  delay(500);
#endif

  int resetReason = 0;
  bool abnormalReset = false;
  logBootBannerAndResetReason(&resetReason, &abnormalReset);

  SpiArbiter::init();
  SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, -1);

#ifdef ENABLE_UNIT_TESTS
  LOG_INFO_STR("Booting into Test Mode...");
  const bool allPassed = runAllTests();
  LOG_INFO_STR(allPassed ? "TESTS SUCCESS" : "TESTS FAILED");
  while (true)
    delay(100);
#endif

  initDisplayAndSafeMode(resetReason, abnormalReset);
  BMO_PROFILE_BEGIN();
  Battery::begin();
  BmoFace::begin();
  BmoFace::setExpression(BmoFace::IDLE);
  BmoAppRuntime::state().bootSplashStartMs = millis();

  mountStorageAndLogMemory();
  BmoAppRuntime::state().lastFpsLogMs = millis();
}

void BmoApp::loop() {
  BmoAppRuntime::RuntimeState& rt = BmoAppRuntime::state();
  BmoAppRuntime::updateBootBreadcrumb();
  Battery::update();
  Buttons::setGameplayMode(rt.currentState == BmoApp::STATE_EMULATOR);
  if (rt.currentState != BmoApp::STATE_EMULATOR && rt.currentState != BmoApp::STATE_PAUSE_MENU) {
    BMO_PROFILE_START(ZONE_BMO_FACE_RENDER);
    BmoFace::update();
    BMO_PROFILE_END(ZONE_BMO_FACE_RENDER);
  }

  switch (rt.currentState) {
  case BmoApp::STATE_BOOT_SPLASH:
    BmoAppMenus::handleBootSplash();
    break;
  case BmoApp::STATE_CONSOLE_MENU:
    BmoAppMenus::handleConsoleMenu();
    break;
  case BmoApp::STATE_CONSOLE_MUSEUM:
    BmoAppMenus::handleConsoleMuseum();
    break;
  case BmoApp::STATE_GAME_MENU:
    BmoAppMenus::handleGameMenu();
    break;
  case BmoApp::STATE_EMULATOR:
    BmoAppEmulatorLoop::handleEmulator();
    break;
  case BmoApp::STATE_PAUSE_MENU:
    BmoAppEmulatorLoop::handlePauseMenu();
    break;
  case BmoApp::STATE_DIAGNOSTICS:
    BmoAppDiagnostics::handleDiagnostics();
    break;
  case BmoApp::STATE_IDLE_MASCOT:
    BmoAppMenus::handleIdleMascot();
    break;
  }
}
