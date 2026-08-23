#include "config.h"
#include "emu_peanut.h"
#include "emu_walnut.h"
#include "unit_tests.h"
#include "buttons.h"
#include "display_emu.h"
#include "rom_supermariobrosdeluxe.h"
#include "cover_supermariobrosdeluxe.h"
#include "rom_legendofzeldatheoracleofseasons.h"
#include "cover_legendofzeldatheoracleofseasons.h"
#include <SPI.h>
#include <rom/ets_sys.h>      // N7: ets_delay_us for tight hardware-timer spin
#include <esp_heap_caps.h>    // BM2: IRAM usage reporting

struct Game {
  const char* title;
  const uint8_t* data;
  size_t length;
  const uint16_t* cover;
};

Game games[] = {
  {"Super Mario Bros. Deluxe", rom_supermariobrosdeluxe, rom_supermariobrosdeluxe_len, cover_supermariobrosdeluxe},
  {"Zelda: Oracle of Seasons", rom_legendofzeldatheoracleofseasons, rom_legendofzeldatheoracleofseasons_len, cover_legendofzeldatheoracleofseasons}
};

// Q2: Derived at compile time — stays in sync if the game table grows.
static constexpr int NUM_GAMES = (int)(sizeof(games) / sizeof(games[0]));

enum SystemState {
  STATE_EMULATOR_SELECT,
  STATE_MENU,
  STATE_EMULATOR
};

SystemState currentState = STATE_EMULATOR_SELECT;
int selectedEmulatorIndex = 0; // 0 = Walnut-CGB, 1 = Peanut-GB
int selectedGameIndex = 0;
bool redrawMenu = true;
bool useColorEmulator = true;

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

  Serial.println("\n\n--- BOOTING ---");
  Serial.println("Milestone 4: Game Selection UI");

  // Initialize shared SPI bus before any device uses it.
  SPI.begin(TFT_SCK, -1, TFT_MOSI);

  // --- UNIT TEST RUNNER ---
  // #define ENABLE_UNIT_TESTS
  #ifdef ENABLE_UNIT_TESTS
    Serial.println("Booting into Test Mode...");
    bool all_passed = runAllTests();
    Serial.println(all_passed ? "TESTS SUCCESS" : "TESTS FAILED");
    while(1) delay(100);
  #endif

  Buttons::begin();
  DisplayEmu::begin();

  // BM2: Report IRAM free size so we can verify IRAM_ATTR budget usage.
  Serial.printf("IRAM free: %u bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT));

  // NB5: Set lastTime to NOW so the first FPS window is valid (not skewed
  // by the 3-second boot delay above).
  lastTime = millis();
}

void loop() {
  if (currentState == STATE_EMULATOR_SELECT) {
    Buttons::update();
    bool left  = Buttons::get(Buttons::LEFT).pressed  && Buttons::get(Buttons::LEFT).changed;
    bool right = Buttons::get(Buttons::RIGHT).pressed && Buttons::get(Buttons::RIGHT).changed;
    bool a     = Buttons::get(Buttons::A).pressed     && Buttons::get(Buttons::A).changed;

    if (redrawMenu) {
      DisplayEmu::drawEmulatorSelectMenu(selectedEmulatorIndex);
      redrawMenu = false;
    }

    if (canPress()) {
      if (left) {
        selectedEmulatorIndex = (selectedEmulatorIndex - 1 + 6) % 6;
        redrawMenu = true;
        lastButtonMs = millis();
      }
      if (right) {
        selectedEmulatorIndex = (selectedEmulatorIndex + 1) % 6;
        redrawMenu = true;
        lastButtonMs = millis();
      }
      if (a) {
        if (selectedEmulatorIndex == 0 || selectedEmulatorIndex == 1) {
          useColorEmulator = (selectedEmulatorIndex == 0);
          currentState = STATE_MENU;
          redrawMenu = true;
        } else {
          DisplayEmu::showSDCardWarning();
          delay(2000);
          redrawMenu = true;
        }
        lastButtonMs = millis();
      }
    }
    
    delay(16);

  } else if (currentState == STATE_MENU) {
    Buttons::update();
    bool left   = Buttons::get(Buttons::LEFT).pressed   && Buttons::get(Buttons::LEFT).changed;
    bool right  = Buttons::get(Buttons::RIGHT).pressed  && Buttons::get(Buttons::RIGHT).changed;
    bool a      = Buttons::get(Buttons::A).pressed      && Buttons::get(Buttons::A).changed;
    bool select = Buttons::get(Buttons::SELECT).pressed && Buttons::get(Buttons::SELECT).changed;

    if (redrawMenu) {
      // Q1: Build titles array inline — no redundant global gameTitles[].
      const char* titles[NUM_GAMES];
      for (int i = 0; i < NUM_GAMES; i++) titles[i] = games[i].title;
      DisplayEmu::drawMenu(titles, NUM_GAMES, selectedGameIndex,
                           games[selectedGameIndex].cover, useColorEmulator);
      redrawMenu = false;
    }

    if (canPress()) {
      if (left) {
        selectedGameIndex = (selectedGameIndex - 1 + NUM_GAMES) % NUM_GAMES;
        redrawMenu = true;
        lastButtonMs = millis();
      }
      if (right) {
        selectedGameIndex = (selectedGameIndex + 1) % NUM_GAMES;
        redrawMenu = true;
        lastButtonMs = millis();
      }
      if (select) {
        useColorEmulator = !useColorEmulator;
        redrawMenu = true;
        lastButtonMs = millis();
      }
      if (a) {
        DisplayEmu::clearScreen();
        
        bool success = false;
        if (useColorEmulator) {
          success = WalnutEmu::begin(games[selectedGameIndex].data, games[selectedGameIndex].length);
        } else {
          success = PeanutEmu::begin(games[selectedGameIndex].data, games[selectedGameIndex].length);
        }
        
        if (!success) {
          Serial.println("Failed to start emulator. Check errors.");
          while (1) delay(100);
        }
        resetFrameStats();
        currentState = STATE_EMULATOR;
        lastButtonMs = millis();
      }
    }

    delay(16);
    
  } else if (currentState == STATE_EMULATOR) {
    unsigned long frameStart = micros();

    Buttons::update();
    bool select = Buttons::get(Buttons::SELECT).pressed;
    bool up     = Buttons::get(Buttons::UP).pressed;

    // Return to menu: SELECT + UP
    if (select && up) {
      currentState = STATE_MENU;
      redrawMenu = true;
      lastButtonMs = millis();
      delay(300);
      return;
    }

    if (useColorEmulator) {
      WalnutEmu::updateJoypad();
      WalnutEmu::runFrame();
    } else {
      PeanutEmu::updateJoypad();
      PeanutEmu::runFrame();
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
        Serial.printf("FPS: %d | Frame: avg=%luus min=%luus max=%luus | Dropped: %d (total %d)\n",
                      frames, avg, frameTimeMin, frameTimeMax,
                      totalDroppedFramesThisSecond, droppedFrames);
      } else {
        Serial.printf("FPS: %d | Frame: avg=%luus min=%luus max=%luus\n",
                      frames, avg, frameTimeMin, frameTimeMax);
      }
      frames = 0;
      totalDroppedFramesThisSecond = 0;
      lastTime = now;
      resetFrameStats();
    }
  }
}
