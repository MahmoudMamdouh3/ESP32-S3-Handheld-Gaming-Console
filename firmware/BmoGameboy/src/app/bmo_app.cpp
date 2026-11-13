#include "bmo_app.h"
#include "boot_safe_mode.h"

#include "../cores/core_registry.h"
#include "../hal/battery.h"
#include "../hal/buttons.h"
#include "../hal/config.h"
#include "../hal/spi_arbiter.h"
#include "../storage/battery_save.h"
#include "../storage/box_art.h"
#include "../storage/rom_index.h"
#include "../storage/save_manager.h"
#include "../storage/sd_card.h"
#include "../tests/unit_tests.h"
#include "../ui/bmo_face.h"
#include "../ui/console_catalog.h"
#include "../ui/display_emu.h"
#include "../ui/frame_pacing.h"
#include "../ui/frame_percentiles.h"
#include "../ui/pause_quit_policy.h"
#include "../ui/theme.h"

#include <Preferences.h>
#include <SPI.h>
#include <ctype.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <limits.h>
#include <rom/ets_sys.h>
#include <stdlib.h>

namespace {

constexpr unsigned long IDLE_MASCOT_TIMEOUT_MS = 30000;
constexpr const char* kDisplayPrefsNamespace = "bmo_disp";
constexpr const char* kDisplaySpiHzKey = "spi_hz";

BmoApp::SystemState s_currentState = BmoApp::STATE_BOOT_SPLASH;
unsigned long s_bootSplashStartMs = 0;
unsigned long s_lastInputActivityMs = 0;

int s_selectedConsoleIndex = 0;
int s_selectedEmulatorIndex = 0;
int s_selectedGameIndex = 0;

int s_fallbackRomIndexes[32];
int* s_visibleRomIndexes = s_fallbackRomIndexes;
const RomFile* s_fallbackVisibleGames[32];
const RomFile** s_visibleGames = s_fallbackVisibleGames;
int s_maxVisibleCapacity = 32;
int s_visibleGameCount = 0;

uint8_t* s_currentRomBuffer = nullptr;
size_t s_currentRomBufferSize = 0;
char s_currentRomFilename[64] = "";

int s_pauseOption = 0;
PauseQuit::State s_pauseQuitState;
unsigned long s_pauseQuitArmedAt = 0;
char s_pauseToast[48] = "";
bool s_pauseMetaDirty = true;
bool s_pauseHasSave = false;
bool s_pauseHasBatt = false;

bool s_launchCelebrationPending = false;
unsigned long s_launchCelebrationStartMs = 0;
bool s_hotkeyCooldownActive = false;
unsigned long s_hotkeyCooldownStartMs = 0;

unsigned long s_frameTimeMin = ULONG_MAX;
unsigned long s_frameTimeMax = 0;
unsigned long s_frameTimeSum = 0;
int s_frameTimeCount = 0;
unsigned long s_framePercentileSamples[FramePercentiles::WINDOW_SIZE];
size_t s_framePercentileCount = 0;

unsigned long s_lastFpsLogMs = 0;
int s_frames = 0;
int s_droppedFrames = 0;
int s_totalDroppedFramesThisSecond = 0;

bool s_visibleGamesDirty = true;
bool s_topPicksOnly = false;
int s_cachedConsoleCounts[ConsoleCatalog::CONSOLE_COUNT] = {0};
bool s_consoleCountsDirty = true;

void syncVisiblePoolCapacity() {
  int targetCap = SDCard::computePoolCapacity(SDCard::getRomCount());
  if (targetCap < SDCard::getPoolCapacity()) {
    targetCap = SDCard::getPoolCapacity();
  }
  if (targetCap <= s_maxVisibleCapacity) return;

  int* psramIndexes = (int*)heap_caps_malloc(sizeof(int) * (size_t)targetCap, MALLOC_CAP_SPIRAM);
  const RomFile** psramGames =
      (const RomFile**)heap_caps_malloc(sizeof(const RomFile*) * (size_t)targetCap, MALLOC_CAP_SPIRAM);
  if (!psramIndexes || !psramGames) {
    if (psramIndexes) heap_caps_free(psramIndexes);
    if (psramGames) heap_caps_free(psramGames);
    LOG_WARN_STR("PSRAM game-menu allocation failed; using the current bounded capacity.");
    return;
  }

  if (s_visibleRomIndexes != s_fallbackRomIndexes) heap_caps_free(s_visibleRomIndexes);
  if (s_visibleGames != s_fallbackVisibleGames) heap_caps_free(s_visibleGames);
  s_visibleRomIndexes = psramIndexes;
  s_visibleGames = psramGames;
  s_maxVisibleCapacity = targetCap;
  s_visibleGameCount = 0;
}

bool currentGameHasBatterySaveSupport() {
  if (!s_currentRomBuffer) return false;
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(s_selectedEmulatorIndex);
  if (!core || !core->hasBatterySave) return false;
  return core->hasBatterySave(s_currentRomBuffer, s_currentRomBufferSize);
}

bool autoSaveCurrentBatteryRam() {
  if (s_currentRomFilename[0] == '\0' || !currentGameHasBatterySaveSupport()) return false;
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(s_selectedEmulatorIndex);
  return core && core->saveBatteryRam ? core->saveBatteryRam(s_currentRomFilename) : false;
}

bool quickSaveState(int slot) {
  if (s_currentRomFilename[0] == '\0') return false;
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(s_selectedEmulatorIndex);
  return core && core->saveState ? core->saveState(s_currentRomFilename, slot) : false;
}

bool quickLoadState(int slot) {
  if (s_currentRomFilename[0] == '\0') return false;
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(s_selectedEmulatorIndex);
  return core && core->loadState ? core->loadState(s_currentRomFilename, slot) : false;
}

void destroyActiveEmulator() {
  DisplayEmu::waitForDisplay();
  const CoreRegistry::CoreDescriptor* core = CoreRegistry::findByIndex(s_selectedEmulatorIndex);
  if (core && core->destroy) {
    core->destroy();
  }

  if (s_currentRomBuffer) {
    SDCard::freeRom(s_currentRomBuffer);
    s_currentRomBuffer = nullptr;
  }
  s_currentRomBufferSize = 0;
  s_currentRomFilename[0] = '\0';
  s_launchCelebrationPending = false;
  DisplayEmu::resetLastFramebuffer();
}

void finishQuitToConsole() {
  destroyActiveEmulator();
  s_currentState = BmoApp::STATE_CONSOLE_MENU;
  BmoFace::setExpression(BmoFace::IDLE);
  Buttons::suppressMenuInputUntil((uint32_t)(millis() + 300U));
}

inline void recordFrameTime(unsigned long us, int emulatorIndex) {
  if (us < s_frameTimeMin) s_frameTimeMin = us;
  if (us > s_frameTimeMax) s_frameTimeMax = us;
  s_frameTimeSum += us;
  s_frameTimeCount++;
  s_framePercentileSamples[s_framePercentileCount++] = us;
  if (s_framePercentileCount == FramePercentiles::WINDOW_SIZE) {
    FramePercentiles::Summary percentiles;
    if (FramePercentiles::sortAndSummarize(s_framePercentileSamples, FramePercentiles::WINDOW_SIZE, percentiles)) {
      LOG_INFO("Frame window: emu=%d n=600 target=%luus p50=%luus p95=%luus p99=%luus (core execution)", emulatorIndex,
               FramePacing::targetFrameUs(emulatorIndex), percentiles.p50, percentiles.p95, percentiles.p99);
    }
    s_framePercentileCount = 0;
  }
}

void resetFrameStats() {
  s_frameTimeMin = ULONG_MAX;
  s_frameTimeMax = 0;
  s_frameTimeSum = 0;
  s_frameTimeCount = 0;
}

void resetFramePercentiles() {
  s_framePercentileCount = 0;
}

void rebuildVisibleGames() {
  if (!s_visibleGamesDirty) return;
  s_visibleGameCount = 0;
  const RomType selectedType = ConsoleCatalog::CONSOLES[s_selectedConsoleIndex];
  const int totalRoms = SDCard::getRomCount();

  if (selectedType == ROM_FAVORITES) {
    for (int i = 0; i < totalRoms && s_visibleGameCount < s_maxVisibleCapacity; ++i) {
      const RomFile* game = SDCard::getRomInfo(i);
      if (game && game->isFavorite) {
        s_visibleRomIndexes[s_visibleGameCount++] = i;
      }
    }
  } else {
    for (int i = 0; i < totalRoms && s_visibleGameCount < s_maxVisibleCapacity; ++i) {
      const RomFile* game = SDCard::getRomInfo(i);
      if (game && game->type == selectedType && (!s_topPicksOnly || game->isPopular)) {
        s_visibleRomIndexes[s_visibleGameCount++] = i;
      }
    }
    if (s_topPicksOnly && s_visibleGameCount == 0) {
      for (int i = 0; i < totalRoms && s_visibleGameCount < s_maxVisibleCapacity; ++i) {
        const RomFile* game = SDCard::getRomInfo(i);
        if (game && game->type == selectedType) {
          s_visibleRomIndexes[s_visibleGameCount++] = i;
        }
      }
    }
  }

  for (int i = 0; i < s_visibleGameCount; ++i) {
    s_visibleGames[i] = SDCard::getRomInfo(s_visibleRomIndexes[i]);
  }

  if (s_visibleGameCount == 0) {
    s_selectedGameIndex = 0;
  } else if (s_selectedGameIndex >= s_visibleGameCount) {
    s_selectedGameIndex = s_visibleGameCount - 1;
  }
  s_visibleGamesDirty = false;
}

const RomFile* selectedGame() {
  if (s_selectedGameIndex < 0 || s_selectedGameIndex >= s_visibleGameCount) return nullptr;
  return SDCard::getRomInfo(s_visibleRomIndexes[s_selectedGameIndex]);
}

void refreshSelectedBoxArt() {
  const RomFile* rom = (s_visibleGameCount > 0) ? selectedGame() : nullptr;
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
  return base[0] ? (char)toupper((unsigned char)base[0]) : 'A';
}

void paceMenuFrame(unsigned long frameStartMs, unsigned long targetMs = 16UL) {
  const unsigned long elapsed = millis() - frameStartMs;
  if (elapsed < targetMs) delay(targetMs - elapsed);
}

void handleBootSplash() {
  const unsigned long splashStart = millis();
  DisplayEmu::initMenuUI();
  Buttons::update(false);
  const auto& input = Buttons::inputFrame();

  if (input.justPressedMask != 0U || (millis() - s_bootSplashStartMs) > 3500UL) {
    s_currentState = BmoApp::STATE_CONSOLE_MENU;
    s_lastInputActivityMs = millis();
    Buttons::suppressMenuInputUntil((uint32_t)(millis() + 220U));
    return;
  }

  const bool blinkState = ((millis() / 400UL) % 2UL == 0UL);
  DisplayEmu::drawBootSplash(blinkState);
  paceMenuFrame(splashStart, 33UL);
}

void handleConsoleMenu() {
  const unsigned long menuFrameStart = millis();
  DisplayEmu::initMenuUI();
  Buttons::update(true);
  const auto& input = Buttons::inputFrame();
  const unsigned long nowMs = millis();

  const bool left = input.triggeredOrRepeat(Buttons::LEFT);
  const bool right = input.triggeredOrRepeat(Buttons::RIGHT);
  const bool up = input.edgePressed(Buttons::UP);
  const bool down = input.edgePressed(Buttons::DOWN);
  const bool a = input.edgePressed(Buttons::A);
  const bool select = input.edgePressed(Buttons::SELECT);
  const bool start = input.edgePressed(Buttons::START);

  if (left || right || up || down || a || select || start) {
    s_lastInputActivityMs = nowMs;
  }
  if (left) {
    s_selectedConsoleIndex =
        (s_selectedConsoleIndex - 1 + ConsoleCatalog::CONSOLE_COUNT) % ConsoleCatalog::CONSOLE_COUNT;
    s_visibleGamesDirty = true;
    BmoFace::setGaze(-0.70f, 0.0f);
    DisplayEmu::triggerCarouselGlide(false);
  }
  if (right) {
    s_selectedConsoleIndex = (s_selectedConsoleIndex + 1) % ConsoleCatalog::CONSOLE_COUNT;
    s_visibleGamesDirty = true;
    BmoFace::setGaze(0.70f, 0.0f);
    DisplayEmu::triggerCarouselGlide(true);
  }
  if (up) {
    s_currentState = BmoApp::STATE_IDLE_MASCOT;
    BmoFace::setExpression(BmoFace::HAPPY);
    BmoFace::say("Who wants to play video games?!", 3500);
    Buttons::suppressMenuInputUntil((uint32_t)(nowMs + 180U));
  } else if (down || select) {
    s_currentState = BmoApp::STATE_CONSOLE_MUSEUM;
    Buttons::suppressMenuInputUntil((uint32_t)(nowMs + 180U));
  } else if (start) {
    s_currentState = BmoApp::STATE_DIAGNOSTICS;
    Buttons::suppressMenuInputUntil((uint32_t)(nowMs + 180U));
  } else if (a) {
    s_selectedGameIndex = 0;
    s_visibleGamesDirty = true;
    rebuildVisibleGames();
    s_currentState = BmoApp::STATE_GAME_MENU;
    BmoFace::setExpression(BmoFace::IDLE);
    refreshSelectedBoxArt();
    Buttons::suppressMenuInputUntil((uint32_t)(nowMs + 180U));
  }

  if (millis() - s_lastInputActivityMs > IDLE_MASCOT_TIMEOUT_MS) {
    s_currentState = BmoApp::STATE_IDLE_MASCOT;
    BmoFace::setExpression(BmoFace::SLEEPY);
  }

  if (s_consoleCountsDirty) {
    for (int i = 0; i < ConsoleCatalog::CONSOLE_COUNT; ++i) {
      s_cachedConsoleCounts[i] = SDCard::getRomCountForType(ConsoleCatalog::CONSOLES[i]);
    }
    s_consoleCountsDirty = false;
  }
  DisplayEmu::drawConsoleSelectMenu(s_selectedConsoleIndex, s_cachedConsoleCounts, ConsoleCatalog::CONSOLE_COUNT,
                                    SDCard::isMounted());
  paceMenuFrame(menuFrameStart);
}

void handleConsoleMuseum() {
  const unsigned long museumFrameStart = millis();
  DisplayEmu::initMenuUI();
  Buttons::update(false);
  const auto& input = Buttons::inputFrame();

  if (input.edgePressed(Buttons::B) || input.edgePressed(Buttons::SELECT) || input.edgePressed(Buttons::A)) {
    s_currentState = BmoApp::STATE_CONSOLE_MENU;
    Buttons::suppressMenuInputUntil((uint32_t)(millis() + 180U));
  }

  DisplayEmu::drawConsoleMuseumModal(ConsoleCatalog::CONSOLES[s_selectedConsoleIndex]);
  paceMenuFrame(museumFrameStart);
}

void handleDiagnostics() {
  static bool spiStressPattern = false;
  static uint32_t spiStressFrame = 0;
  static unsigned long lastStackLogMs = 0;
  const unsigned long diagFrameStart = millis();

  DisplayEmu::initMenuUI();
  Buttons::update(false);
  const auto& input = Buttons::inputFrame();
  const uint16_t btnMask = Buttons::rawMask();
  bool spiStressFallback = (DisplayEmu::getDiagnosticSpiClock() == TFT_SPI_FALLBACK_HZ);

  if (input.edgePressed(Buttons::B)) {
    spiStressPattern = false;
    s_currentState = BmoApp::STATE_CONSOLE_MENU;
    Buttons::suppressMenuInputUntil((uint32_t)(millis() + 180U));
  } else if (input.edgePressed(Buttons::A)) {
    spiStressPattern = !spiStressPattern;
  } else if (spiStressPattern && (input.edgePressed(Buttons::LEFT) || input.edgePressed(Buttons::RIGHT))) {
    spiStressFallback = !spiStressFallback;
    const uint32_t nextSpiHz = spiStressFallback ? TFT_SPI_FALLBACK_HZ : TFT_SPI_HZ;
    DisplayEmu::setDiagnosticSpiClock(nextSpiHz);
    persistDisplaySpiClock(nextSpiHz);
  } else if (!spiStressPattern && input.edgePressed(Buttons::SELECT)) {
    if (!SDCard::isMounted()) {
      const bool mounted = SDCard::begin();
      if (mounted) {
        SaveManager::begin();
        syncVisiblePoolCapacity();
        s_consoleCountsDirty = true;
        s_visibleGamesDirty = true;
      }
      DisplayEmu::showToast(mounted ? "SD CARD RETRY SUCCEEDED" : "SD CARD STILL NOT FOUND", Theme::BMO_DPAD_YELLOW,
                            3000);
    } else {
      DisplayEmu::showToast("RESCANNING SD LIBRARY", Theme::BMO_DPAD_YELLOW, 10000);
      SDCard::rebuildIndex();
      syncVisiblePoolCapacity();
      s_consoleCountsDirty = true;
      s_visibleGamesDirty = true;
      DisplayEmu::showToast(SDCard::getSkippedRomCount() == 0 ? "LIBRARY RESCAN COMPLETE"
                                                              : "RESCAN COMPLETE; SOME PATHS SKIPPED",
                            Theme::BMO_DPAD_YELLOW, 3000);
    }
  }

  const uint32_t freeDram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  const uint32_t largestPsramBlock = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  const uint32_t freeIram = heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT);
  const uint32_t loopStackHighWaterWords = (uint32_t)uxTaskGetStackHighWaterMark(nullptr);
  const uint32_t displayStackHighWaterWords = DisplayEmu::getDisplayStackHighWaterMarkWords();
  if (millis() - lastStackLogMs >= 5000) {
    LOG_INFO("[STACK] loopTask HWM=%u words, displayTask HWM=%u words", (unsigned int)loopStackHighWaterWords,
             (unsigned int)displayStackHighWaterWords);
    lastStackLogMs = millis();
  }

