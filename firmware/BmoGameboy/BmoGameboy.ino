#include "src/hal/config.h"
#include "src/cores/core_registry.h"
#include "src/tests/unit_tests.h"
#include "src/hal/buttons.h"
#include "src/storage/sd_card.h"
#include "src/hal/battery.h"
#include "src/ui/display_emu.h"
#include "src/ui/theme.h"
#include "src/ui/bmo_face.h"
#include "src/hal/spi_arbiter.h"
#include "src/storage/save_manager.h"
#include "src/storage/battery_save.h"
#include "src/storage/box_art.h"
#include "src/storage/rom_index.h"
#include "src/ui/pause_quit_policy.h"
#include "src/ui/frame_pacing.h"
#include "src/ui/frame_percentiles.h"
#include <SPI.h>
#include <Preferences.h>
#include <stdlib.h>
#include <rom/ets_sys.h>      // N7: ets_delay_us for tight hardware-timer spin
#include <esp_heap_caps.h>    // BM2: IRAM usage reporting
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

enum SystemState {
  STATE_BOOT_SPLASH,
  STATE_CONSOLE_MENU,
  STATE_CONSOLE_MUSEUM,
  STATE_GAME_MENU,
  STATE_EMULATOR,
  STATE_PAUSE_MENU,
  STATE_DIAGNOSTICS,
  STATE_IDLE_MASCOT
};

SystemState currentState = STATE_BOOT_SPLASH;
static unsigned long bootSplashStartMs = 0;
static unsigned long lastInputActivityMs = 0;
static const unsigned long IDLE_MASCOT_TIMEOUT_MS = 30000;
int selectedConsoleIndex = 0;
int selectedEmulatorIndex = 0;
int selectedGameIndex = 0;
static const int MAX_VISIBLE_ROMS = 36000;
static int fallbackRomIndexes[32];
static int* visibleRomIndexes = fallbackRomIndexes;
static const RomFile* fallbackVisibleGames[32];
static const RomFile** visibleGames = fallbackVisibleGames;
static int maxVisibleCapacity = 32;
static int visibleGameCount = 0;

static void syncVisiblePoolCapacity() {
  int targetCap = SDCard::computePoolCapacity(SDCard::getRomCount());
  if (targetCap < SDCard::getPoolCapacity()) {
    targetCap = SDCard::getPoolCapacity();
  }
  if (targetCap <= maxVisibleCapacity) return;

  int* psramIndexes = (int*)heap_caps_malloc(sizeof(int) * (size_t)targetCap, MALLOC_CAP_SPIRAM);
  const RomFile** psramGames = (const RomFile**)heap_caps_malloc(sizeof(const RomFile*) * (size_t)targetCap, MALLOC_CAP_SPIRAM);
  if (!psramIndexes || !psramGames) {
    if (psramIndexes) heap_caps_free(psramIndexes);
    if (psramGames) heap_caps_free(psramGames);
    LOG_WARN_STR("PSRAM game-menu allocation failed; using the current bounded capacity.");
    return;
  }

  if (visibleRomIndexes != fallbackRomIndexes) heap_caps_free(visibleRomIndexes);
  if (visibleGames != fallbackVisibleGames) heap_caps_free(visibleGames);
  visibleRomIndexes = psramIndexes;
  visibleGames = psramGames;
  maxVisibleCapacity = targetCap;
  visibleGameCount = 0;
}

static const RomType CONSOLES[] = {
  ROM_FAVORITES,
  ROM_GB, ROM_GBC, ROM_NES, ROM_WAD,
  ROM_SMS, ROM_GG, ROM_PCE, ROM_ATARI, ROM_PICO8,
  ROM_GENESIS, ROM_SNES, ROM_WSWAN, ROM_NGP, ROM_LYNX, ROM_COLEM
};
static const int CONSOLE_COUNT = sizeof(CONSOLES) / sizeof(CONSOLES[0]);

// Pointer to dynamically loaded ROM buffer in PSRAM
uint8_t* currentRomBuffer = nullptr;
size_t currentRomBufferSize = 0;
char currentRomFilename[64] = "";
static int pauseOption = 0;
static PauseQuit::State s_pauseQuitState;
static unsigned long s_pauseQuitArmedAt = 0;
static char pauseToast[48] = "";
static bool s_pauseMetaDirty = true;
static bool s_pauseHasSave = false;
static bool s_pauseHasBatt = false;

static bool currentGameHasBatterySaveSupport() {
  if (!currentRomBuffer) return false;
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(selectedEmulatorIndex);
  if (!core || !core->hasBatterySave) return false;
  return core->hasBatterySave(currentRomBuffer, currentRomBufferSize);
}

static bool autoSaveCurrentBatteryRam() {
  if (currentRomFilename[0] == '\0' || !currentGameHasBatterySaveSupport()) return false;
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(selectedEmulatorIndex);
  return core && core->saveBatteryRam ? core->saveBatteryRam(currentRomFilename) : false;
}

static bool quickSaveState(int slot) {
  if (currentRomFilename[0] == '\0') return false;
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(selectedEmulatorIndex);
  return core && core->saveState ? core->saveState(currentRomFilename, slot) : false;
}

static bool quickLoadState(int slot) {
  if (currentRomFilename[0] == '\0') return false;
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(selectedEmulatorIndex);
  return core && core->loadState ? core->loadState(currentRomFilename, slot) : false;
}


static void destroyActiveEmulator() {
  DisplayEmu::waitForDisplay();
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(selectedEmulatorIndex);
  if (core && core->destroy) {
    core->destroy();
  }
  
  if (currentRomBuffer) {
    SDCard::freeRom(currentRomBuffer);
    currentRomBuffer = nullptr;
  }
  currentRomBufferSize = 0;
  currentRomFilename[0] = '\0';
  DisplayEmu::resetLastFramebuffer();
}

static void finishQuitToConsole() {
  destroyActiveEmulator();
  currentState = STATE_CONSOLE_MENU;
  BmoFace::setExpression(BmoFace::IDLE);
  delay(300);
}

// P3: Timestamp-based debounce — non-blocking replacement for delay(200).
static const unsigned long DEBOUNCE_MS = 200;
unsigned long lastButtonMs = 0;

inline bool canPress() {
  return (millis() - lastButtonMs) >= DEBOUNCE_MS;
}

// ---------------------------------------------------------------------------
// BM1: Frame timing ring buffer
// ---------------------------------------------------------------------------
// Tracks min/max/avg frame time over the last reporting window so we can
// evaluate the effect of each optimisation pass precisely.
static unsigned long frameTimeMin = ULONG_MAX;
static unsigned long frameTimeMax = 0;
static unsigned long frameTimeSum = 0;
static int          frameTimeCount = 0;
static unsigned long framePercentileSamples[FramePercentiles::WINDOW_SIZE];
static size_t framePercentileCount = 0;

