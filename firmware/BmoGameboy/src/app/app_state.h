// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../storage/rom_index.h"
#include "../storage/sd_card.h"
#include "../ui/console_catalog.h"
#include "../ui/pause_quit_policy.h"
#include "boot_safe_mode.h"

namespace BmoAppRuntime {

constexpr unsigned long kIdleMascotTimeoutMs = 30000UL;
constexpr int kFallbackVisibleCap = 32;

struct RuntimeState {
  BmoApp::SystemState currentState = BmoApp::STATE_BOOT_SPLASH;
  unsigned long bootSplashStartMs = 0;
  unsigned long lastInputActivityMs = 0;

  int selectedConsoleIndex = 0;
  int selectedEmulatorIndex = 0;
  int selectedGameIndex = 0;

  int fallbackRomIndexes[kFallbackVisibleCap] = {0};
  int* visibleRomIndexes = fallbackRomIndexes;
  const RomFile* fallbackVisibleGames[kFallbackVisibleCap] = {nullptr};
  const RomFile** visibleGames = fallbackVisibleGames;
  int maxVisibleCapacity = kFallbackVisibleCap;
  int visibleGameCount = 0;

  uint8_t* currentRomBuffer = nullptr;
  size_t currentRomBufferSize = 0;
  char currentRomFilename[SDCard::MAX_ROM_PATH_LEN + 1] = "";

  int pauseOption = 0;
  PauseQuit::State pauseQuitState;
  unsigned long pauseQuitArmedAt = 0;
  char pauseToast[48] = "";
  bool pauseMetaDirty = true;
  bool pauseHasSave = false;
  bool pauseHasBatt = false;

  bool launchCelebrationPending = false;
  unsigned long launchCelebrationStartMs = 0;
  bool hotkeyCooldownActive = false;
  unsigned long hotkeyCooldownStartMs = 0;

  unsigned long frameTimeMin = 0;
  unsigned long frameTimeMax = 0;
  unsigned long frameTimeSum = 0;
  int frameTimeCount = 0;

  unsigned long lastFpsLogMs = 0;
  int frames = 0;
  int droppedFrames = 0;
  int totalDroppedFramesThisSecond = 0;

  bool visibleGamesDirty = true;
  bool topPicksOnly = false;
  int cachedConsoleCounts[ConsoleCatalog::CONSOLE_COUNT] = {0};
  bool consoleCountsDirty = true;

  bool spiStressPattern = false;
  bool diagShowProfiler = false;
  uint32_t spiStressFrame = 0;
};

/** Return the mutable Core-1 application runtime state singleton. */
RuntimeState& state();

/** Persist current state/emulator breadcrumb into RTC NOINIT memory. */
void updateBootBreadcrumb();

/** Read the raw RTC NOINIT breadcrumb captured prior to reset. */
BmoApp::BootBreadcrumb readBootBreadcrumb();

/** Grow visible-ROM index/pointer pools in PSRAM to match SD catalog capacity. */
void syncVisiblePoolCapacity();

/** Check whether the active game or ROM header supports battery-backed SRAM saves. */
bool currentGameHasBatterySaveSupport();

/** Flush battery-backed SRAM to SD card for the active ROM if supported. */
bool autoSaveCurrentBatteryRam();

/** Save an emulator state snapshot to the given slot for the active ROM. */
bool quickSaveState(int slot);

/** Load an emulator state snapshot from the given slot for the active ROM. */
bool quickLoadState(int slot);

/** Wait for pending display DMA, destroy active emulator core, and free ROM buffer. */
void destroyActiveEmulator();

/** Destroy active emulator cleanly and transition back to the console carousel. */
void finishQuitToConsole();

/** Record a single core frame duration sample into interval stats (percentiles live in BmoProfiler). */
void recordFrameTime(unsigned long us);

/** Reset per-second frame min/max/sum accumulators. */
void resetFrameStats();

/** Rebuild the filtered visible-games list when `visibleGamesDirty` is set. */
void rebuildVisibleGames();

/** Return the currently highlighted RomFile entry or nullptr when empty. */
const RomFile* selectedGame();

/** Load box art for the currently highlighted ROM or unload stale art. */
void refreshSelectedBoxArt();

/** Load persisted display SPI clock frequency from NVS preferences. */
uint32_t loadSavedDisplaySpiClock();

/** Persist sanitized display SPI clock frequency (40 MHz or 80 MHz) to NVS. */
void persistDisplaySpiClock(uint32_t spiClockHz);

/** Extract upper-case initial letter of a ROM filename for alphabetical jumps. */
char romInitialLetter(const RomFile* rf);

/** Pace non-emulator UI frames to the target frame period. */
void paceMenuFrame(unsigned long frameStartMs, unsigned long targetMs = 16UL);

} // namespace BmoAppRuntime
