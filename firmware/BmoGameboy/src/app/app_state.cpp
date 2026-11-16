// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "app_state.h"

#include <Arduino.h>
#include <Preferences.h>
#include <ctype.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <limits.h>
#include <string.h>

#include "../cores/core_registry.h"
#include "../hal/board_timing.h"
#include "../hal/buttons.h"
#include "../hal/feature_flags.h"
#include "../storage/box_art.h"
#include "../ui/bmo_face.h"
#include "../ui/display_emu.h"

namespace {

constexpr const char* kDisplayPrefsNamespace = "bmo_disp";
constexpr const char* kDisplaySpiHzKey = "spi_hz";

RTC_NOINIT_ATTR BmoApp::BootBreadcrumb s_bootBreadcrumb;

BmoAppRuntime::RuntimeState& mutableState() {
  static BmoAppRuntime::RuntimeState instance = []() {
    BmoAppRuntime::RuntimeState init{};
    init.visibleRomIndexes = init.fallbackRomIndexes;
    init.visibleGames = init.fallbackVisibleGames;
    init.frameTimeMin = ULONG_MAX;
    return init;
  }();
  return instance;
}

void collectFavoriteIndexes(BmoAppRuntime::RuntimeState& rt, int totalRoms) {
  for (int i = 0; i < totalRoms && rt.visibleGameCount < rt.maxVisibleCapacity; ++i) {
    const RomFile* game = SDCard::getRomInfo(i);
    if (game && game->isFavorite) {
      rt.visibleRomIndexes[rt.visibleGameCount++] = i;
    }
  }
}

void collectTypedIndexes(BmoAppRuntime::RuntimeState& rt, RomType selectedType, int totalRoms, bool popularOnly) {
  for (int i = 0; i < totalRoms && rt.visibleGameCount < rt.maxVisibleCapacity; ++i) {
    const RomFile* game = SDCard::getRomInfo(i);
    if (game && game->type == selectedType && (!popularOnly || game->isPopular)) {
      rt.visibleRomIndexes[rt.visibleGameCount++] = i;
    }
  }
}

} // namespace