static inline void recordFrameTime(unsigned long us, int emulatorIndex) {
  if (us < frameTimeMin) frameTimeMin = us;
  if (us > frameTimeMax) frameTimeMax = us;
  frameTimeSum  += us;
  frameTimeCount++;
  framePercentileSamples[framePercentileCount++] = us;
  if (framePercentileCount == FramePercentiles::WINDOW_SIZE) {
    FramePercentiles::Summary percentiles;
    if (FramePercentiles::sortAndSummarize(framePercentileSamples,
                                           FramePercentiles::WINDOW_SIZE, percentiles)) {
      LOG_INFO("Frame window: emu=%d n=600 target=%luus p50=%luus p95=%luus p99=%luus (core execution)",
               emulatorIndex, FramePacing::targetFrameUs(emulatorIndex), percentiles.p50,
               percentiles.p95, percentiles.p99);
    }
    framePercentileCount = 0;
  }
}

static void resetFrameStats() {
  frameTimeMin   = ULONG_MAX;
  frameTimeMax   = 0;
  frameTimeSum   = 0;
  frameTimeCount = 0;
}

static void resetFramePercentiles() {
  framePercentileCount = 0;
}

static bool visibleGamesDirty = true;
static bool s_topPicksOnly = false;

// File-scope so STATE_GAME_MENU (toggleFavorite) can invalidate the
//          Favorites-count badge rendered in STATE_CONSOLE_MENU.
static int  s_cachedConsoleCounts[CONSOLE_COUNT];
static bool s_consoleCountsDirty = true;

static int countGamesForConsole(RomType type) {
  // O(1) lookup via SDCard::getRomCountForType (eliminates 245K iterations per frame)
  return SDCard::getRomCountForType(type);
}

static void rebuildVisibleGames() {
  // Gate scan with dirty flag so idle frames do not perform O(N) linear scans
  if (!visibleGamesDirty) return;
  visibleGameCount = 0;
  const RomType selectedType = CONSOLES[selectedConsoleIndex];
  const int totalRoms = SDCard::getRomCount();

  if (selectedType == ROM_FAVORITES) {
    for (int i = 0; i < totalRoms && visibleGameCount < maxVisibleCapacity; ++i) {
      const RomFile* game = SDCard::getRomInfo(i);
      if (game && game->isFavorite) {
        visibleRomIndexes[visibleGameCount++] = i;
      }
    }
  } else {
    for (int i = 0; i < totalRoms && visibleGameCount < maxVisibleCapacity; ++i) {
      const RomFile* game = SDCard::getRomInfo(i);
      if (game && game->type == selectedType && (!s_topPicksOnly || game->isPopular)) {
        visibleRomIndexes[visibleGameCount++] = i;
      }
    }
    // If Top Picks filter yielded 0 matches on this library, gracefully fall back to all games
    if (s_topPicksOnly && visibleGameCount == 0) {
      for (int i = 0; i < totalRoms && visibleGameCount < maxVisibleCapacity; ++i) {
        const RomFile* game = SDCard::getRomInfo(i);
        if (game && game->type == selectedType) {
          visibleRomIndexes[visibleGameCount++] = i;
        }
      }
    }
  }

  // Rebuild pointer array here (dirty-flag guarded) so the game-menu
  //          render path never repeats this work on idle frames.
  for (int i = 0; i < visibleGameCount; ++i) {
    visibleGames[i] = SDCard::getRomInfo(visibleRomIndexes[i]);
  }

  if (visibleGameCount == 0) selectedGameIndex = 0;
  else if (selectedGameIndex >= visibleGameCount) selectedGameIndex = visibleGameCount - 1;
  visibleGamesDirty = false;
}

static const RomFile* selectedGame() {
  if (selectedGameIndex < 0 || selectedGameIndex >= visibleGameCount) return nullptr;
  return SDCard::getRomInfo(visibleRomIndexes[selectedGameIndex]);
}
// ---------------------------------------------------------------------------

// These must be declared before setup() so NB5 can set lastTime = millis().
unsigned long lastTime = 0;
int frames = 0;
int droppedFrames = 0;
int totalDroppedFramesThisSecond = 0;

namespace {
constexpr const char* kDisplayPrefsNamespace = "bmo_disp";
constexpr const char* kDisplaySpiHzKey = "spi_hz";

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
  const uint32_t sanitizedHz =
      (spiClockHz == TFT_SPI_FALLBACK_HZ) ? TFT_SPI_FALLBACK_HZ : TFT_SPI_HZ;
  Preferences prefs;
  if (!prefs.begin(kDisplayPrefsNamespace, false)) {
    return;
  }
  prefs.putUInt(kDisplaySpiHzKey, sanitizedHz);
  prefs.end();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  // USB CDC writes can wait under host backpressure; logging is best-effort so
  // a connected-but-not-reading host cannot stall the emulator loop.
  Serial.setTxTimeoutMs(0);
  
  // Wait 3 seconds for Serial to connect (avoids hang on UART port).
  // In release builds skip the 3-second Serial wait — users see a black
  //           screen for 3 s on every cold boot otherwise.
#if defined(CORE_DEBUG_LEVEL) && CORE_DEBUG_LEVEL > 0
  delay(3000);
#else
  delay(500);  // Brief settle for USB CDC enumeration.
#endif

  LOG_INFO_STR("\n\n--- BOOTING ---");
  LOG_INFO_STR("Milestone 4: Game Selection UI");
  LOG_INFO("Features: SD=%d, Audio=%d, Battery=%d", FEATURE_SD_CARD, FEATURE_AUDIO, FEATURE_BATTERY_MONITOR);

  // Initialize shared SPI bus arbiter before any device uses it.
  SpiArbiter::init();
  SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, -1);

  #ifdef ENABLE_UNIT_TESTS
    LOG_INFO_STR("Booting into Test Mode...");
    bool all_passed = runAllTests();
    LOG_INFO_STR(all_passed ? "TESTS SUCCESS" : "TESTS FAILED");
    while(1) delay(100);
  #endif

  Buttons::begin();
  Buttons::update();
  const bool safeBootRequested =
      Buttons::get(Buttons::SELECT).pressed || Buttons::get(Buttons::B).pressed;
  uint32_t initialSpiHz = safeBootRequested ? TFT_SPI_FALLBACK_HZ : loadSavedDisplaySpiClock();
  if (safeBootRequested) {
    persistDisplaySpiClock(TFT_SPI_FALLBACK_HZ);
    LOG_WARN("[SAFE_MODE] SELECT/B held at boot; forced display SPI clock to %lu MHz",
             (unsigned long)(TFT_SPI_FALLBACK_HZ / 1000000UL));
  }
  DisplayEmu::begin(initialSpiHz);
  if (safeBootRequested) {
    DisplayEmu::showToast("SAFE MODE: 40 MHz SPI", Theme::BMO_DPAD_YELLOW, 3500);
  }
  Battery::begin();
  BmoFace::begin();
  BmoFace::setExpression(BmoFace::IDLE);
  bootSplashStartMs = millis();

  if (!SDCard::begin()) {
    LOG_ERROR_STR("Failed to mount SD card!");
  } else {
    LOG_INFO("SD Card mounted. Found %d ROMs.", SDCard::getRomCount());
    SaveManager::begin();
  }
  syncVisiblePoolCapacity();

  LOG_INFO("PSRAM boot state: free=%u B, largest_block=%u B, pool_cap=%d",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
           SDCard::getPoolCapacity());

  // BM2: Report IRAM free size so we can verify IRAM_ATTR budget usage.
  LOG_INFO("IRAM free: %u bytes",
                heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT));

  // Set lastTime to NOW so the first FPS window is valid
  lastTime = millis();

  // Expression already set to IDLE above; update() will handle blinking.
}

