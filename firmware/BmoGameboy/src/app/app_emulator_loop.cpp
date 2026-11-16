// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "app_emulator_loop.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#include "../cores/core_registry.h"
#include "../cores/gb_common.h"
#include "../hal/buttons.h"
#include "../hal/profiler.h"
#include "../storage/save_manager.h"
#include "../ui/bmo_face.h"
#include "../ui/display_emu.h"
#include "../ui/frame_pacing.h"
#include "../ui/pause_quit_policy.h"
#include "../ui/theme.h"
#include "app_state.h"

namespace {

bool processLaunchCelebration(BmoAppRuntime::RuntimeState& rt) {
  if (!rt.launchCelebrationPending) return false;
  Buttons::update(false);
  if (static_cast<unsigned long>(millis() - rt.launchCelebrationStartMs) < 400UL) {
    vTaskDelay(pdMS_TO_TICKS(16));
    return true;
  }
  rt.launchCelebrationPending = false;
  DisplayEmu::drawConsoleBezel(rt.selectedEmulatorIndex);
  BMO_PROFILE_RESET();
  rt.lastFpsLogMs = millis();
  return true;
}

void enterEmulatorPause(BmoAppRuntime::RuntimeState& rt, int option, const char* initialToast) {
  rt.currentState = BmoApp::STATE_PAUSE_MENU;
  rt.pauseOption = option;
  if (initialToast) {
    snprintf(rt.pauseToast, sizeof(rt.pauseToast), "%s", initialToast);
  } else {
    rt.pauseToast[0] = '\0';
  }
  rt.pauseMetaDirty = true;
  Buttons::suppressMenuInputUntil(static_cast<uint32_t>(millis() + 250U));
}

bool processEmulatorHotkeys(BmoAppRuntime::RuntimeState& rt) {
  if (rt.hotkeyCooldownActive && static_cast<unsigned long>(millis() - rt.hotkeyCooldownStartMs) >= 250UL) {
    rt.hotkeyCooldownActive = false;
  }
  if (rt.hotkeyCooldownActive) return false;

  const bool select = Buttons::get(Buttons::SELECT).pressed;
  if (!select) return false;
  const bool start = Buttons::get(Buttons::START).pressed && Buttons::get(Buttons::START).changed;
  const bool btnA = Buttons::get(Buttons::A).pressed && Buttons::get(Buttons::A).changed;
  const bool btnB = Buttons::get(Buttons::B).pressed && Buttons::get(Buttons::B).changed;

  if (start) {
    enterEmulatorPause(rt, 0, nullptr);
    return true;
  }
  if (btnA) {
    BmoAppRuntime::quickSaveState(SaveManager::getActiveSlot());
    rt.pauseMetaDirty = true;
    rt.hotkeyCooldownActive = true;
    rt.hotkeyCooldownStartMs = millis();
    return true;
  }
  if (btnB) {
    BmoAppRuntime::quickLoadState(SaveManager::getActiveSlot());
    rt.hotkeyCooldownActive = true;
    rt.hotkeyCooldownStartMs = millis();
    return true;
  }
  if (Buttons::get(Buttons::UP).pressed) {
    PauseQuit::transition(rt.pauseQuitState, PauseQuit::Input::Cancel, false);
    rt.pauseQuitArmedAt = 0;
    enterEmulatorPause(rt, 5, "A TO CONFIRM QUIT; B TO RESUME");
    return true;
  }
  return false;
}

void enforceEmulatorPacing(BmoAppRuntime::RuntimeState& rt, unsigned long frameStart, unsigned long targetFrameUs,
                           bool fastForwardActive) {
  if (fastForwardActive) return;
  unsigned long elapsed = micros() - frameStart;
  if (elapsed < targetFrameUs) {
    const unsigned long remaining = targetFrameUs - elapsed;
    if (remaining > 1500UL) {
      const TickType_t sleepTicks = pdMS_TO_TICKS((remaining - 800UL) / 1000UL);
      if (sleepTicks > 0U) {
        vTaskDelay(sleepTicks);
      }
    }
    while (static_cast<unsigned long>(micros() - frameStart) < targetFrameUs) {
      taskYIELD();
    }
  } else {
    rt.droppedFrames++;
    rt.totalDroppedFramesThisSecond++;
  }
}

void logFpsStatsIfNeeded(BmoAppRuntime::RuntimeState& rt, unsigned long targetFrameUs) {
  rt.frames++;
  const unsigned long now = millis();
  if (now - rt.lastFpsLogMs < 1000UL) return;
  const unsigned long avg = (rt.frameTimeCount > 0) ? (rt.frameTimeSum / rt.frameTimeCount) : 0UL;
  if (rt.totalDroppedFramesThisSecond > 0) {
    LOG_INFO("FPS: %d | Emu: %d target=%luus | Frame: avg=%luus min=%luus max=%luus | Dropped: %d (total %d)",
             rt.frames, rt.selectedEmulatorIndex, targetFrameUs, avg, rt.frameTimeMin, rt.frameTimeMax,
             rt.totalDroppedFramesThisSecond, rt.droppedFrames);
  } else {
    LOG_INFO("FPS: %d | Emu: %d target=%luus | Frame: avg=%luus min=%luus max=%luus", rt.frames,
             rt.selectedEmulatorIndex, targetFrameUs, avg, rt.frameTimeMin, rt.frameTimeMax);
  }
  BMO_PROFILE_REPORT_FRAME_INTERVAL(1000UL, rt.selectedEmulatorIndex, static_cast<uint32_t>(targetFrameUs),
                                    static_cast<uint32_t>(rt.frames),
                                    static_cast<uint32_t>(rt.totalDroppedFramesThisSecond));
  rt.frames = 0;
  rt.totalDroppedFramesThisSecond = 0;
  rt.lastFpsLogMs = now;
  BmoAppRuntime::resetFrameStats();
}

void resumeEmulatorFromPause(BmoAppRuntime::RuntimeState& rt) {
  DisplayEmu::drawConsoleBezel(rt.selectedEmulatorIndex);
  rt.currentState = BmoApp::STATE_EMULATOR;
  rt.hotkeyCooldownActive = true;
  rt.hotkeyCooldownStartMs = millis();
}

bool handlePauseMenuDirectionAndMeta(BmoAppRuntime::RuntimeState& rt, const BmoInput::InputFrame& input) {
  if (input.triggeredOrRepeat(Buttons::UP)) {
    PauseQuit::transition(rt.pauseQuitState, PauseQuit::Input::Cancel, false);
    rt.pauseOption = (rt.pauseOption + 5) % 6;
    rt.pauseToast[0] = '\0';
  } else if (input.triggeredOrRepeat(Buttons::DOWN)) {
    PauseQuit::transition(rt.pauseQuitState, PauseQuit::Input::Cancel, false);
    rt.pauseOption = (rt.pauseOption + 1) % 6;
    rt.pauseToast[0] = '\0';
  } else if (input.edgePressed(Buttons::LEFT)) {
    PauseQuit::transition(rt.pauseQuitState, PauseQuit::Input::Cancel, false);
    snprintf(rt.pauseToast, sizeof(rt.pauseToast), Buttons::toggleSwapAB() ? "A/B SWAP: ON" : "A/B SWAP: OFF");
  } else if (input.edgePressed(Buttons::RIGHT)) {
    PauseQuit::transition(rt.pauseQuitState, PauseQuit::Input::Cancel, false);
    snprintf(rt.pauseToast, sizeof(rt.pauseToast),
             DisplayEmu::toggleBezel() ? "CONSOLE BEZEL: ON" : "CONSOLE BEZEL: OFF");
  } else if (input.edgePressed(Buttons::SELECT)) {
    PauseQuit::transition(rt.pauseQuitState, PauseQuit::Input::Screenshot, false);
    int shotW = 0, shotH = 0;
    const uint16_t* shotFb = DisplayEmu::getLastFramebuffer(&shotW, &shotH);
    const bool ok = shotFb && SaveManager::saveScreenshotBmp(rt.currentRomFilename, shotFb, shotW, shotH);
    snprintf(rt.pauseToast, sizeof(rt.pauseToast), ok ? "SCREENSHOT SAVED!" : "SCREENSHOT FAILED!");
  } else {
    return false;
  }
  return true;
}

bool executePauseQuitConfirm(BmoAppRuntime::RuntimeState& rt) {
  const bool batterySaveSupported = BmoAppRuntime::currentGameHasBatterySaveSupport();
  PauseQuit::Action action = PauseQuit::transition(rt.pauseQuitState, PauseQuit::Input::Confirm, batterySaveSupported);
  if (action == PauseQuit::Action::Stay && rt.pauseQuitState.confirmationArmed) {
    rt.pauseQuitArmedAt = millis();
    snprintf(rt.pauseToast, sizeof(rt.pauseToast), "PRESS A AGAIN TO QUIT; B TO RESUME");
  } else if (action == PauseQuit::Action::AttemptSave) {
    rt.pauseQuitArmedAt = 0;
    if (BmoAppRuntime::autoSaveCurrentBatteryRam()) {
      action = PauseQuit::transition(rt.pauseQuitState, PauseQuit::Input::SaveSucceeded, batterySaveSupported);
    } else {
      action = PauseQuit::transition(rt.pauseQuitState, PauseQuit::Input::SaveFailed, batterySaveSupported);
      rt.pauseMetaDirty = true;
      snprintf(rt.pauseToast, sizeof(rt.pauseToast), "SAVE FAILED: A RETRY, START QUIT");
    }
  }
  if (action == PauseQuit::Action::Quit) {
    rt.pauseQuitArmedAt = 0;
    BmoAppRuntime::finishQuitToConsole();
    return true;
  }
  return false;
}

bool executePausePrimaryOption(BmoAppRuntime::RuntimeState& rt) {
  const int slot = SaveManager::getActiveSlot();
  if (rt.pauseOption == 0) {
    resumeEmulatorFromPause(rt);
    return true;
  }
  if (rt.pauseOption == 1) {
    const bool ok = BmoAppRuntime::quickSaveState(slot);
    if (ok) rt.pauseMetaDirty = true;
    snprintf(rt.pauseToast, sizeof(rt.pauseToast), ok ? "SAVED TO SLOT %d!" : "SAVE FAILED!", slot);
    return false;
  }
  if (rt.pauseOption == 2) {
    if (BmoAppRuntime::quickLoadState(slot)) {
      resumeEmulatorFromPause(rt);
      return true;
    }
    snprintf(rt.pauseToast, sizeof(rt.pauseToast), "SLOT %d EMPTY!", slot);
    return false;
  }
  if (rt.pauseOption == 3) {
    SaveManager::cycleActiveSlot();
    rt.pauseMetaDirty = true;
    snprintf(rt.pauseToast, sizeof(rt.pauseToast), "ACTIVE SLOT: %d", SaveManager::getActiveSlot());
    return false;
  }
  if (rt.pauseOption == 4) {
    rt.pauseMetaDirty = true;
    if (!BmoAppRuntime::currentGameHasBatterySaveSupport()) {
      snprintf(rt.pauseToast, sizeof(rt.pauseToast), "NO BATTERY SAVE");
    } else {
      const bool ok = BmoAppRuntime::autoSaveCurrentBatteryRam();
      snprintf(rt.pauseToast, sizeof(rt.pauseToast), ok ? "BATTERY RAM SAVED!" : "SAVE FAILED!");
    }
    return false;
  }
  return executePauseQuitConfirm(rt);
}

} // namespace

