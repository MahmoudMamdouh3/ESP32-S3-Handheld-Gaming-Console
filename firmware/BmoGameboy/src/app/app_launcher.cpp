// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "app_launcher.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>

#include "../cores/core_registry.h"
#include "../hal/buttons.h"
#include "../hal/feature_flags.h"
#include "../storage/box_art.h"
#include "../storage/sd_card.h"
#include "../ui/bmo_face.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include "app_state.h"

namespace {

bool checkPlatformLaunchEnabled(const RomFile* selectedRom) {
  if (selectedRom->type == ROM_WAD && !BMO_ENABLE_DOOM) {
    LOG_WARN_STR("DOOM disabled at compile time (-DBMO_ENABLE_DOOM=1 to enable)");
    DisplayEmu::showToast("DOOM CORE OFF", Theme::ACCENT_RED, 2000);
    BmoAppRuntime::refreshSelectedBoxArt();
    Buttons::suppressMenuInputUntil(static_cast<uint32_t>(millis() + 250U));
    return false;
  }
  if (selectedRom->type >= ROM_SMS && !BMO_EXPERIMENTAL_CORES) {
    LOG_WARN_STR("Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
    DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2000);
    BmoAppRuntime::refreshSelectedBoxArt();
    Buttons::suppressMenuInputUntil(static_cast<uint32_t>(millis() + 250U));
    return false;
  }
  return true;
}

void returnToMenuOnLoadFailure(const char* toastMsg, uint16_t toastColor) {
  DisplayEmu::showToast(toastMsg, toastColor, 2200);
  BmoAppRuntime::refreshSelectedBoxArt();
  BmoAppRuntime::state().currentState = BmoApp::STATE_GAME_MENU;
  BmoFace::setExpression(BmoFace::IDLE);
  Buttons::suppressMenuInputUntil(static_cast<uint32_t>(millis() + 300U));
}

void handleRomIdentityMismatch(BmoAppRuntime::RuntimeState& rt, const RomFile* selectedRom, uint8_t* romData) {
  LOG_WARN("ROM mismatch on launch (%s); rebuilding SD index.", selectedRom->filename);
  SDCard::freeRom(romData);
  DisplayEmu::showToast("SD CHANGED: RESCANNING...", Theme::BMO_DPAD_YELLOW, 2500);
  SDCard::rebuildIndex();
  BmoAppRuntime::syncVisiblePoolCapacity();
  rt.consoleCountsDirty = true;
  rt.visibleGamesDirty = true;
  BmoAppRuntime::rebuildVisibleGames();
  BmoAppRuntime::refreshSelectedBoxArt();
  DisplayEmu::showToast("LIBRARY UPDATED", Theme::BMO_DPAD_YELLOW, 1800);
  rt.currentState = BmoApp::STATE_GAME_MENU;
  BmoFace::setExpression(BmoFace::IDLE);
  Buttons::suppressMenuInputUntil(static_cast<uint32_t>(millis() + 300U));
}

void activateStartedCore(BmoAppRuntime::RuntimeState& rt, const CoreRegistry::CoreDescriptor* activeCore,
                         const RomFile* selectedRom, uint8_t* romData, size_t romSize) {
  rt.currentRomBuffer = romData;
  rt.currentRomBufferSize = romSize;
  strncpy(rt.currentRomFilename, selectedRom->filename, sizeof(rt.currentRomFilename) - 1);
  rt.currentRomFilename[sizeof(rt.currentRomFilename) - 1] = '\0';
  SDCard::recordRecentLaunch(rt.currentRomFilename);

  if (activeCore && activeCore->loadBatteryRam && activeCore->hasBatterySave &&
      activeCore->hasBatterySave(rt.currentRomBuffer, rt.currentRomBufferSize)) {
    activeCore->loadBatteryRam(rt.currentRomFilename);
  }

  BmoAppRuntime::resetFrameStats();
  BmoAppRuntime::resetFramePercentiles();
  rt.currentState = BmoApp::STATE_EMULATOR;
  rt.launchCelebrationPending = true;
  rt.launchCelebrationStartMs = millis();
  rt.hotkeyCooldownActive = false;
  BmoFace::setExpression(BmoFace::HAPPY);
  BmoFace::draw();
}

} // namespace

namespace BmoAppLauncher {

void launchSelectedRom(const RomFile* selectedRom) {
  BmoAppRuntime::RuntimeState& rt = BmoAppRuntime::state();
  BoxArt::unload();
  if (!selectedRom) {
    rt.currentState = BmoApp::STATE_CONSOLE_MENU;
    BmoFace::setExpression(BmoFace::IDLE);
    return;
  }
  if (!checkPlatformLaunchEnabled(selectedRom)) {
    return;
  }

  DisplayEmu::cleanupMenuUI();
  DisplayEmu::clearScreen();
  LOG_INFO("PSRAM launch state (%s): free=%u B, largest_block=%u B", selectedRom->filename,
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
           static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));

  size_t romSize = 0;
  uint8_t* romData = (selectedRom->type == ROM_WAD) ? nullptr : SDCard::loadRom(selectedRom->filename, &romSize);
  if (selectedRom->type != ROM_WAD && !romData) {
    LOG_ERROR_STR("Failed to load ROM from SD card.");
    returnToMenuOnLoadFailure("SD READ FAILED: CHECK CARD", Theme::BMO_CORAL_RED);
    return;
  }

  if (!SDCard::verifyRomIdentity(selectedRom, romData, romSize)) {
    handleRomIdentityMismatch(rt, selectedRom, romData);
    return;
  }

  const CoreRegistry::CoreDescriptor* activeCore = CoreRegistry::findByRomType(selectedRom->type);
  const bool started = (activeCore && activeCore->start) ? activeCore->start(selectedRom, romData, romSize) : false;
  if (activeCore && activeCore->start) {
    rt.selectedEmulatorIndex = activeCore->emulatorIndex;
  }
  if (!started) {
    LOG_ERROR_STR("Failed to start emulator. Check errors.");
    SDCard::freeRom(romData);
    rt.currentRomBuffer = nullptr;
    rt.currentRomBufferSize = 0;
    returnToMenuOnLoadFailure("EMULATOR START FAILED", Theme::BMO_CORAL_RED);
    return;
  }

  activateStartedCore(rt, activeCore, selectedRom, romData, romSize);
}

} // namespace BmoAppLauncher