void loop() {
  Battery::update();
  Buttons::setGameplayMode(currentState == STATE_EMULATOR);
  // Face is HIDDEN during emulation/pause and never drawn; skip update() overhead.
  if (currentState != STATE_EMULATOR && currentState != STATE_PAUSE_MENU) {
    BmoFace::update();
  }

  if (currentState == STATE_BOOT_SPLASH) {
    DisplayEmu::initMenuUI();
    Buttons::update();
    uint16_t btnMask = 0;
    if (Buttons::get(Buttons::UP).pressed) btnMask |= (1 << 0);
    if (Buttons::get(Buttons::DOWN).pressed) btnMask |= (1 << 1);
    if (Buttons::get(Buttons::LEFT).pressed) btnMask |= (1 << 2);
    if (Buttons::get(Buttons::RIGHT).pressed) btnMask |= (1 << 3);
    if (Buttons::get(Buttons::A).pressed) btnMask |= (1 << 4);
    if (Buttons::get(Buttons::B).pressed) btnMask |= (1 << 5);
    if (Buttons::get(Buttons::SELECT).pressed) btnMask |= (1 << 6);
    if (Buttons::get(Buttons::START).pressed) btnMask |= (1 << 7);
    if (Buttons::get(Buttons::X).pressed) btnMask |= (1 << 8);
    if (Buttons::get(Buttons::Y).pressed) btnMask |= (1 << 9);
    if (Buttons::get(Buttons::L).pressed) btnMask |= (1 << 10);
    if (Buttons::get(Buttons::R).pressed) btnMask |= (1 << 11);

    if ((btnMask != 0 && canPress()) || (millis() - bootSplashStartMs > 3500)) {
      currentState = STATE_CONSOLE_MENU;
      lastInputActivityMs = millis();
      lastButtonMs = millis();
      return;
    }

    bool blinkState = ((millis() / 400) % 2 == 0);
    DisplayEmu::drawBootSplash(blinkState);
    delay(33);
    return;
  }
  
  if (currentState == STATE_CONSOLE_MENU) {
    const unsigned long menuFrameStart = millis();
    DisplayEmu::initMenuUI();
    
    Buttons::update();
    const auto& btnLeft = Buttons::get(Buttons::LEFT);
    const auto& btnRight = Buttons::get(Buttons::RIGHT);
    const auto& btnUp = Buttons::get(Buttons::UP);
    const auto& btnDown = Buttons::get(Buttons::DOWN);
    const auto& btnA = Buttons::get(Buttons::A);
    const auto& btnSelect = Buttons::get(Buttons::SELECT);
    const auto& btnStart = Buttons::get(Buttons::START);
    bool left   = btnLeft.pressed   && btnLeft.changed;
    bool right  = btnRight.pressed  && btnRight.changed;
    bool up     = btnUp.pressed     && btnUp.changed;
    bool down   = btnDown.pressed   && btnDown.changed;
    bool a      = btnA.pressed      && btnA.changed;
    bool select = btnSelect.pressed && btnSelect.changed;
    bool start  = btnStart.pressed  && btnStart.changed;
    
    if (canPress()) {
      // Cache millis() once per frame instead of calling it 5+ times per event.
      const unsigned long nowMs = millis();
      if (left || right || up || down || a || select || start) {
        lastInputActivityMs = nowMs;
      }
      if (left) {
        selectedConsoleIndex = (selectedConsoleIndex - 1 + CONSOLE_COUNT) % CONSOLE_COUNT;
        visibleGamesDirty = true;
        lastButtonMs = nowMs;
        BmoFace::setGaze(-0.70f, 0.0f); // BMO companion glances left with carousel
        DisplayEmu::triggerCarouselGlide(false);
      }
      if (right) {
        selectedConsoleIndex = (selectedConsoleIndex + 1) % CONSOLE_COUNT;
        visibleGamesDirty = true;
        lastButtonMs = nowMs;
        BmoFace::setGaze(0.70f, 0.0f);  // BMO companion glances right with carousel
        DisplayEmu::triggerCarouselGlide(true);
      }
      if (up) {
        currentState = STATE_IDLE_MASCOT;
        BmoFace::setExpression(BmoFace::HAPPY);
        BmoFace::say("Who wants to play video games?!", 3500);
        lastButtonMs = nowMs;
      }
      if (down) {
        currentState = STATE_CONSOLE_MUSEUM;
        lastButtonMs = nowMs;
      }
      if (select) {
        currentState = STATE_CONSOLE_MUSEUM;
        lastButtonMs = nowMs;
      }
      if (start) {
        currentState = STATE_DIAGNOSTICS;
        lastButtonMs = nowMs;
      }
      if (a) {
        selectedGameIndex = 0;
        visibleGamesDirty = true;
        rebuildVisibleGames();
        currentState = STATE_GAME_MENU;
        BmoFace::setExpression(BmoFace::IDLE);
        if (visibleGameCount > 0 && selectedGame() && selectedGame()->hasBoxArt) {
          BoxArt::load(selectedGame()->filename);
        } else {
          BoxArt::unload();
        }
        lastButtonMs = nowMs;
      }
    }

    if (millis() - lastInputActivityMs > IDLE_MASCOT_TIMEOUT_MS) {
      currentState = STATE_IDLE_MASCOT;
      BmoFace::setExpression(BmoFace::SLEEPY);
    }

    // Use file-scope dirty flag so toggleFavorite() in STATE_GAME_MENU
    //          invalidates the Favorites-count badge in the carousel.
    if (s_consoleCountsDirty) {
      for (int i = 0; i < CONSOLE_COUNT; ++i)
        s_cachedConsoleCounts[i] = SDCard::getRomCountForType(CONSOLES[i]);
      s_consoleCountsDirty = false;
    }
    DisplayEmu::drawConsoleSelectMenu(selectedConsoleIndex, s_cachedConsoleCounts, CONSOLE_COUNT, SDCard::isMounted());

    // The full-screen SPI blit already consumes most of a 16.7 ms frame.
    // Only sleep for the remaining budget; an unconditional delay(16) here
    // previously limited the menu to roughly 30 FPS.
    const unsigned long menuElapsed = millis() - menuFrameStart;
    if (menuElapsed < 16) delay(16 - menuElapsed);

  } else if (currentState == STATE_CONSOLE_MUSEUM) {
    const unsigned long museumFrameStart = millis();
    DisplayEmu::initMenuUI();
    Buttons::update();
    const auto& btnB = Buttons::get(Buttons::B);
    const auto& btnSelect = Buttons::get(Buttons::SELECT);
    const auto& btnA = Buttons::get(Buttons::A);
    bool b = btnB.pressed && btnB.changed;
    bool select = btnSelect.pressed && btnSelect.changed;
    bool a = btnA.pressed && btnA.changed;

    if (canPress() && (b || select || a)) {
      currentState = STATE_CONSOLE_MENU;
      lastButtonMs = millis();
    }

    DisplayEmu::drawConsoleMuseumModal(CONSOLES[selectedConsoleIndex]);

    const unsigned long elapsed = millis() - museumFrameStart;
    if (elapsed < 16) delay(16 - elapsed);

  } else if (currentState == STATE_DIAGNOSTICS) {
    static bool spiStressPattern = false;
    static uint32_t spiStressFrame = 0;
    static unsigned long lastStackLogMs = 0;
    const unsigned long diagFrameStart = millis();
    DisplayEmu::initMenuUI();
    Buttons::update();
    bool spiStressFallback = (DisplayEmu::getDiagnosticSpiClock() == TFT_SPI_FALLBACK_HZ);

    uint16_t btnMask = 0;
    if (Buttons::get(Buttons::UP).pressed)     btnMask |= (1 << 0);
    if (Buttons::get(Buttons::DOWN).pressed)   btnMask |= (1 << 1);
    if (Buttons::get(Buttons::LEFT).pressed)   btnMask |= (1 << 2);
    if (Buttons::get(Buttons::RIGHT).pressed)  btnMask |= (1 << 3);
    if (Buttons::get(Buttons::A).pressed)      btnMask |= (1 << 4);
    if (Buttons::get(Buttons::B).pressed)      btnMask |= (1 << 5);
    if (Buttons::get(Buttons::SELECT).pressed) btnMask |= (1 << 6);
    if (Buttons::get(Buttons::START).pressed)  btnMask |= (1 << 7);
    if (Buttons::get(Buttons::X).pressed)      btnMask |= (1 << 8);
    if (Buttons::get(Buttons::Y).pressed)      btnMask |= (1 << 9);
    if (Buttons::get(Buttons::L).pressed)      btnMask |= (1 << 10);
    if (Buttons::get(Buttons::R).pressed)      btnMask |= (1 << 11);

    const auto& btnB = Buttons::get(Buttons::B);
    const auto& btnA = Buttons::get(Buttons::A);
    const auto& btnLeft = Buttons::get(Buttons::LEFT);
    const auto& btnRight = Buttons::get(Buttons::RIGHT);
    const auto& btnSelect = Buttons::get(Buttons::SELECT);
    if (canPress() && btnB.pressed && btnB.changed) {
      spiStressPattern = false;
      currentState = STATE_CONSOLE_MENU;
      lastButtonMs = millis();
    } else if (canPress() && btnA.pressed && btnA.changed) {
      spiStressPattern = !spiStressPattern;
      lastButtonMs = millis();
    } else if (canPress() && spiStressPattern &&
               ((btnLeft.pressed && btnLeft.changed) || (btnRight.pressed && btnRight.changed))) {
      spiStressFallback = !spiStressFallback;
      const uint32_t nextSpiHz = spiStressFallback ? TFT_SPI_FALLBACK_HZ : TFT_SPI_HZ;
      DisplayEmu::setDiagnosticSpiClock(nextSpiHz);
      persistDisplaySpiClock(nextSpiHz);
      lastButtonMs = millis();
    } else if (canPress() && !spiStressPattern && btnSelect.pressed && btnSelect.changed) {
      if (!SDCard::isMounted()) {
        const bool mounted = SDCard::begin();
        if (mounted) {
          SaveManager::begin();
          syncVisiblePoolCapacity();
          s_consoleCountsDirty = true;
          visibleGamesDirty = true;
        }
        DisplayEmu::showToast(mounted ? "SD CARD RETRY SUCCEEDED" : "SD CARD STILL NOT FOUND",
                              Theme::BMO_DPAD_YELLOW, 3000);
      } else {
        DisplayEmu::showToast("RESCANNING SD LIBRARY", Theme::BMO_DPAD_YELLOW, 10000);
        SDCard::rebuildIndex();
        syncVisiblePoolCapacity();
        s_consoleCountsDirty = true;
        visibleGamesDirty = true;
        DisplayEmu::showToast(SDCard::getSkippedRomCount() == 0
                                  ? "LIBRARY RESCAN COMPLETE"
                                  : "RESCAN COMPLETE; SOME PATHS SKIPPED",
                              Theme::BMO_DPAD_YELLOW, 3000);
      }
      lastButtonMs = millis();
    }

    uint32_t freeDram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t largestPsramBlock = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    uint32_t freeIram = heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT);
    uint32_t loopStackHighWaterWords = (uint32_t)uxTaskGetStackHighWaterMark(nullptr);
    uint32_t displayStackHighWaterWords = DisplayEmu::getDisplayStackHighWaterMarkWords();
    if (millis() - lastStackLogMs >= 5000) {
      LOG_INFO("[STACK] loopTask HWM=%u words, displayTask HWM=%u words",
               (unsigned int)loopStackHighWaterWords, (unsigned int)displayStackHighWaterWords);
      lastStackLogMs = millis();
    }

    if (spiStressPattern) {
      DisplayEmu::drawSpiStressPattern(spiStressFallback ? TFT_SPI_FALLBACK_HZ : TFT_SPI_HZ,
                                       spiStressFrame++);
    } else {
      DisplayEmu::drawDiagnosticsDashboard(millis(), freeDram, freePsram, freeIram, btnMask,
                                           largestPsramBlock, (uint32_t)SDCard::getSkippedRomCount(),
                                           loopStackHighWaterWords, displayStackHighWaterWords,
                                           SDCard::isMounted(), SDCard::getClockHz());
    }

    const unsigned long elapsed = millis() - diagFrameStart;
    if (elapsed < 16) delay(16 - elapsed);

  } else if (currentState == STATE_IDLE_MASCOT) {
    const unsigned long idleStart = millis();
    Buttons::update();

    const auto& bUp    = Buttons::get(Buttons::UP);
    const auto& bDown  = Buttons::get(Buttons::DOWN);
    const auto& bLeft  = Buttons::get(Buttons::LEFT);
    const auto& bRight = Buttons::get(Buttons::RIGHT);
    const auto& bA     = Buttons::get(Buttons::A);
    const auto& bB     = Buttons::get(Buttons::B);
    const auto& bSel   = Buttons::get(Buttons::SELECT);
    const auto& bStart = Buttons::get(Buttons::START);

    // Gaze Direction Tracking from D-Pad
    float gx = 0.0f;
    float gy = 0.0f;
    if (bLeft.pressed)  gx -= 0.85f;
    if (bRight.pressed) gx += 0.85f;
    if (bUp.pressed)    gy += 0.85f;
    if (bDown.pressed)  gy -= 0.85f;

    if (gx != 0.0f || gy != 0.0f) {
      BmoFace::setGaze(gx, gy);
      lastInputActivityMs = millis();

      // Tickle detection: rapid direction toggling pets companion
      static unsigned long lastDirChangeMs = 0;
      static int dirToggleCount = 0;
      static float lastGx = 0.0f;
      if ((gx > 0.0f && lastGx < 0.0f) || (gx < 0.0f && lastGx > 0.0f)) {
        if (millis() - lastDirChangeMs < 350) {
          dirToggleCount++;
          if (dirToggleCount >= 2) {
            BmoFace::petCompanion();
            dirToggleCount = 0;
          }
        } else {
          dirToggleCount = 0;
        }
        lastDirChangeMs = millis();
      }
      lastGx = gx;
    }

    // Konami Code Easter Egg: UP, UP, DOWN, DOWN, LEFT, RIGHT, LEFT, RIGHT, B, A
    static const int kKonamiSeq[10] = {
      Buttons::UP, Buttons::UP, Buttons::DOWN, Buttons::DOWN,
      Buttons::LEFT, Buttons::RIGHT, Buttons::LEFT, Buttons::RIGHT,
      Buttons::B, Buttons::A
    };
    static int s_konamiStep = 0;
    for (int bi = 0; bi < Buttons::count(); ++bi) {
      const auto& bs = Buttons::get(bi);
      if (bs.pressed && bs.changed) {
        if (bi == kKonamiSeq[s_konamiStep]) {
          s_konamiStep++;
          if (s_konamiStep == 10) {
            s_konamiStep = 0;
            BmoFace::triggerDance(140.0f);
            BmoFace::triggerWink();
            BmoFace::say("KONAMI CODE! +30 LIVES FOR BMO!", 4500);
            DisplayEmu::showToast("* KONAMI CODE UNLOCKED! *", Theme::BMO_DPAD_YELLOW, 2500);
          }
        } else {
          s_konamiStep = (bi == kKonamiSeq[0]) ? 1 : 0;
        }
        break;
      }
    }

    // Interactive button triggers
    if (bA.pressed && bA.changed) {
      BmoFace::triggerWink();
      BmoFace::say("BMO is camera ready! *wink*", 3000);
      lastInputActivityMs = millis();
    } else if (bB.pressed && bB.changed) {
      BmoFace::triggerBlush();
      BmoFace::say("Oh, you are making BMO blush!", 3000);
      lastInputActivityMs = millis();
    } else if (bSel.pressed && bSel.changed) {
      static const char* const BMO_QUOTES[] = {
        "Who wants to play video games?!",
        "Yay! BMO is so happy to see you!",
        "Sometimes life is scary, but we have games!",
        "Yes, Finn. It goes in my butt.",
        "BMO chop! If this were a real attack, you'd be dead!",
        "When bad things happen, I know you want to believe they're a joke.",
        "I am a little living boy!"
      };
      static int s_quoteIdx = 0;
      BmoFace::setExpression(BmoFace::JOY);
      BmoFace::say(BMO_QUOTES[s_quoteIdx % 7], 3800);
      s_quoteIdx++;
      lastInputActivityMs = millis();
    } else if (bUp.pressed && bUp.changed) {
      if (BmoFace::isDancing()) {
        BmoFace::stopDance();
        BmoFace::say("Whew! What a fun dance party!", 2500);
      } else {
        BmoFace::triggerDance(125.0f);
      }
      lastInputActivityMs = millis();
    }

    // Wake Up / Return to Menu: START button or hold B
    static unsigned long btnHoldStart = 0;
    bool returnPressed = (bStart.pressed && bStart.changed);
    if (bB.pressed) {
      if (btnHoldStart == 0) btnHoldStart = millis();
    } else {
      btnHoldStart = 0;
    }

    if (returnPressed || (btnHoldStart != 0 && (millis() - btnHoldStart > 600))) {
      currentState = STATE_CONSOLE_MENU;
      BmoFace::setExpression(BmoFace::HAPPY);
      BmoFace::clearSpeech();
      lastInputActivityMs = millis();
      lastButtonMs = millis();
      btnHoldStart = 0;
    } else {
      unsigned long idleSec = (millis() - lastInputActivityMs) / 1000;
      if (idleSec > 45 && BmoFace::getExpression() != BmoFace::SLEEPING) {
        BmoFace::setExpression(BmoFace::SLEEPING);
      } else if (idleSec > 20 && idleSec <= 45 && BmoFace::getExpression() == BmoFace::IDLE) {
        BmoFace::setExpression(BmoFace::SLEEPY);
      }
      DisplayEmu::drawIdleMascotScreen(idleSec);
    }

    const unsigned long elapsed = millis() - idleStart;
    if (elapsed < 16) delay(16 - elapsed);

  } else if (currentState == STATE_GAME_MENU) {
    const unsigned long menuFrameStart = millis();
    DisplayEmu::initMenuUI();
    Buttons::update();
    bool left = Buttons::get(Buttons::LEFT).pressed && Buttons::get(Buttons::LEFT).changed;
    bool right = Buttons::get(Buttons::RIGHT).pressed && Buttons::get(Buttons::RIGHT).changed;
    bool up = Buttons::get(Buttons::UP).pressed && Buttons::get(Buttons::UP).changed;
    bool down = Buttons::get(Buttons::DOWN).pressed && Buttons::get(Buttons::DOWN).changed;
    bool a = Buttons::get(Buttons::A).pressed && Buttons::get(Buttons::A).changed;
    bool b = Buttons::get(Buttons::B).pressed && Buttons::get(Buttons::B).changed;
    bool select = Buttons::get(Buttons::SELECT).pressed && Buttons::get(Buttons::SELECT).changed;
    bool start = Buttons::get(Buttons::START).pressed && Buttons::get(Buttons::START).changed;

    if (visibleGamesDirty) {
      rebuildVisibleGames();
    }
    if (canPress()) {
      if (start && CONSOLES[selectedConsoleIndex] != ROM_FAVORITES) {
        s_topPicksOnly = !s_topPicksOnly;
        DisplayEmu::setTopPicksFilter(s_topPicksOnly);
        visibleGamesDirty = true;
        selectedGameIndex = 0;
        rebuildVisibleGames();
        if (visibleGameCount > 0 && selectedGame() && selectedGame()->hasBoxArt) {
          BoxArt::load(selectedGame()->filename);
        } else {
          BoxArt::unload();
        }
        DisplayEmu::showToast(s_topPicksOnly ? "FILTER: TOP PICKS ONLY" : "FILTER: ALL GAMES",
                              s_topPicksOnly ? Theme::BMO_SCREEN_MINT : Theme::BMO_BODY_TEAL, 1600);
        lastButtonMs = millis();
      }
      if (select && visibleGameCount > 0) {
        int romIdx = visibleRomIndexes[selectedGameIndex];
        SDCard::toggleFavorite(romIdx);
        s_consoleCountsDirty = true;   // Favorites badge in console menu needs refresh
        
        // Show BMO celebratory wink with sparkle burst when starring a game
        if (SDCard::isFavorite(romIdx)) {
          BmoFace::triggerWink();
          DisplayEmu::showToast("* ADDED TO FAVORITES", Theme::BMO_DPAD_YELLOW, 1600);
        } else {
          BmoFace::setExpression(BmoFace::IDLE);
          DisplayEmu::showToast("REMOVED FROM FAVORITES", Theme::BMO_BODY_TEAL, 1600);
        }
        
        if (CONSOLES[selectedConsoleIndex] == ROM_FAVORITES) {
          visibleGamesDirty = true;
          rebuildVisibleGames();
          if (visibleGameCount > 0 && selectedGame() && selectedGame()->hasBoxArt) {
            BoxArt::load(selectedGame()->filename);
          } else {
            BoxArt::unload();
          }
        }
        lastButtonMs = millis();
      }
      int prevGameIndex = selectedGameIndex;
      auto romInitialLetter = [](const RomFile* rf) -> char {
        if (!rf || !rf->filename[0]) return 'A';
        const char* base = strrchr(rf->filename, '/');
        if (!base) base = strrchr(rf->filename, '\\');
        base = base ? (base + 1) : rf->filename;
        return base[0] ? (char)toupper((unsigned char)base[0]) : 'A';
      };
      if (left && visibleGameCount > 0) {
        // Alphabetical reverse skip: find previous ROM with different starting letter
        const RomFile* cur = selectedGame();
        char curLetter = romInitialLetter(cur);
        int targetIdx = selectedGameIndex;
        for (int step = 1; step < visibleGameCount; ++step) {
          int testIdx = (selectedGameIndex - step + visibleGameCount) % visibleGameCount;
          const RomFile* testRom = SDCard::getRomInfo(visibleRomIndexes[testIdx]);
          if (testRom && testRom->filename[0]) {
            char testLetter = romInitialLetter(testRom);
            if (testLetter != curLetter) {
              targetIdx = testIdx;
              break;
            }
          }
        }
        selectedGameIndex = targetIdx;
        const RomFile* newRom = selectedGame();
        if (newRom && newRom->filename[0]) {
          DisplayEmu::showAlphaBadge(romInitialLetter(newRom), 800);
        }
        lastButtonMs = millis();
      }
      if (right && visibleGameCount > 0) {
        // Alphabetical forward skip: find next ROM with different starting letter
        const RomFile* cur = selectedGame();
        char curLetter = romInitialLetter(cur);
        int targetIdx = selectedGameIndex;
        for (int step = 1; step < visibleGameCount; ++step) {
          int testIdx = (selectedGameIndex + step) % visibleGameCount;
          const RomFile* testRom = SDCard::getRomInfo(visibleRomIndexes[testIdx]);
          if (testRom && testRom->filename[0]) {
            char testLetter = romInitialLetter(testRom);
            if (testLetter != curLetter) {
              targetIdx = testIdx;
              break;
            }
          }
        }
        selectedGameIndex = targetIdx;
        const RomFile* newRom = selectedGame();
        if (newRom && newRom->filename[0]) {
          DisplayEmu::showAlphaBadge(romInitialLetter(newRom), 800);
        }
        lastButtonMs = millis();
      }
      if (up && visibleGameCount > 0) {
        selectedGameIndex = (selectedGameIndex - 1 + visibleGameCount) % visibleGameCount;
        lastButtonMs = millis();
      }
      if (down && visibleGameCount > 0) {
        selectedGameIndex = (selectedGameIndex + 1) % visibleGameCount;
        lastButtonMs = millis();
      }
      if (selectedGameIndex != prevGameIndex) {
        if (visibleGameCount > 0 && selectedGame() && selectedGame()->hasBoxArt) {
          BoxArt::load(selectedGame()->filename);
        } else {
          BoxArt::unload();
        }
      }
      if (b) {
        BoxArt::unload();
        s_topPicksOnly = false;
        DisplayEmu::setTopPicksFilter(false);
        currentState = STATE_CONSOLE_MENU;
        BmoFace::setExpression(BmoFace::IDLE);
        lastButtonMs = millis();
      } else if (a && visibleGameCount > 0) {
        BoxArt::unload();
        const RomFile* selectedRom = selectedGame();
        if (!selectedRom) {
          currentState = STATE_CONSOLE_MENU;
          BmoFace::setExpression(BmoFace::IDLE);
          return;
        }

        DisplayEmu::cleanupMenuUI();
        DisplayEmu::clearScreen();

        if (selectedRom->type == ROM_WAD && !BMO_ENABLE_DOOM) {
          LOG_WARN_STR("DOOM disabled at compile time (-DBMO_ENABLE_DOOM=1 to enable)");
          DisplayEmu::showToast("DOOM CORE OFF", Theme::ACCENT_RED, 2000);
          delay(1200);
          if (selectedRom->hasBoxArt) BoxArt::load(selectedRom->filename);
          currentState = STATE_GAME_MENU;
          return;
        }
        if (selectedRom->type >= ROM_SMS && !BMO_EXPERIMENTAL_CORES) {
          LOG_WARN_STR("Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
          DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2000);
          delay(1200);
          if (selectedRom->hasBoxArt) BoxArt::load(selectedRom->filename);
          currentState = STATE_GAME_MENU;
          return;
        }

        LOG_INFO("PSRAM launch state (%s): free=%u B, largest_block=%u B",
                 selectedRom->filename,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        size_t romSize = 0;
        // Doom reads its WAD directly from the SD VFS. Loading a second full
        // copy into PSRAM wastes memory and can prevent the engine from
        // reserving the large working heap it needs.
        uint8_t* romData = selectedRom->type == ROM_WAD
                         ? nullptr
                         : SDCard::loadRom(selectedRom->filename, &romSize);
        
        if (selectedRom->type != ROM_WAD && !romData) {
          LOG_ERROR_STR("Failed to load ROM from SD card.");
          DisplayEmu::showSDCardWarning();
          delay(2000);
          if (selectedRom->hasBoxArt) BoxArt::load(selectedRom->filename);
          currentState = STATE_GAME_MENU;
          return;
        }

        bool success = false;
        const CoreRegistry::CoreDescriptor* activeCore = CoreRegistry::findByRomType(selectedRom->type);
        if (activeCore && activeCore->start) {
          success = activeCore->start(selectedRom, romData, romSize);
          selectedEmulatorIndex = activeCore->emulatorIndex;
        }
        
        if (!success) {
          LOG_ERROR_STR("Failed to start emulator. Check errors.");
          SDCard::freeRom(romData);
          currentRomBuffer = nullptr;
          currentRomBufferSize = 0;
          if (selectedRom->hasBoxArt) BoxArt::load(selectedRom->filename);
          currentState = STATE_GAME_MENU;
          BmoFace::setExpression(BmoFace::IDLE);
          lastButtonMs = millis();
          return;
        }
        
        currentRomBuffer = romData; // Track it globally so we can free it later
        currentRomBufferSize = romSize;
        strncpy(currentRomFilename, selectedRom->filename, sizeof(currentRomFilename) - 1);
        currentRomFilename[sizeof(currentRomFilename) - 1] = '\0';
        SDCard::recordRecentLaunch(currentRomFilename);

        // Transparent Auto-Load Battery RAM (.sav)
        if (activeCore && activeCore->loadBatteryRam && currentGameHasBatterySaveSupport()) {
          activeCore->loadBatteryRam(currentRomFilename);
        }

        resetFrameStats();
        resetFramePercentiles();
        currentState = STATE_EMULATOR;
        // Show a brief HAPPY expression as a "game launching" beat before
        // gameplay starts.  draw() is called once here; update()/draw() are
        // NOT called during STATE_EMULATOR so the face never appears in-game.
        BmoFace::setExpression(BmoFace::HAPPY);
        BmoFace::draw(); // one-shot launch celebration; large centered blit
        lastButtonMs = millis();
        delay(400);      // Allow launch celebration face to be seen before first emu frame
        DisplayEmu::drawConsoleBezel(selectedEmulatorIndex);
        return;
      }
    }
    
    // visibleGames[] is rebuilt inside rebuildVisibleGames() (dirty-flag guarded).
    //          No unconditional O(N) pointer-copy on every frame.
    DisplayEmu::drawGameSelectMenu(visibleGames, visibleGameCount, selectedGameIndex,
                                   CONSOLES[selectedConsoleIndex], SDCard::isMounted());

    const unsigned long menuElapsed = millis() - menuFrameStart;
    if (menuElapsed < 16) delay(16 - menuElapsed);

  } else if (currentState == STATE_EMULATOR) {
    unsigned long frameStart = micros();

    Buttons::update();
    bool select = Buttons::get(Buttons::SELECT).pressed;
    bool start  = Buttons::get(Buttons::START).pressed && Buttons::get(Buttons::START).changed;
    bool up     = Buttons::get(Buttons::UP).pressed;
    bool down   = Buttons::get(Buttons::DOWN).pressed && Buttons::get(Buttons::DOWN).changed;
    bool right  = Buttons::get(Buttons::RIGHT).pressed;
    bool btnA   = Buttons::get(Buttons::A).pressed && Buttons::get(Buttons::A).changed;
    bool btnB   = Buttons::get(Buttons::B).pressed && Buttons::get(Buttons::B).changed;

    // In-Game Quick Pause & Save State Overlay Menu: SELECT + START
    if (select && start) {
      currentState = STATE_PAUSE_MENU;
      pauseOption = 0;
      pauseToast[0] = '\0';
      s_pauseMetaDirty = true;
      delay(200);
      return;
    }

    // Quick Save Hotkey: SELECT + A
    if (select && btnA) {
      int slot = SaveManager::getActiveSlot();
      quickSaveState(slot);
      s_pauseMetaDirty = true;
      delay(200);
      return;
    }

    // Quick Load Hotkey: SELECT + B
    if (select && btnB) {
      int slot = SaveManager::getActiveSlot();
      quickLoadState(slot);
      delay(200);
      return;
    }

    // Runtime DMG Palette Switcher: SELECT + DOWN
    const CoreRegistry::CoreDescriptor* activeCore = CoreRegistry::findByIndex(selectedEmulatorIndex);
    if (select && down && activeCore && activeCore->supportsDmgPalette) {
      DisplayEmu::cycleDmgPalette();
    }

    // Toggle Rapid Auto-Fire Turbo: SELECT + LEFT
    bool leftEdge = Buttons::get(Buttons::LEFT).pressed && Buttons::get(Buttons::LEFT).changed;
    if (select && leftEdge) {
      Buttons::toggleTurbo();
    }

    // SELECT + UP opens the quit action; leaving requires a second A press.
    if (select && up) {
      currentState = STATE_PAUSE_MENU;
      pauseOption = 5;
      PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Cancel, false);
      s_pauseQuitArmedAt = 0;
      snprintf(pauseToast, sizeof(pauseToast), "A TO CONFIRM QUIT; B TO RESUME");
      s_pauseMetaDirty = true;
      delay(200);
      return;
    }

    if (activeCore && activeCore->stepFrame) {
      activeCore->stepFrame();
    }

    unsigned long elapsed = micros() - frameStart;
    const unsigned long targetFrameUs = FramePacing::targetFrameUs(selectedEmulatorIndex);

    // BM1: Record this frame's time for diagnostics.
    recordFrameTime(elapsed, selectedEmulatorIndex);
    
    // ---------------------------------------------------------------------------
    // Use nominal per-core pacing budgets; fast-forward bypasses the wait.
    // Use delay() for the bulk sleep (yields to FreeRTOS/watchdog), then
    // ets_delay_us() for a clean hardware-timer spin on the sub-ms remainder.
    // SELECT + RIGHT activates Fast-Forward (skips pacing delay).
    // ---------------------------------------------------------------------------
    const bool fastForwardActive = select && right;
    if (!fastForwardActive) {
      if (elapsed < targetFrameUs) {
        unsigned long remaining = targetFrameUs - elapsed;
        if (remaining > 1000) {
          // Sleep bulk margin with 800us spin-tail guard
          delay((remaining - 800) / 1000);
        }
        // Re-measure after sleep, then use hardware-timer spin for the tail.
        elapsed = micros() - frameStart;
        if (elapsed < targetFrameUs) {
          ets_delay_us(targetFrameUs - elapsed);
        }
      } else {
        droppedFrames++;
        totalDroppedFramesThisSecond++;
      }
    }

    // FPS + frame timing diagnostics reported once per second.
    frames++;
    unsigned long now = millis();
    if (now - lastTime >= 1000) {
      unsigned long avg = (frameTimeCount > 0) ? frameTimeSum / frameTimeCount : 0;
      if (totalDroppedFramesThisSecond > 0) {
        LOG_INFO("FPS: %d | Emu: %d target=%luus | Frame: avg=%luus min=%luus max=%luus | Dropped: %d (total %d)",
                      frames, selectedEmulatorIndex, targetFrameUs, avg, frameTimeMin, frameTimeMax,
                      totalDroppedFramesThisSecond, droppedFrames);
      } else {
        LOG_INFO("FPS: %d | Emu: %d target=%luus | Frame: avg=%luus min=%luus max=%luus",
                      frames, selectedEmulatorIndex, targetFrameUs, avg, frameTimeMin, frameTimeMax);
      }
      frames = 0;
      totalDroppedFramesThisSecond = 0;
      lastTime = now;
      resetFrameStats();
    }
  } else if (currentState == STATE_PAUSE_MENU) {
    Buttons::update();
    bool up    = Buttons::get(Buttons::UP).pressed && Buttons::get(Buttons::UP).changed;
    bool down  = Buttons::get(Buttons::DOWN).pressed && Buttons::get(Buttons::DOWN).changed;
    bool left  = Buttons::get(Buttons::LEFT).pressed && Buttons::get(Buttons::LEFT).changed;
    bool right = Buttons::get(Buttons::RIGHT).pressed && Buttons::get(Buttons::RIGHT).changed;
    bool btnA  = Buttons::get(Buttons::A).pressed && Buttons::get(Buttons::A).changed;
    bool btnB  = Buttons::get(Buttons::B).pressed && Buttons::get(Buttons::B).changed;
    bool start = Buttons::get(Buttons::START).pressed && Buttons::get(Buttons::START).changed;
    bool sel   = Buttons::get(Buttons::SELECT).pressed && Buttons::get(Buttons::SELECT).changed;

    if (s_pauseQuitState.confirmationArmed && (uint32_t)(millis() - s_pauseQuitArmedAt) > 3000U) {
      PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Timeout, false);
      s_pauseQuitArmedAt = 0;
    }

    if (up) {
      PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Cancel, false);
      pauseOption = (pauseOption + 5) % 6;
      pauseToast[0] = '\0';
    } else if (down) {
      PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Cancel, false);
      pauseOption = (pauseOption + 1) % 6;
      pauseToast[0] = '\0';
    } else if (left) {
      PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Cancel, false);
      bool swapped = Buttons::toggleSwapAB();
      snprintf(pauseToast, sizeof(pauseToast), swapped ? "A/B SWAP: ON" : "A/B SWAP: OFF");
    } else if (right) {
      PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Cancel, false);
      bool bezel = DisplayEmu::toggleBezel();
      snprintf(pauseToast, sizeof(pauseToast), bezel ? "CONSOLE BEZEL: ON" : "CONSOLE BEZEL: OFF");
    } else if (sel) {
      PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Screenshot, false);
      int shotW = 0, shotH = 0;
      const uint16_t* shotFb = DisplayEmu::getLastFramebuffer(&shotW, &shotH);
      if (shotFb && SaveManager::saveScreenshotBmp(currentRomFilename, shotFb, shotW, shotH)) {
        snprintf(pauseToast, sizeof(pauseToast), "SCREENSHOT SAVED!");
      } else {
        snprintf(pauseToast, sizeof(pauseToast), "SCREENSHOT FAILED!");
      }
    } else if (btnB || start) {
      const PauseQuit::Action action = start && s_pauseQuitState.saveFailed
          ? PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::QuitAnyway, false)
          : PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Resume, false);
      if (action == PauseQuit::Action::Quit) {
        finishQuitToConsole();
        return;
      }
      s_pauseQuitArmedAt = 0;
      DisplayEmu::drawConsoleBezel(selectedEmulatorIndex);
      currentState = STATE_EMULATOR;
      delay(200);
      return;
    } else if (btnA) {
      int slot = SaveManager::getActiveSlot();
      if (pauseOption == 0) {
        DisplayEmu::drawConsoleBezel(selectedEmulatorIndex);
        currentState = STATE_EMULATOR;
        delay(200);
        return;
      } else if (pauseOption == 1) {
        if (quickSaveState(slot)) {
          s_pauseMetaDirty = true;
          snprintf(pauseToast, sizeof(pauseToast), "SAVED TO SLOT %d!", slot);
        } else {
          snprintf(pauseToast, sizeof(pauseToast), "SAVE FAILED!");
        }
      } else if (pauseOption == 2) {
        if (quickLoadState(slot)) {
          DisplayEmu::drawConsoleBezel(selectedEmulatorIndex);
          currentState = STATE_EMULATOR;
          delay(200);
          return;
        } else {
          snprintf(pauseToast, sizeof(pauseToast), "SLOT %d EMPTY!", slot);
        }
      } else if (pauseOption == 3) {
        SaveManager::cycleActiveSlot();
        s_pauseMetaDirty = true;
        snprintf(pauseToast, sizeof(pauseToast), "ACTIVE SLOT: %d", SaveManager::getActiveSlot());
      } else if (pauseOption == 4) {
        s_pauseMetaDirty = true;
        if (!currentGameHasBatterySaveSupport()) {
          snprintf(pauseToast, sizeof(pauseToast), "NO BATTERY SAVE");
        } else if (autoSaveCurrentBatteryRam()) {
          snprintf(pauseToast, sizeof(pauseToast), "BATTERY RAM SAVED!");
        } else {
          snprintf(pauseToast, sizeof(pauseToast), "SAVE FAILED!");
        }
      } else if (pauseOption == 5) {
        const bool batterySaveSupported = currentGameHasBatterySaveSupport();
        PauseQuit::Action action = PauseQuit::transition(
            s_pauseQuitState, PauseQuit::Input::Confirm, batterySaveSupported);
        if (action == PauseQuit::Action::Stay && s_pauseQuitState.confirmationArmed) {
          s_pauseQuitArmedAt = millis();
          snprintf(pauseToast, sizeof(pauseToast), "PRESS A AGAIN TO QUIT; B TO RESUME");
        } else if (action == PauseQuit::Action::AttemptSave) {
          s_pauseQuitArmedAt = 0;
          if (autoSaveCurrentBatteryRam()) {
            action = PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::SaveSucceeded, batterySaveSupported);
          } else {
            action = PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::SaveFailed, batterySaveSupported);
            s_pauseMetaDirty = true;
            snprintf(pauseToast, sizeof(pauseToast), "SAVE FAILED: A RETRY, START QUIT");
          }
        }
        if (action == PauseQuit::Action::Quit) {
          s_pauseQuitArmedAt = 0;
          finishQuitToConsole();
          return;
        }
      }
    }

    int currentSlot = SaveManager::getActiveSlot();
    const bool batterySaveSupported = currentGameHasBatterySaveSupport();
    if (s_pauseMetaDirty) {
      s_pauseHasSave = SaveManager::hasSaveState(currentRomFilename, currentSlot);
      s_pauseHasBatt = batterySaveSupported && SaveManager::hasBatteryRam(currentRomFilename);
      s_pauseMetaDirty = false;
    }

    DisplayEmu::drawPauseMenu(currentRomFilename, currentSlot, s_pauseHasSave,
                               s_pauseHasBatt, batterySaveSupported, pauseOption, pauseToast);
    pauseToast[0] = '\0';
    delay(30);
  }
}