namespace BmoAppRuntime {

RuntimeState& state() {
  return mutableState();
}

void updateBootBreadcrumb() {
  static uint8_t s_lastState = 0xFFU;
  static int8_t s_lastEmu = -2;
  const RuntimeState& rt = mutableState();
  const uint8_t curState = static_cast<uint8_t>(rt.currentState);
  const int8_t curEmu = static_cast<int8_t>(rt.selectedEmulatorIndex);
  if (s_bootBreadcrumb.magic == BmoApp::kBreadcrumbMagic && curState == s_lastState && curEmu == s_lastEmu) {
    return;
  }
  s_lastState = curState;
  s_lastEmu = curEmu;
  s_bootBreadcrumb = BmoApp::makeBootBreadcrumb(curState, curEmu);
}

BmoApp::BootBreadcrumb readBootBreadcrumb() {
  return s_bootBreadcrumb;
}

void syncVisiblePoolCapacity() {
  RuntimeState& rt = mutableState();
  int targetCap = SDCard::computePoolCapacity(SDCard::getRomCount());
  if (targetCap < SDCard::getPoolCapacity()) {
    targetCap = SDCard::getPoolCapacity();
  }
  if (targetCap <= rt.maxVisibleCapacity) return;

  int* psramIndexes = static_cast<int*>(heap_caps_malloc(sizeof(int) * (size_t)targetCap, MALLOC_CAP_SPIRAM));
  const RomFile** psramGames =
      static_cast<const RomFile**>(heap_caps_malloc(sizeof(const RomFile*) * (size_t)targetCap, MALLOC_CAP_SPIRAM));
  if (!psramIndexes || !psramGames) {
    if (psramIndexes) heap_caps_free(psramIndexes);
    if (psramGames) heap_caps_free(static_cast<void*>(psramGames));
    LOG_WARN_STR("PSRAM game-menu allocation failed; using the current bounded capacity.");
    return;
  }

  if (rt.visibleRomIndexes != rt.fallbackRomIndexes) heap_caps_free(rt.visibleRomIndexes);
  if (rt.visibleGames != rt.fallbackVisibleGames) heap_caps_free(static_cast<void*>(rt.visibleGames));
  rt.visibleRomIndexes = psramIndexes;
  rt.visibleGames = psramGames;
  rt.maxVisibleCapacity = targetCap;
  rt.visibleGameCount = 0;
}

bool currentGameHasBatterySaveSupport() {
  const RuntimeState& rt = mutableState();
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(rt.selectedEmulatorIndex);
  if (!core) return false;
  if (core->hasActiveBatteryRam) {
    return core->hasActiveBatteryRam();
  }
  if (!rt.currentRomBuffer || !core->hasBatterySave) return false;
  return core->hasBatterySave(rt.currentRomBuffer, rt.currentRomBufferSize);
}

bool autoSaveCurrentBatteryRam() {
  const RuntimeState& rt = mutableState();
  if (rt.currentRomFilename[0] == '\0' || !currentGameHasBatterySaveSupport()) return false;
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(rt.selectedEmulatorIndex);
  return core && core->saveBatteryRam ? core->saveBatteryRam(rt.currentRomFilename) : false;
}

bool quickSaveState(int slot) {
  const RuntimeState& rt = mutableState();
  if (rt.currentRomFilename[0] == '\0') return false;
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(rt.selectedEmulatorIndex);
  return core && core->saveState ? core->saveState(rt.currentRomFilename, slot) : false;
}

bool quickLoadState(int slot) {
  const RuntimeState& rt = mutableState();
  if (rt.currentRomFilename[0] == '\0') return false;
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(rt.selectedEmulatorIndex);
  return core && core->loadState ? core->loadState(rt.currentRomFilename, slot) : false;
}

void destroyActiveEmulator() {
  RuntimeState& rt = mutableState();
  DisplayEmu::waitForDisplay();
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(rt.selectedEmulatorIndex);
  if (core && core->destroy) {
    core->destroy();
  }

  if (rt.currentRomBuffer) {
    SDCard::freeRom(rt.currentRomBuffer);
    rt.currentRomBuffer = nullptr;
  }
  rt.currentRomBufferSize = 0;
  rt.currentRomFilename[0] = '\0';
  rt.launchCelebrationPending = false;
  DisplayEmu::resetLastFramebuffer();
}

void finishQuitToConsole() {
  destroyActiveEmulator();
  mutableState().currentState = BmoApp::STATE_CONSOLE_MENU;
  BmoFace::setExpression(BmoFace::IDLE);
  Buttons::suppressMenuInputUntil(static_cast<uint32_t>(millis() + 300U));
}

void recordFrameTime(unsigned long us) {
  RuntimeState& rt = mutableState();
  if (us < rt.frameTimeMin) rt.frameTimeMin = us;
  if (us > rt.frameTimeMax) rt.frameTimeMax = us;
  rt.frameTimeSum += us;
  rt.frameTimeCount++;
}

void resetFrameStats() {
  RuntimeState& rt = mutableState();
  rt.frameTimeMin = ULONG_MAX;
  rt.frameTimeMax = 0;
  rt.frameTimeSum = 0;
  rt.frameTimeCount = 0;
}

void rebuildVisibleGames() {
  RuntimeState& rt = mutableState();
  if (!rt.visibleGamesDirty) return;
  rt.visibleGameCount = 0;
  const RomType selectedType = ConsoleCatalog::CONSOLES[rt.selectedConsoleIndex];
  const int totalRoms = SDCard::getRomCount();

  if (selectedType == ROM_FAVORITES) {
    collectFavoriteIndexes(rt, totalRoms);
  } else {
    collectTypedIndexes(rt, selectedType, totalRoms, rt.topPicksOnly);
    if (rt.topPicksOnly && rt.visibleGameCount == 0) {
      collectTypedIndexes(rt, selectedType, totalRoms, false);
    }
  }

  for (int i = 0; i < rt.visibleGameCount; ++i) {
    rt.visibleGames[i] = SDCard::getRomInfo(rt.visibleRomIndexes[i]);
  }

  if (rt.visibleGameCount == 0) {
    rt.selectedGameIndex = 0;
  } else if (rt.selectedGameIndex >= rt.visibleGameCount) {
    rt.selectedGameIndex = rt.visibleGameCount - 1;
  }
  rt.visibleGamesDirty = false;
}

const RomFile* selectedGame() {
  const RuntimeState& rt = mutableState();
  if (rt.selectedGameIndex < 0 || rt.selectedGameIndex >= rt.visibleGameCount) return nullptr;
  return SDCard::getRomInfo(rt.visibleRomIndexes[rt.selectedGameIndex]);
}

void refreshSelectedBoxArt() {
  const RuntimeState& rt = mutableState();
  const RomFile* rom = (rt.visibleGameCount > 0) ? selectedGame() : nullptr;
  if (rom && rom->hasBoxArt) {
    BoxArt::load(rom->filename);
  } else {
    BoxArt::unload();
  }
}

uint32_t loadSavedDisplaySpiClock() {
  Preferences prefs;
  if (!prefs.begin(kDisplayPrefsNamespace, true)) {
    return TFT_SPI_HZ;
  }
  const uint32_t savedHz = prefs.getUInt(kDisplaySpiHzKey, TFT_SPI_HZ);
  prefs.end();
  return (savedHz == TFT_SPI_FALLBACK_HZ) ? TFT_SPI_FALLBACK_HZ : TFT_SPI_HZ;
}

void persistDisplaySpiClock(uint32_t spiClockHz) {
  const uint32_t sanitizedHz = (spiClockHz == TFT_SPI_FALLBACK_HZ) ? TFT_SPI_FALLBACK_HZ : TFT_SPI_HZ;
  Preferences prefs;
  if (!prefs.begin(kDisplayPrefsNamespace, false)) {
    return;
  }
  prefs.putUInt(kDisplaySpiHzKey, sanitizedHz);
  prefs.end();
}

char romInitialLetter(const RomFile* rf) {
  if (!rf || !rf->filename[0]) return 'A';
  const char* base = strrchr(rf->filename, '/');
  if (!base) base = strrchr(rf->filename, '\\');
  base = base ? (base + 1) : rf->filename;
  return base[0] ? static_cast<char>(toupper(static_cast<unsigned char>(base[0]))) : 'A';
}

void paceMenuFrame(unsigned long frameStartMs, unsigned long targetMs) {
  const unsigned long elapsed = millis() - frameStartMs;
  if (elapsed < targetMs) {
    const TickType_t ticks = pdMS_TO_TICKS(targetMs - elapsed);
    if (ticks > 0U) {
      vTaskDelay(ticks);
    } else {
      taskYIELD();
    }
  }
}

} // namespace BmoAppRuntime