  if (spiStressPattern) {
    DisplayEmu::drawSpiStressPattern(spiStressFallback ? TFT_SPI_FALLBACK_HZ : TFT_SPI_HZ, spiStressFrame++);
  } else {
    DisplayEmu::drawDiagnosticsDashboard(millis(), freeDram, freePsram, freeIram, btnMask, largestPsramBlock,
                                         (uint32_t)SDCard::getSkippedRomCount(), loopStackHighWaterWords,
                                         displayStackHighWaterWords, SDCard::isMounted(), SDCard::getClockHz());
  }

  paceMenuFrame(diagFrameStart);
}

void handleIdleMascot() {
  const unsigned long idleStart = millis();
  Buttons::update(false);
  const auto& input = Buttons::inputFrame();

  float gx = 0.0f;
  float gy = 0.0f;
  if (Buttons::get(Buttons::LEFT).pressed) gx -= 0.85f;
  if (Buttons::get(Buttons::RIGHT).pressed) gx += 0.85f;
  if (Buttons::get(Buttons::UP).pressed) gy += 0.85f;
  if (Buttons::get(Buttons::DOWN).pressed) gy -= 0.85f;

  if (gx != 0.0f || gy != 0.0f) {
    BmoFace::setGaze(gx, gy);
    s_lastInputActivityMs = millis();

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

  static constexpr int kKonamiSeq[10] = {Buttons::UP,    Buttons::UP,   Buttons::DOWN,  Buttons::DOWN, Buttons::LEFT,
                                         Buttons::RIGHT, Buttons::LEFT, Buttons::RIGHT, Buttons::B,    Buttons::A};
  static int s_konamiStep = 0;
  for (int bi = 0; bi < Buttons::count(); ++bi) {
    if (input.edgePressed(bi)) {
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

  if (input.edgePressed(Buttons::A)) {
    BmoFace::triggerWink();
    BmoFace::say("BMO is camera ready! *wink*", 3000);
    s_lastInputActivityMs = millis();
  } else if (input.edgePressed(Buttons::B)) {
    BmoFace::triggerBlush();
    BmoFace::say("Oh, you are making BMO blush!", 3000);
    s_lastInputActivityMs = millis();
  } else if (input.edgePressed(Buttons::SELECT)) {
    static const char* const BMO_QUOTES[] = {"Who wants to play video games?!",
                                             "Yay! BMO is so happy to see you!",
                                             "Sometimes life is scary, but we have games!",
                                             "Yes, Finn. It goes in my butt.",
                                             "BMO chop! If this were a real attack, you'd be dead!",
                                             "When bad things happen, I know you want to believe they're a joke.",
                                             "I am a little living boy!"};
    static int s_quoteIdx = 0;
    BmoFace::setExpression(BmoFace::JOY);
    BmoFace::say(BMO_QUOTES[s_quoteIdx % 7], 3800);
    s_quoteIdx++;
    s_lastInputActivityMs = millis();
  } else if (input.edgePressed(Buttons::UP)) {
    if (BmoFace::isDancing()) {
      BmoFace::stopDance();
      BmoFace::say("Whew! What a fun dance party!", 2500);
    } else {
      BmoFace::triggerDance(125.0f);
    }
    s_lastInputActivityMs = millis();
  }

  static unsigned long btnHoldStart = 0;
  const bool returnPressed = input.edgePressed(Buttons::START);
  if (Buttons::get(Buttons::B).pressed) {
    if (btnHoldStart == 0) btnHoldStart = millis();
  } else {
    btnHoldStart = 0;
  }

  if (returnPressed || (btnHoldStart != 0 && (millis() - btnHoldStart > 600))) {
    s_currentState = BmoApp::STATE_CONSOLE_MENU;
    BmoFace::setExpression(BmoFace::HAPPY);
    BmoFace::clearSpeech();
    s_lastInputActivityMs = millis();
    Buttons::suppressMenuInputUntil((uint32_t)(millis() + 220U));
    btnHoldStart = 0;
  } else {
    const unsigned long idleSec = (millis() - s_lastInputActivityMs) / 1000;
    if (idleSec > 45 && BmoFace::getExpression() != BmoFace::SLEEPING) {
      BmoFace::setExpression(BmoFace::SLEEPING);
    } else if (idleSec > 20 && idleSec <= 45 && BmoFace::getExpression() == BmoFace::IDLE) {
      BmoFace::setExpression(BmoFace::SLEEPY);
    }
    DisplayEmu::drawIdleMascotScreen(idleSec);
  }

  paceMenuFrame(idleStart);
}

void launchSelectedRom(const RomFile* selectedRom) {
  BoxArt::unload();
  if (!selectedRom) {
    s_currentState = BmoApp::STATE_CONSOLE_MENU;
    BmoFace::setExpression(BmoFace::IDLE);
    return;
  }

  if (selectedRom->type == ROM_WAD && !BMO_ENABLE_DOOM) {
    LOG_WARN_STR("DOOM disabled at compile time (-DBMO_ENABLE_DOOM=1 to enable)");
    DisplayEmu::showToast("DOOM CORE OFF", Theme::ACCENT_RED, 2000);
    refreshSelectedBoxArt();
    Buttons::suppressMenuInputUntil((uint32_t)(millis() + 250U));
    return;
  }
  if (selectedRom->type >= ROM_SMS && !BMO_EXPERIMENTAL_CORES) {
    LOG_WARN_STR("Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
    DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2000);
    refreshSelectedBoxArt();
    Buttons::suppressMenuInputUntil((uint32_t)(millis() + 250U));
    return;
  }

  DisplayEmu::cleanupMenuUI();
  DisplayEmu::clearScreen();

  LOG_INFO("PSRAM launch state (%s): free=%u B, largest_block=%u B", selectedRom->filename,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

  size_t romSize = 0;
  uint8_t* romData = (selectedRom->type == ROM_WAD) ? nullptr : SDCard::loadRom(selectedRom->filename, &romSize);

  if (selectedRom->type != ROM_WAD && !romData) {
    LOG_ERROR_STR("Failed to load ROM from SD card.");
    DisplayEmu::showToast("SD READ FAILED: CHECK CARD", Theme::BMO_CORAL_RED, 2200);
    refreshSelectedBoxArt();
    s_currentState = BmoApp::STATE_GAME_MENU;
    Buttons::suppressMenuInputUntil((uint32_t)(millis() + 300U));
    return;
  }

  bool success = false;
  const CoreRegistry::CoreDescriptor* activeCore = CoreRegistry::findByRomType(selectedRom->type);
  if (activeCore && activeCore->start) {
    success = activeCore->start(selectedRom, romData, romSize);
    s_selectedEmulatorIndex = activeCore->emulatorIndex;
  }

  if (!success) {
    LOG_ERROR_STR("Failed to start emulator. Check errors.");
    SDCard::freeRom(romData);
    s_currentRomBuffer = nullptr;
    s_currentRomBufferSize = 0;
    DisplayEmu::showToast("EMULATOR START FAILED", Theme::BMO_CORAL_RED, 2200);
    refreshSelectedBoxArt();
    s_currentState = BmoApp::STATE_GAME_MENU;
    BmoFace::setExpression(BmoFace::IDLE);
    Buttons::suppressMenuInputUntil((uint32_t)(millis() + 300U));
    return;
  }

  s_currentRomBuffer = romData;
  s_currentRomBufferSize = romSize;
  strncpy(s_currentRomFilename, selectedRom->filename, sizeof(s_currentRomFilename) - 1);
  s_currentRomFilename[sizeof(s_currentRomFilename) - 1] = '\0';
  SDCard::recordRecentLaunch(s_currentRomFilename);

  if (activeCore && activeCore->loadBatteryRam && currentGameHasBatterySaveSupport()) {
    activeCore->loadBatteryRam(s_currentRomFilename);
  }

  resetFrameStats();
  resetFramePercentiles();
  s_currentState = BmoApp::STATE_EMULATOR;
  s_launchCelebrationPending = true;
  s_launchCelebrationStartMs = millis();
  s_hotkeyCooldownActive = false;
  BmoFace::setExpression(BmoFace::HAPPY);
  BmoFace::draw();
}

void handleGameMenu() {
  const unsigned long menuFrameStart = millis();
  DisplayEmu::initMenuUI();
  Buttons::update(true);
  const auto& input = Buttons::inputFrame();

  if (s_visibleGamesDirty) {
    rebuildVisibleGames();
  }

  const bool left = input.triggeredOrRepeat(Buttons::LEFT);
  const bool right = input.triggeredOrRepeat(Buttons::RIGHT);
  const bool up = input.triggeredOrRepeat(Buttons::UP);
  const bool down = input.triggeredOrRepeat(Buttons::DOWN);
  const bool a = input.edgePressed(Buttons::A);
  const bool b = input.edgePressed(Buttons::B);
  const bool select = input.edgePressed(Buttons::SELECT);
  const bool start = input.edgePressed(Buttons::START);

  if (start && ConsoleCatalog::CONSOLES[s_selectedConsoleIndex] != ROM_FAVORITES) {
    s_topPicksOnly = !s_topPicksOnly;
    DisplayEmu::setTopPicksFilter(s_topPicksOnly);
    s_visibleGamesDirty = true;
    s_selectedGameIndex = 0;
    rebuildVisibleGames();
    refreshSelectedBoxArt();
    DisplayEmu::showToast(s_topPicksOnly ? "FILTER: TOP PICKS ONLY" : "FILTER: ALL GAMES",
                          s_topPicksOnly ? Theme::BMO_SCREEN_MINT : Theme::BMO_BODY_TEAL, 1600);
  }
  if (select && s_visibleGameCount > 0) {
    const int romIdx = s_visibleRomIndexes[s_selectedGameIndex];
    SDCard::toggleFavorite(romIdx);
    s_consoleCountsDirty = true;

    if (SDCard::isFavorite(romIdx)) {
      BmoFace::triggerWink();
      DisplayEmu::showToast("* ADDED TO FAVORITES", Theme::BMO_DPAD_YELLOW, 1600);
    } else {
      BmoFace::setExpression(BmoFace::IDLE);
      DisplayEmu::showToast("REMOVED FROM FAVORITES", Theme::BMO_BODY_TEAL, 1600);
    }

    if (ConsoleCatalog::CONSOLES[s_selectedConsoleIndex] == ROM_FAVORITES) {
      s_visibleGamesDirty = true;
      rebuildVisibleGames();
      refreshSelectedBoxArt();
    }
  }

  const int prevGameIndex = s_selectedGameIndex;
  if (left && s_visibleGameCount > 0) {
    const RomFile* cur = selectedGame();
    const char curLetter = romInitialLetter(cur);
    int targetIdx = s_selectedGameIndex;
    for (int step = 1; step < s_visibleGameCount; ++step) {
      const int testIdx = (s_selectedGameIndex - step + s_visibleGameCount) % s_visibleGameCount;
      const RomFile* testRom = SDCard::getRomInfo(s_visibleRomIndexes[testIdx]);
      if (testRom && testRom->filename[0] && romInitialLetter(testRom) != curLetter) {
        targetIdx = testIdx;
        break;
      }
    }
    s_selectedGameIndex = targetIdx;
    const RomFile* newRom = selectedGame();
    if (newRom && newRom->filename[0]) {
      DisplayEmu::showAlphaBadge(romInitialLetter(newRom), 800);
    }
  }
  if (right && s_visibleGameCount > 0) {
    const RomFile* cur = selectedGame();
    const char curLetter = romInitialLetter(cur);
    int targetIdx = s_selectedGameIndex;
    for (int step = 1; step < s_visibleGameCount; ++step) {
      const int testIdx = (s_selectedGameIndex + step) % s_visibleGameCount;
      const RomFile* testRom = SDCard::getRomInfo(s_visibleRomIndexes[testIdx]);
      if (testRom && testRom->filename[0] && romInitialLetter(testRom) != curLetter) {
        targetIdx = testIdx;
        break;
      }
    }
    s_selectedGameIndex = targetIdx;
    const RomFile* newRom = selectedGame();
    if (newRom && newRom->filename[0]) {
      DisplayEmu::showAlphaBadge(romInitialLetter(newRom), 800);
    }
  }
  if (up && s_visibleGameCount > 0) {
    s_selectedGameIndex = (s_selectedGameIndex - 1 + s_visibleGameCount) % s_visibleGameCount;
  }
  if (down && s_visibleGameCount > 0) {
    s_selectedGameIndex = (s_selectedGameIndex + 1) % s_visibleGameCount;
  }
  if (s_selectedGameIndex != prevGameIndex) {
    refreshSelectedBoxArt();
  }

  if (b) {
    BoxArt::unload();
    s_topPicksOnly = false;
    DisplayEmu::setTopPicksFilter(false);
    s_currentState = BmoApp::STATE_CONSOLE_MENU;
    BmoFace::setExpression(BmoFace::IDLE);
    Buttons::suppressMenuInputUntil((uint32_t)(millis() + 180U));
  } else if (a && s_visibleGameCount > 0) {
    launchSelectedRom(selectedGame());
    return;
  }

  DisplayEmu::drawGameSelectMenu(s_visibleGames, s_visibleGameCount, s_selectedGameIndex,
                                 ConsoleCatalog::CONSOLES[s_selectedConsoleIndex], SDCard::isMounted());
  paceMenuFrame(menuFrameStart);
}

void handleEmulator() {
  if (s_launchCelebrationPending) {
    Buttons::update(false);
    if ((unsigned long)(millis() - s_launchCelebrationStartMs) < 400UL) {
      delay(16);
      return;
    }
    s_launchCelebrationPending = false;
    DisplayEmu::drawConsoleBezel(s_selectedEmulatorIndex);
    s_lastFpsLogMs = millis();
    return;
  }

  const unsigned long frameStart = micros();
  Buttons::update(false);

  const bool select = Buttons::get(Buttons::SELECT).pressed;
  const bool start = Buttons::get(Buttons::START).pressed && Buttons::get(Buttons::START).changed;
  const bool up = Buttons::get(Buttons::UP).pressed;
  const bool downEdge = Buttons::get(Buttons::DOWN).pressed && Buttons::get(Buttons::DOWN).changed;
  const bool right = Buttons::get(Buttons::RIGHT).pressed;
  const bool btnA = Buttons::get(Buttons::A).pressed && Buttons::get(Buttons::A).changed;
  const bool btnB = Buttons::get(Buttons::B).pressed && Buttons::get(Buttons::B).changed;
  const bool leftEdge = Buttons::get(Buttons::LEFT).pressed && Buttons::get(Buttons::LEFT).changed;

  if (s_hotkeyCooldownActive && (unsigned long)(millis() - s_hotkeyCooldownStartMs) >= 250UL) {
    s_hotkeyCooldownActive = false;
  }

  if (!s_hotkeyCooldownActive) {
    if (select && start) {
      s_currentState = BmoApp::STATE_PAUSE_MENU;
      s_pauseOption = 0;
      s_pauseToast[0] = '\0';
      s_pauseMetaDirty = true;
      Buttons::suppressMenuInputUntil((uint32_t)(millis() + 250U));
      return;
    }

    if (select && btnA) {
      quickSaveState(SaveManager::getActiveSlot());
      s_pauseMetaDirty = true;
      s_hotkeyCooldownActive = true;
      s_hotkeyCooldownStartMs = millis();
      return;
    }

    if (select && btnB) {
      quickLoadState(SaveManager::getActiveSlot());
      s_hotkeyCooldownActive = true;
      s_hotkeyCooldownStartMs = millis();
      return;
    }

    if (select && up) {
      s_currentState = BmoApp::STATE_PAUSE_MENU;
      s_pauseOption = 5;
      PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Cancel, false);
      s_pauseQuitArmedAt = 0;
      snprintf(s_pauseToast, sizeof(s_pauseToast), "A TO CONFIRM QUIT; B TO RESUME");
      s_pauseMetaDirty = true;
      Buttons::suppressMenuInputUntil((uint32_t)(millis() + 250U));
      return;
    }
  }

  const CoreRegistry::CoreDescriptor* activeCore = CoreRegistry::findByIndex(s_selectedEmulatorIndex);
  if (select && downEdge && activeCore && activeCore->supportsDmgPalette) {
    DisplayEmu::cycleDmgPalette();
  }
  if (select && leftEdge) {
    Buttons::toggleTurbo();
  }

  if (activeCore && activeCore->stepFrame) {
    activeCore->stepFrame();
  }

  unsigned long elapsed = micros() - frameStart;
  const unsigned long targetFrameUs = FramePacing::targetFrameUs(s_selectedEmulatorIndex);
  recordFrameTime(elapsed, s_selectedEmulatorIndex);

  const bool fastForwardActive = select && right;
  if (!fastForwardActive) {
    if (elapsed < targetFrameUs) {
      const unsigned long remaining = targetFrameUs - elapsed;
      if (remaining > 1000) {
        delay((remaining - 800) / 1000);
      }
      elapsed = micros() - frameStart;
      if (elapsed < targetFrameUs) {
        ets_delay_us(targetFrameUs - elapsed);
      }
    } else {
      s_droppedFrames++;
      s_totalDroppedFramesThisSecond++;
    }
  }

  s_frames++;
  const unsigned long now = millis();
  if (now - s_lastFpsLogMs >= 1000) {
    const unsigned long avg = (s_frameTimeCount > 0) ? s_frameTimeSum / s_frameTimeCount : 0;
    if (s_totalDroppedFramesThisSecond > 0) {
      LOG_INFO("FPS: %d | Emu: %d target=%luus | Frame: avg=%luus min=%luus max=%luus | Dropped: %d (total %d)",
               s_frames, s_selectedEmulatorIndex, targetFrameUs, avg, s_frameTimeMin, s_frameTimeMax,
               s_totalDroppedFramesThisSecond, s_droppedFrames);
    } else {
      LOG_INFO("FPS: %d | Emu: %d target=%luus | Frame: avg=%luus min=%luus max=%luus", s_frames,
               s_selectedEmulatorIndex, targetFrameUs, avg, s_frameTimeMin, s_frameTimeMax);
    }
    s_frames = 0;
    s_totalDroppedFramesThisSecond = 0;
    s_lastFpsLogMs = now;
    resetFrameStats();
  }
}

void handlePauseMenu() {
  const unsigned long pauseStart = millis();
  Buttons::update(true);
  const auto& input = Buttons::inputFrame();

  const bool up = input.triggeredOrRepeat(Buttons::UP);
  const bool down = input.triggeredOrRepeat(Buttons::DOWN);
  const bool left = input.edgePressed(Buttons::LEFT);
  const bool right = input.edgePressed(Buttons::RIGHT);
  const bool btnA = input.edgePressed(Buttons::A);
  const bool btnB = input.edgePressed(Buttons::B);
  const bool start = input.edgePressed(Buttons::START);
  const bool sel = input.edgePressed(Buttons::SELECT);

  if (s_pauseQuitState.confirmationArmed && (uint32_t)(millis() - s_pauseQuitArmedAt) > 3000U) {
    PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Timeout, false);
    s_pauseQuitArmedAt = 0;
  }

  if (up) {
    PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Cancel, false);
    s_pauseOption = (s_pauseOption + 5) % 6;
    s_pauseToast[0] = '\0';
  } else if (down) {
    PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Cancel, false);
    s_pauseOption = (s_pauseOption + 1) % 6;
    s_pauseToast[0] = '\0';
  } else if (left) {
    PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Cancel, false);
    const bool swapped = Buttons::toggleSwapAB();
    snprintf(s_pauseToast, sizeof(s_pauseToast), swapped ? "A/B SWAP: ON" : "A/B SWAP: OFF");
  } else if (right) {
    PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Cancel, false);
    const bool bezel = DisplayEmu::toggleBezel();
    snprintf(s_pauseToast, sizeof(s_pauseToast), bezel ? "CONSOLE BEZEL: ON" : "CONSOLE BEZEL: OFF");
  } else if (sel) {
    PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Screenshot, false);
    int shotW = 0, shotH = 0;
    const uint16_t* shotFb = DisplayEmu::getLastFramebuffer(&shotW, &shotH);
    if (shotFb && SaveManager::saveScreenshotBmp(s_currentRomFilename, shotFb, shotW, shotH)) {
      snprintf(s_pauseToast, sizeof(s_pauseToast), "SCREENSHOT SAVED!");
    } else {
      snprintf(s_pauseToast, sizeof(s_pauseToast), "SCREENSHOT FAILED!");
    }
  } else if (btnB || start) {
    const PauseQuit::Action action = (start && s_pauseQuitState.saveFailed)
                                         ? PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::QuitAnyway, false)
                                         : PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Resume, false);
    if (action == PauseQuit::Action::Quit) {
      finishQuitToConsole();
      return;
    }
    s_pauseQuitArmedAt = 0;
    DisplayEmu::drawConsoleBezel(s_selectedEmulatorIndex);
    s_currentState = BmoApp::STATE_EMULATOR;
    s_hotkeyCooldownActive = true;
    s_hotkeyCooldownStartMs = millis();
    return;
  } else if (btnA) {
    const int slot = SaveManager::getActiveSlot();
    if (s_pauseOption == 0) {
      DisplayEmu::drawConsoleBezel(s_selectedEmulatorIndex);
      s_currentState = BmoApp::STATE_EMULATOR;
      s_hotkeyCooldownActive = true;
      s_hotkeyCooldownStartMs = millis();
      return;
    } else if (s_pauseOption == 1) {
      if (quickSaveState(slot)) {
        s_pauseMetaDirty = true;
        snprintf(s_pauseToast, sizeof(s_pauseToast), "SAVED TO SLOT %d!", slot);
      } else {
        snprintf(s_pauseToast, sizeof(s_pauseToast), "SAVE FAILED!");
      }
    } else if (s_pauseOption == 2) {
      if (quickLoadState(slot)) {
        DisplayEmu::drawConsoleBezel(s_selectedEmulatorIndex);
        s_currentState = BmoApp::STATE_EMULATOR;
        s_hotkeyCooldownActive = true;
        s_hotkeyCooldownStartMs = millis();
        return;
      } else {
        snprintf(s_pauseToast, sizeof(s_pauseToast), "SLOT %d EMPTY!", slot);
      }
    } else if (s_pauseOption == 3) {
      SaveManager::cycleActiveSlot();
      s_pauseMetaDirty = true;
      snprintf(s_pauseToast, sizeof(s_pauseToast), "ACTIVE SLOT: %d", SaveManager::getActiveSlot());
    } else if (s_pauseOption == 4) {
      s_pauseMetaDirty = true;
      if (!currentGameHasBatterySaveSupport()) {
        snprintf(s_pauseToast, sizeof(s_pauseToast), "NO BATTERY SAVE");
      } else if (autoSaveCurrentBatteryRam()) {
        snprintf(s_pauseToast, sizeof(s_pauseToast), "BATTERY RAM SAVED!");
      } else {
        snprintf(s_pauseToast, sizeof(s_pauseToast), "SAVE FAILED!");
      }
    } else if (s_pauseOption == 5) {
      const bool batterySaveSupported = currentGameHasBatterySaveSupport();
      PauseQuit::Action action =
          PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::Confirm, batterySaveSupported);
      if (action == PauseQuit::Action::Stay && s_pauseQuitState.confirmationArmed) {
        s_pauseQuitArmedAt = millis();
        snprintf(s_pauseToast, sizeof(s_pauseToast), "PRESS A AGAIN TO QUIT; B TO RESUME");
      } else if (action == PauseQuit::Action::AttemptSave) {
        s_pauseQuitArmedAt = 0;
        if (autoSaveCurrentBatteryRam()) {
          action = PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::SaveSucceeded, batterySaveSupported);
        } else {
          action = PauseQuit::transition(s_pauseQuitState, PauseQuit::Input::SaveFailed, batterySaveSupported);
          s_pauseMetaDirty = true;
          snprintf(s_pauseToast, sizeof(s_pauseToast), "SAVE FAILED: A RETRY, START QUIT");
        }
      }
      if (action == PauseQuit::Action::Quit) {
        s_pauseQuitArmedAt = 0;
        finishQuitToConsole();
        return;
      }
    }
  }

  const int currentSlot = SaveManager::getActiveSlot();
  const bool batterySaveSupported = currentGameHasBatterySaveSupport();
  if (s_pauseMetaDirty) {
    s_pauseHasSave = SaveManager::hasSaveState(s_currentRomFilename, currentSlot);
    s_pauseHasBatt = batterySaveSupported && SaveManager::hasBatteryRam(s_currentRomFilename);
    s_pauseMetaDirty = false;
  }

  DisplayEmu::drawPauseMenu(s_currentRomFilename, currentSlot, s_pauseHasSave, s_pauseHasBatt, batterySaveSupported,
                            s_pauseOption, s_pauseToast);
  s_pauseToast[0] = '\0';
  paceMenuFrame(pauseStart);
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

  LOG_INFO_STR("\n\n--- BOOTING ---");
  LOG_INFO_STR("Milestone 4: Game Selection UI");
  LOG_INFO("Features: SD=%d, Audio=%d, Battery=%d", FEATURE_SD_CARD, FEATURE_AUDIO, FEATURE_BATTERY_MONITOR);

  SpiArbiter::init();
  SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, -1);

