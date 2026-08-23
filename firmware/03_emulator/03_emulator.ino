#include "config.h"
#include "emu_peanut.h"
#include "emu_walnut.h"
#include "emu_nes.h"
#include "emu_doom.h"
#include "unit_tests.h"
#include "buttons.h"
#include "display_emu.h"
#include "sd_card.h"
#include <SPI.h>
#include <rom/ets_sys.h>      // N7: ets_delay_us for tight hardware-timer spin
#include <esp_heap_caps.h>    // BM2: IRAM usage reporting

enum SystemState {
  STATE_MENU,
  STATE_EMULATOR
};

SystemState currentState = STATE_MENU;
int selectedEmulatorIndex = 0; // Legacy unused var, kept for compat
int selectedGameIndex = 0;
bool useColorEmulator = true;

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
  
  if (!SDCard::begin()) {
    Serial.println("Failed to mount SD card!");
  } else {
    Serial.printf("SD Card mounted. Found %d ROMs.\n", SDCard::getRomCount());
  }

  // BM2: Report IRAM free size so we can verify IRAM_ATTR budget usage.
  Serial.printf("IRAM free: %u bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT));

  // NB5: Set lastTime to NOW so the first FPS window is valid (not skewed
  // by the 3-second boot delay above).
  lastTime = millis();
}

void loop() {
  if (currentState == STATE_MENU) {
    DisplayEmu::initMenuUI();
    
    Buttons::update();
    const auto& btnLeft = Buttons::get(Buttons::LEFT);
    const auto& btnRight = Buttons::get(Buttons::RIGHT);
    const auto& btnA = Buttons::get(Buttons::A);
    const auto& btnSelect = Buttons::get(Buttons::SELECT);
    bool left   = btnLeft.pressed   && btnLeft.changed;
    bool right  = btnRight.pressed  && btnRight.changed;
    bool a      = btnA.pressed      && btnA.changed;
    bool select = btnSelect.pressed && btnSelect.changed;
    
    int numRoms = SDCard::getRomCount();

    if (canPress() && numRoms > 0) {
      if (left) {
        selectedGameIndex = (selectedGameIndex - 1 + numRoms) % numRoms;
        lastButtonMs = millis();
      }
      if (right) {
        selectedGameIndex = (selectedGameIndex + 1) % numRoms;
        lastButtonMs = millis();
      }
      if (select) {
        useColorEmulator = !useColorEmulator;
        lastButtonMs = millis();
      }
      if (a) {
        // Prepare to launch emulator
        DisplayEmu::cleanupMenuUI();
        DisplayEmu::clearScreen();
        
        const RomFile* selectedRom = SDCard::getRomInfo(selectedGameIndex);
        size_t romSize = 0;
        uint8_t* romData = SDCard::loadRom(selectedRom->filename, &romSize);
        
        if (!romData) {
          Serial.println("Failed to load ROM from SD card.");
          DisplayEmu::showSDCardWarning();
          delay(2000);
          currentState = STATE_MENU;
          return;
        }

        bool success = false;
        
        // Boot appropriate emulator core based on file extension
        if (selectedRom->type == ROM_WAD) {
          // DOOM handles its own PSRAM loading via standard C file I/O
          char wadPath[64];
          snprintf(wadPath, sizeof(wadPath), "/sd/%s", selectedRom->filename);
          success = DoomEmu::begin(wadPath);
          selectedEmulatorIndex = 3;
          // We don't need romData for DOOM
          if (romData) {
            SDCard::freeRom(romData);
            romData = nullptr;
          }
        } else if (selectedRom->type == ROM_NES) {
          success = NesEmu::begin(romData, romSize);
          selectedEmulatorIndex = 2; 
        } else if (selectedRom->type == ROM_GBC || (selectedRom->type == ROM_GB && useColorEmulator)) {
          success = WalnutEmu::begin(romData, romSize);
          selectedEmulatorIndex = 0;
        } else {
          success = PeanutEmu::begin(romData, romSize);
          selectedEmulatorIndex = 1;
        }
        
        if (!success) {
          Serial.println("Failed to start emulator. Check errors.");
          SDCard::freeRom(romData); // Free PSRAM on failure
          while (1) delay(100);
        }
        
        currentRomBuffer = romData; // Track it globally so we can free it later

        resetFrameStats();
        currentState = STATE_EMULATOR;
        lastButtonMs = millis();
      }
    }
    
    // 60FPS Draw Loop
    if (numRoms == 0) {
      const char* empty_msg[] = {"No ROMs found on SD card."};
      DisplayEmu::drawMenuFrame(empty_msg, 1, 0, false);
    } else {
      const char* titles[100];
      for (int i = 0; i < numRoms; i++) titles[i] = SDCard::getRomInfo(i)->filename;
      DisplayEmu::drawMenuFrame(titles, numRoms, selectedGameIndex, useColorEmulator);
    }
    
    // ~60 FPS delay (16ms)
    delay(16);

  } else if (currentState == STATE_EMULATOR) {
    unsigned long frameStart = micros();

    Buttons::update();
    bool select = Buttons::get(Buttons::SELECT).pressed;
    bool up     = Buttons::get(Buttons::UP).pressed;

    // Return to menu: SELECT + UP
    if (select && up) {
      if (selectedEmulatorIndex == 2) {
        NesEmu::destroy();
      } else if (selectedEmulatorIndex == 3) {
        DoomEmu::destroy();
      }
      
      if (currentRomBuffer) {
        SDCard::freeRom(currentRomBuffer);
        currentRomBuffer = nullptr;
      }

      currentState = STATE_MENU;
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
