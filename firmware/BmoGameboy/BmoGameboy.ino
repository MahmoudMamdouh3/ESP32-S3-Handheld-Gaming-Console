#include "src/core/config.h"
#include "src/emulators/emu_peanut.h"
#include "src/emulators/emu_walnut.h"
#include "src/emulators/emu_nes.h"
#include "src/emulators/emu_doom.h"
#include "src/emulators/emu_sms.h"
#include "src/emulators/emu_pce.h"
#include "src/emulators/emu_atari.h"
#include "src/emulators/emu_pico.h"
#include "src/emulators/emu_genesis.h"
#include "src/emulators/emu_snes.h"
#include "src/emulators/emu_wswan.h"
#include "src/emulators/emu_ngp.h"
#include "src/emulators/emu_lynx.h"
#include "src/emulators/emu_colem.h"
#include "src/tests/unit_tests.h"
#include "src/core/buttons.h"
#include "src/core/sd_card.h"
#include "src/core/battery.h"
#include "src/core/display_emu.h"
#include "src/core/bmo_face.h"
#include <SPI.h>
#include <rom/ets_sys.h>      // N7: ets_delay_us for tight hardware-timer spin
#include <esp_heap_caps.h>    // BM2: IRAM usage reporting

enum SystemState {
  STATE_CONSOLE_MENU,
  STATE_GAME_MENU,
  STATE_EMULATOR
};

SystemState currentState = STATE_CONSOLE_MENU;
int selectedConsoleIndex = 0;
int selectedEmulatorIndex = 0;
int selectedGameIndex = 0;
static const int MAX_VISIBLE_ROMS = 16384;
static int fallbackRomIndexes[32];
static int* visibleRomIndexes = fallbackRomIndexes;
static const RomFile* fallbackVisibleGames[32];
static const RomFile** visibleGames = fallbackVisibleGames;
static int maxVisibleCapacity = 32;
static int visibleGameCount = 0;

static const RomType CONSOLES[] = {
  ROM_GB, ROM_GBC, ROM_NES, ROM_WAD,
  ROM_SMS, ROM_GG, ROM_PCE, ROM_ATARI, ROM_PICO8,
  ROM_GENESIS, ROM_SNES, ROM_WSWAN, ROM_NGP, ROM_LYNX, ROM_COLEM
};
static const int CONSOLE_COUNT = sizeof(CONSOLES) / sizeof(CONSOLES[0]);

// Pointer to dynamically loaded ROM buffer in PSRAM
uint8_t* currentRomBuffer = nullptr;

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

static inline void recordFrameTime(unsigned long us) {
  if (us < frameTimeMin) frameTimeMin = us;
  if (us > frameTimeMax) frameTimeMax = us;
  frameTimeSum  += us;
  frameTimeCount++;
}

static void resetFrameStats() {
  frameTimeMin   = ULONG_MAX;
  frameTimeMax   = 0;
  frameTimeSum   = 0;
  frameTimeCount = 0;
}

static int countGamesForConsole(RomType type) {
  int count = 0;
  for (int i = 0; i < SDCard::getRomCount(); ++i) {
    const RomFile* game = SDCard::getRomInfo(i);
    if (game && game->type == type) ++count;
  }
  return count;
}