#ifdef ENABLE_UNIT_TESTS
  LOG_INFO_STR("Booting into Test Mode...");
  const bool all_passed = runAllTests();
  LOG_INFO_STR(all_passed ? "TESTS SUCCESS" : "TESTS FAILED");
  while (true)
    delay(100);
#endif

  Buttons::begin();
  Buttons::update(false);
  const BmoApp::BootSpiDecision spiDecision =
      BmoApp::resolveBootDisplaySpiClock(Buttons::get(Buttons::SELECT).pressed, Buttons::get(Buttons::B).pressed,
                                         loadSavedDisplaySpiClock(), TFT_SPI_HZ, TFT_SPI_FALLBACK_HZ);

  if (spiDecision.safeModeForced) {
    persistDisplaySpiClock(TFT_SPI_FALLBACK_HZ);
    LOG_WARN("[SAFE_MODE] SELECT/B held at boot; forced display SPI clock to %lu MHz",
             (unsigned long)(TFT_SPI_FALLBACK_HZ / 1000000UL));
  }
  DisplayEmu::begin(spiDecision.spiHz);
  if (spiDecision.safeModeForced) {
    DisplayEmu::showToast("SAFE MODE: 40 MHz SPI", Theme::BMO_DPAD_YELLOW, 3500);
  }
  Battery::begin();
  BmoFace::begin();
  BmoFace::setExpression(BmoFace::IDLE);
  s_bootSplashStartMs = millis();

  if (!SDCard::begin()) {
    LOG_ERROR_STR("Failed to mount SD card!");
  } else {
    LOG_INFO("SD Card mounted. Found %d ROMs.", SDCard::getRomCount());
    SaveManager::begin();
  }
  syncVisiblePoolCapacity();

  LOG_INFO("PSRAM boot state: free=%u B, largest_block=%u B, pool_cap=%d",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM), SDCard::getPoolCapacity());
  LOG_INFO("IRAM free: %u bytes", heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT));

  s_lastFpsLogMs = millis();
}

void BmoApp::loop() {
  Battery::update();
  Buttons::setGameplayMode(s_currentState == BmoApp::STATE_EMULATOR);
  if (s_currentState != BmoApp::STATE_EMULATOR && s_currentState != BmoApp::STATE_PAUSE_MENU) {
    BmoFace::update();
  }

  switch (s_currentState) {
  case BmoApp::STATE_BOOT_SPLASH:
    handleBootSplash();
    break;
  case BmoApp::STATE_CONSOLE_MENU:
    handleConsoleMenu();
    break;
  case BmoApp::STATE_CONSOLE_MUSEUM:
    handleConsoleMuseum();
    break;
  case BmoApp::STATE_GAME_MENU:
    handleGameMenu();
    break;
  case BmoApp::STATE_EMULATOR:
    handleEmulator();
    break;
  case BmoApp::STATE_PAUSE_MENU:
    handlePauseMenu();
    break;
  case BmoApp::STATE_DIAGNOSTICS:
    handleDiagnostics();
    break;
  case BmoApp::STATE_IDLE_MASCOT:
    handleIdleMascot();
    break;
  }
}