namespace BmoAppEmulatorLoop {

void handleEmulator() {
  BmoAppRuntime::RuntimeState& rt = BmoAppRuntime::state();
  if (processLaunchCelebration(rt)) return;

  const unsigned long frameStart = micros();
  Buttons::update(false);
  if (processEmulatorHotkeys(rt)) return;

  const bool select = Buttons::get(Buttons::SELECT).pressed;
  const bool downEdge = Buttons::get(Buttons::DOWN).pressed && Buttons::get(Buttons::DOWN).changed;
  const bool leftEdge = Buttons::get(Buttons::LEFT).pressed && Buttons::get(Buttons::LEFT).changed;
  const CoreRegistry::CoreDescriptor* activeCore = CoreRegistry::findByIndex(rt.selectedEmulatorIndex);
  if (select && downEdge && activeCore && activeCore->supportsDmgPalette) {
    DisplayEmu::cycleDmgPalette();
  }
  if (select && leftEdge) {
    Buttons::toggleTurbo();
  }

  if (activeCore && activeCore->stepFrame) {
    BMO_PROFILE_START(ZONE_EMU_FRAME);
    activeCore->stepFrame();
    BMO_PROFILE_END(ZONE_EMU_FRAME);
    int gbErr = 0;
    uint16_t gbAddr = 0;
    if (GbCommon::consumeError(&gbErr, &gbAddr)) {
      LOG_ERROR("Core aborted safely: err=%d pc=0x%04X", gbErr, static_cast<unsigned int>(gbAddr));
      BmoAppRuntime::finishQuitToConsole();
      DisplayEmu::showToast("EMULATOR CORE ERROR", Theme::ACCENT_RED, 2800);
      BmoFace::setExpression(BmoFace::ERROR);
      return;
    }
  }

  const unsigned long elapsed = micros() - frameStart;
  const unsigned long targetFrameUs = FramePacing::targetFrameUs(rt.selectedEmulatorIndex);
  BmoAppRuntime::recordFrameTime(elapsed);
  enforceEmulatorPacing(rt, frameStart, targetFrameUs, select && Buttons::get(Buttons::RIGHT).pressed);
  logFpsStatsIfNeeded(rt, targetFrameUs);
}

void handlePauseMenu() {
  BmoAppRuntime::RuntimeState& rt = BmoAppRuntime::state();
  const unsigned long pauseStart = millis();
  Buttons::update(true);
  const auto& input = Buttons::inputFrame();

  if (rt.pauseQuitState.confirmationArmed && static_cast<uint32_t>(millis() - rt.pauseQuitArmedAt) > 3000U) {
    PauseQuit::transition(rt.pauseQuitState, PauseQuit::Input::Timeout, false);
    rt.pauseQuitArmedAt = 0;
  }

  if (!handlePauseMenuDirectionAndMeta(rt, input)) {
    const bool btnB = input.edgePressed(Buttons::B);
    const bool start = input.edgePressed(Buttons::START);
    if (btnB || start) {
      const PauseQuit::Action action =
          (start && rt.pauseQuitState.saveFailed)
              ? PauseQuit::transition(rt.pauseQuitState, PauseQuit::Input::QuitAnyway, false)
              : PauseQuit::transition(rt.pauseQuitState, PauseQuit::Input::Resume, false);
      if (action == PauseQuit::Action::Quit) {
        BmoAppRuntime::finishQuitToConsole();
      } else {
        rt.pauseQuitArmedAt = 0;
        resumeEmulatorFromPause(rt);
      }
      return;
    }
    if (input.edgePressed(Buttons::A) && executePausePrimaryOption(rt)) {
      return;
    }
  }

  const int currentSlot = SaveManager::getActiveSlot();
  const bool batterySaveSupported = BmoAppRuntime::currentGameHasBatterySaveSupport();
  if (rt.pauseMetaDirty) {
    rt.pauseHasSave = SaveManager::hasSaveState(rt.currentRomFilename, currentSlot);
    rt.pauseHasBatt = batterySaveSupported && SaveManager::hasBatteryRam(rt.currentRomFilename);
    rt.pauseMetaDirty = false;
  }

  BMO_PROFILE_START(ZONE_MENU_RENDER);
  DisplayEmu::drawPauseMenu(rt.currentRomFilename, currentSlot, rt.pauseHasSave, rt.pauseHasBatt, batterySaveSupported,
                            rt.pauseOption, rt.pauseToast);
  BMO_PROFILE_END(ZONE_MENU_RENDER);
  rt.pauseToast[0] = '\0';
  BmoAppRuntime::paceMenuFrame(pauseStart);
}

} // namespace BmoAppEmulatorLoop