static void rebuildVisibleGames() {
  visibleGameCount = 0;
  const RomType selectedType = CONSOLES[selectedConsoleIndex];
  for (int i = 0; i < SDCard::getRomCount() && visibleGameCount < maxVisibleCapacity; ++i) {
    const RomFile* game = SDCard::getRomInfo(i);
    if (game && game->type == selectedType) {
      visibleRomIndexes[visibleGameCount++] = i;
    }
  }
  if (visibleGameCount == 0) selectedGameIndex = 0;
  else if (selectedGameIndex >= visibleGameCount) selectedGameIndex = visibleGameCount - 1;
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

void setup() {
  Serial.begin(115200);
  
  // Wait 3 seconds for Serial to connect (avoids hang on UART port).
  delay(3000);

  LOG_INFO_STR("\n\n--- BOOTING ---");
  LOG_INFO_STR("Milestone 4: Game Selection UI");
  LOG_INFO("Features: SD=%d, Audio=%d, Battery=%d", FEATURE_SD_CARD, FEATURE_AUDIO, FEATURE_BATTERY_MONITOR);

  // Initialize shared SPI bus before any device uses it.
  SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, -1);

  #ifdef ENABLE_UNIT_TESTS
    LOG_INFO_STR("Booting into Test Mode...");
    bool all_passed = runAllTests();
    LOG_INFO_STR(all_passed ? "TESTS SUCCESS" : "TESTS FAILED");
    while(1) delay(100);
  #endif

  Buttons::begin();
  DisplayEmu::begin();
  Battery::begin();

  // Boot splash: show the mascot face before SD scan so the user sees
  // something while the (potentially slow) SD enumeration runs.
  BmoFace::begin();
  BmoFace::setExpression(BmoFace::IDLE);
  BmoFace::draw(); // full-screen centered boot face
  delay(1000);     // Boot splash display hold
  
  int* psramIndexes = (int*)heap_caps_malloc(sizeof(int) * MAX_VISIBLE_ROMS, MALLOC_CAP_SPIRAM);
  if (psramIndexes) {
    visibleRomIndexes = psramIndexes;
    maxVisibleCapacity = MAX_VISIBLE_ROMS;
  }
  const RomFile** psramGames = (const RomFile**)heap_caps_malloc(sizeof(const RomFile*) * MAX_VISIBLE_ROMS, MALLOC_CAP_SPIRAM);
  if (psramGames) {
    visibleGames = psramGames;
  }

  if (!SDCard::begin()) {
    LOG_ERROR_STR("Failed to mount SD card!");
  } else {
    LOG_INFO("SD Card mounted. Found %d ROMs.", SDCard::getRomCount());
  }

  // BM2: Report IRAM free size so we can verify IRAM_ATTR budget usage.
  LOG_INFO("IRAM free: %u bytes",
                heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT));

  // Set lastTime to NOW so the first FPS window is valid
  lastTime = millis();

  // Expression already set to IDLE above; update() will handle blinking.
}

void loop() {
  Battery::update();
  BmoFace::update();
  
  if (currentState == STATE_CONSOLE_MENU) {
    const unsigned long menuFrameStart = millis();
    DisplayEmu::initMenuUI();
    
    Buttons::update();
    const auto& btnLeft = Buttons::get(Buttons::LEFT);
    const auto& btnRight = Buttons::get(Buttons::RIGHT);
    const auto& btnUp = Buttons::get(Buttons::UP);
    const auto& btnDown = Buttons::get(Buttons::DOWN);
    const auto& btnA = Buttons::get(Buttons::A);
    bool left   = btnLeft.pressed   && btnLeft.changed;
    bool right  = btnRight.pressed  && btnRight.changed;
    bool up     = btnUp.pressed     && btnUp.changed;
    bool down   = btnDown.pressed   && btnDown.changed;
    bool a      = btnA.pressed      && btnA.changed;
    
    if (canPress()) {
      if (left || up) {
        selectedConsoleIndex = (selectedConsoleIndex - 1 + CONSOLE_COUNT) % CONSOLE_COUNT;
        lastButtonMs = millis();
      }
      if (right || down) {
        selectedConsoleIndex = (selectedConsoleIndex + 1) % CONSOLE_COUNT;
        lastButtonMs = millis();
      }
      if (a) {
        selectedGameIndex = 0;
        rebuildVisibleGames();
        currentState = STATE_GAME_MENU;
        BmoFace::setExpression(BmoFace::IDLE);
        lastButtonMs = millis();
      }
    }

    int counts[CONSOLE_COUNT];
    for (int i = 0; i < CONSOLE_COUNT; ++i) counts[i] = countGamesForConsole(CONSOLES[i]);
    DisplayEmu::drawConsoleSelectMenu(selectedConsoleIndex, counts, CONSOLE_COUNT, SDCard::isMounted());

    // Blit the mascot face into the top-left corner on top of the menu canvas.
    // blitFace() internally caches faceBuf and recomputes SDF only when dirty (~0.4ms blit).
    BmoFace::draw(FACE_MENU_X, FACE_MENU_Y, FACE_MENU_SIZE);

    // The full-screen SPI blit already consumes most of a 16.7 ms frame.
    // Only sleep for the remaining budget; an unconditional delay(16) here
    // previously limited the menu to roughly 30 FPS.
    const unsigned long menuElapsed = millis() - menuFrameStart;
    if (menuElapsed < 16) delay(16 - menuElapsed);

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

    rebuildVisibleGames();
    if (canPress()) {
      if (left && visibleGameCount > 0) {
        selectedGameIndex = (selectedGameIndex - 1 + visibleGameCount) % visibleGameCount;
        lastButtonMs = millis();
      }
      if (right && visibleGameCount > 0) {
        selectedGameIndex = (selectedGameIndex + 1) % visibleGameCount;
        lastButtonMs = millis();
      }
      if (up && visibleGameCount > 0) {
        selectedGameIndex = (selectedGameIndex - 10 + visibleGameCount) % visibleGameCount;
        lastButtonMs = millis();
      }
      if (down && visibleGameCount > 0) {
        selectedGameIndex = (selectedGameIndex + 10) % visibleGameCount;
        lastButtonMs = millis();
      }
      if (b) {
        currentState = STATE_CONSOLE_MENU;
        BmoFace::setExpression(BmoFace::IDLE);
        lastButtonMs = millis();
      } else if (a && visibleGameCount > 0) {
        const RomFile* selectedRom = selectedGame();
        if (!selectedRom) {
          currentState = STATE_CONSOLE_MENU;
          BmoFace::setExpression(BmoFace::IDLE);
          return;
        }

        DisplayEmu::cleanupMenuUI();
        DisplayEmu::clearScreen();
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
          currentState = STATE_GAME_MENU;
          return;
        }

        bool success = false;
        
        // Boot appropriate emulator core based on file extension
        if (selectedRom->type == ROM_WAD) {
          // DOOM handles its own PSRAM loading via standard C file I/O
          static char wadPath[64]; // "/sd/" + name + NUL
          snprintf(wadPath, sizeof(wadPath), "/sd/%s", selectedRom->filename);
          success = DoomEmu::begin(wadPath);
          selectedEmulatorIndex = 3;
        } else if (selectedRom->type == ROM_NES) {
          success = NesEmu::begin(romData, romSize);
          selectedEmulatorIndex = 2; 
        } else if (selectedRom->type == ROM_GBC) {
          success = WalnutEmu::begin(romData, romSize);
          selectedEmulatorIndex = 0;
        } else if (selectedRom->type == ROM_GB) {
          success = PeanutEmu::begin(romData, romSize);
          selectedEmulatorIndex = 1;
        } else if (selectedRom->type == ROM_SMS) {
          success = SmsEmu::begin(romData, romSize, false);
          selectedEmulatorIndex = 4;
        } else if (selectedRom->type == ROM_GG) {
          success = SmsEmu::begin(romData, romSize, true);
          selectedEmulatorIndex = 5;
        } else if (selectedRom->type == ROM_PCE) {
          success = PceEmu::begin(romData, romSize);
          selectedEmulatorIndex = 6;
        } else if (selectedRom->type == ROM_ATARI) {
          success = AtariEmu::begin(romData, romSize);
          selectedEmulatorIndex = 7;
        } else if (selectedRom->type == ROM_PICO8) {
          success = PicoEmu::begin(romData, romSize);
          selectedEmulatorIndex = 8;
        } else if (selectedRom->type == ROM_GENESIS) {
          success = GenesisEmu::init(romData, romSize);
          selectedEmulatorIndex = 9;
        } else if (selectedRom->type == ROM_SNES) {
          success = SNESEmu::init(romData, romSize);
          selectedEmulatorIndex = 10;
        } else if (selectedRom->type == ROM_WSWAN) {
          success = WSwanEmu::init(romData, romSize, true);
          selectedEmulatorIndex = 11;
        } else if (selectedRom->type == ROM_NGP) {
          success = NGPEmu::init(romData, romSize, true);
          selectedEmulatorIndex = 12;
        } else if (selectedRom->type == ROM_LYNX) {
          success = LynxEmu::init(romData, romSize);
          selectedEmulatorIndex = 13;
        } else if (selectedRom->type == ROM_COLEM) {
          success = ColemEmu::init(romData, romSize);
          selectedEmulatorIndex = 14;
        }
        
        if (!success) {
          LOG_ERROR_STR("Failed to start emulator. Check errors.");
          SDCard::freeRom(romData);
          currentRomBuffer = nullptr;
          currentState = STATE_GAME_MENU;
          BmoFace::setExpression(BmoFace::IDLE);
          lastButtonMs = millis();
          return;
        }
        
        currentRomBuffer = romData; // Track it globally so we can free it later

        resetFrameStats();
        currentState = STATE_EMULATOR;
        // Show a brief HAPPY expression as a "game launching" beat before
        // gameplay starts.  draw() is called once here; update()/draw() are
        // NOT called during STATE_EMULATOR so the face never appears in-game.
        BmoFace::setExpression(BmoFace::HAPPY);
        BmoFace::draw(); // one-shot launch celebration; large centered blit
        lastButtonMs = millis();
        delay(400);      // Allow launch celebration face to be seen before first emu frame
        return;
      }
    }
    
    for (int i = 0; i < visibleGameCount; ++i) {
      visibleGames[i] = SDCard::getRomInfo(visibleRomIndexes[i]);
    }
    DisplayEmu::drawGameSelectMenu(visibleGames, visibleGameCount, selectedGameIndex,
                                   CONSOLES[selectedConsoleIndex], SDCard::isMounted());

    // Blit the mascot face into the top-left corner of the game list menu.
    BmoFace::draw(FACE_MENU_X, FACE_MENU_Y, FACE_MENU_SIZE);

    const unsigned long menuElapsed = millis() - menuFrameStart;
    if (menuElapsed < 16) delay(16 - menuElapsed);

  } else if (currentState == STATE_EMULATOR) {
    unsigned long frameStart = micros();

    Buttons::update();
    bool select = Buttons::get(Buttons::SELECT).pressed;
    bool up     = Buttons::get(Buttons::UP).pressed;

    // Return to menu: SELECT + UP
    if (select && up) {
      if (selectedEmulatorIndex == 0) {
        WalnutEmu::destroy();
      } else if (selectedEmulatorIndex == 1) {
        PeanutEmu::destroy();
      } else if (selectedEmulatorIndex == 2) {
        NesEmu::destroy();
      } else if (selectedEmulatorIndex == 3) {
        DoomEmu::destroy();
      } else if (selectedEmulatorIndex == 4 || selectedEmulatorIndex == 5) {
        SmsEmu::destroy();
      } else if (selectedEmulatorIndex == 6) {
        PceEmu::destroy();
      } else if (selectedEmulatorIndex == 7) {
        AtariEmu::destroy();
      } else if (selectedEmulatorIndex == 8) {
        PicoEmu::destroy();
      } else if (selectedEmulatorIndex == 9) {
        GenesisEmu::destroy();
      } else if (selectedEmulatorIndex == 10) {
        SNESEmu::destroy();
      } else if (selectedEmulatorIndex == 11) {
        WSwanEmu::destroy();
      } else if (selectedEmulatorIndex == 12) {
        NGPEmu::destroy();
      } else if (selectedEmulatorIndex == 13) {
        LynxEmu::destroy();
      } else if (selectedEmulatorIndex == 14) {
        ColemEmu::destroy();
      }
      
      if (currentRomBuffer) {
        SDCard::freeRom(currentRomBuffer);
        currentRomBuffer = nullptr;
      }

      currentState = STATE_CONSOLE_MENU;
      BmoFace::setExpression(BmoFace::IDLE);
      lastButtonMs = millis();
      delay(300);
      return;
    }

    if (selectedEmulatorIndex == 3) {
      DoomEmu::runFrame();
    } else if (selectedEmulatorIndex == 2) {
      NesEmu::updateJoypad();
      NesEmu::runFrame();
    } else if (selectedEmulatorIndex == 0) {
      WalnutEmu::updateJoypad();
      WalnutEmu::runFrame();
    } else if (selectedEmulatorIndex == 1) {
      PeanutEmu::updateJoypad();
      PeanutEmu::runFrame();
    } else if (selectedEmulatorIndex == 4 || selectedEmulatorIndex == 5) {
      SmsEmu::runFrame();
    } else if (selectedEmulatorIndex == 6) {
      PceEmu::runFrame();
    } else if (selectedEmulatorIndex == 7) {
      AtariEmu::runFrame();
    } else if (selectedEmulatorIndex == 8) {
      PicoEmu::runFrame();
    } else if (selectedEmulatorIndex == 9) {
      GenesisEmu::update();
    } else if (selectedEmulatorIndex == 10) {
      SNESEmu::update();
    } else if (selectedEmulatorIndex == 11) {
      WSwanEmu::update();
    } else if (selectedEmulatorIndex == 12) {
      NGPEmu::update();
    } else if (selectedEmulatorIndex == 13) {
      LynxEmu::update();
    } else if (selectedEmulatorIndex == 14) {
      ColemEmu::update();
    }

    unsigned long elapsed = micros() - frameStart;

    // BM1: Record this frame's time for diagnostics.
    recordFrameTime(elapsed);
    
    // ---------------------------------------------------------------------------
    // N7: Frame pacing — target 16742 µs (59.73 Hz Game Boy vsync).
    // Use delay() for the bulk sleep (yields to FreeRTOS/watchdog), then
    // ets_delay_us() for a clean hardware-timer spin on the sub-ms remainder.
    // This avoids the micros()-polling busy-loop that burned CPU every frame.
    // ---------------------------------------------------------------------------
    if (elapsed < 16742) {
      unsigned long remaining = 16742 - elapsed;
      if (remaining > 2000) {
        // Sleep the bulk — FreeRTOS can run WiFi/BT tasks here.
        delay((remaining - 2000) / 1000);
      }
      // Re-measure after sleep, then use hardware-timer spin for the tail.
      elapsed = micros() - frameStart;
      if (elapsed < 16742) {
        ets_delay_us(16742 - elapsed);
      }
    } else {
      droppedFrames++;
      totalDroppedFramesThisSecond++;
    }

    // FPS + frame timing diagnostics reported once per second.
    frames++;
    unsigned long now = millis();
    if (now - lastTime >= 1000) {
      unsigned long avg = (frameTimeCount > 0) ? frameTimeSum / frameTimeCount : 0;
      if (totalDroppedFramesThisSecond > 0) {
        LOG_INFO("FPS: %d | Frame: avg=%luus min=%luus max=%luus | Dropped: %d (total %d)",
                      frames, avg, frameTimeMin, frameTimeMax,
                      totalDroppedFramesThisSecond, droppedFrames);
      } else {
        LOG_INFO("FPS: %d | Frame: avg=%luus min=%luus max=%luus",
                      frames, avg, frameTimeMin, frameTimeMax);
      }
      frames = 0;
      totalDroppedFramesThisSecond = 0;
      lastTime = now;
      resetFrameStats();
    }
  }
}
