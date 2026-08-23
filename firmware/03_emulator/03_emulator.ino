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
const int NUM_GAMES = 2;

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
const char* gameTitles[NUM_GAMES];

void setup() {
  Serial.begin(115200);
  
  // Wait 3 seconds to let Serial connect. We removed while(!Serial) because it will hang 
  // forever if you are plugged into the UART port instead of the Native USB port!
  delay(3000);

  Serial.println("\n\n--- BOOTING ---");
  Serial.println("Milestone 4: Game Selection UI");

  // Critical: initialize shared SPI bus first
  // TODO(Milestone 2): Update to SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, TFT_CS) when SD card is added
  SPI.begin(TFT_SCK, -1, TFT_MOSI);

  //  Serial.println("System starting...");

  // --- UNIT TEST RUNNER ---
  // Uncomment this to run the rigorous test suite instead of the game!
  // #define ENABLE_UNIT_TESTS
  #ifdef ENABLE_UNIT_TESTS
    Serial.println("Booting into Test Mode...");
    bool all_passed = runAllTests();
    Serial.println(all_passed ? "TESTS SUCCESS" : "TESTS FAILED");
    while(1) delay(100); // Halt here forever
  #endif
  // -------------------------

  // Initialize hardware modules
  Buttons::begin();
  DisplayEmu::begin();
  
  for (int i = 0; i < NUM_GAMES; i++) {
    gameTitles[i] = games[i].title;
  }
}

unsigned long lastTime = 0;
int frames = 0;
int droppedFrames = 0;
int totalDroppedFramesThisSecond = 0;

void loop() {
  if (currentState == STATE_EMULATOR_SELECT) {
    Buttons::update();
    bool left = Buttons::get(Buttons::LEFT).pressed && Buttons::get(Buttons::LEFT).changed;
    bool right = Buttons::get(Buttons::RIGHT).pressed && Buttons::get(Buttons::RIGHT).changed;
    bool a = Buttons::get(Buttons::A).pressed && Buttons::get(Buttons::A).changed;

    if (redrawMenu) {
      DisplayEmu::drawEmulatorSelectMenu(selectedEmulatorIndex);
      redrawMenu = false;
    }

    if (left) {
      selectedEmulatorIndex = (selectedEmulatorIndex - 1 + 6) % 6;
      redrawMenu = true;
      delay(200); // Debounce
    }
    if (right) {
      selectedEmulatorIndex = (selectedEmulatorIndex + 1) % 6;
      redrawMenu = true;
      delay(200);
    }
    
    if (a) {
      if (selectedEmulatorIndex == 0 || selectedEmulatorIndex == 1) {
        useColorEmulator = (selectedEmulatorIndex == 0);
        currentState = STATE_MENU;
        redrawMenu = true;
      } else {
        DisplayEmu::showSDCardWarning();
        delay(2000); // Show warning for 2 seconds
        redrawMenu = true;
      }
      delay(200); // Debounce A press
    }
    
    delay(16);
  } else if (currentState == STATE_MENU) {
    Buttons::update();
    bool left = Buttons::get(Buttons::LEFT).pressed && Buttons::get(Buttons::LEFT).changed;
    bool right = Buttons::get(Buttons::RIGHT).pressed && Buttons::get(Buttons::RIGHT).changed;
    bool a = Buttons::get(Buttons::A).pressed && Buttons::get(Buttons::A).changed;
    bool select = Buttons::get(Buttons::SELECT).pressed && Buttons::get(Buttons::SELECT).changed;

    if (redrawMenu) {
      DisplayEmu::drawMenu(gameTitles, NUM_GAMES, selectedGameIndex, games[selectedGameIndex].cover, useColorEmulator);
      redrawMenu = false;
    }

    if (left) {
      selectedGameIndex = (selectedGameIndex - 1 + NUM_GAMES) % NUM_GAMES;
      redrawMenu = true;
      delay(200); // Debounce
    }
    if (right) {
      selectedGameIndex = (selectedGameIndex + 1) % NUM_GAMES;
      redrawMenu = true;
      delay(200);
    }
    
    if (select) {
      useColorEmulator = !useColorEmulator;
      redrawMenu = true;
      delay(200); // Debounce
    }

    if (a) {
      DisplayEmu::clearScreen(); // Wipe the BMO teal menu before drawing the game!
      
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
      currentState = STATE_EMULATOR;
    }

    delay(16); // ~60fps UI poll rate
    
  } else if (currentState == STATE_EMULATOR) {
    unsigned long frameStart = micros();

    Buttons::update();
    bool select = Buttons::get(Buttons::SELECT).pressed;
    bool up = Buttons::get(Buttons::UP).pressed;

    // Return to Menu Hotkey (SELECT + UP)
    if (select && up) {
      currentState = STATE_MENU;
      redrawMenu = true;
      delay(300); // Debounce
      return; // Skip the rest of the frame
    }

    if (useColorEmulator) {
      WalnutEmu::updateJoypad();
      WalnutEmu::runFrame();
    } else {
      PeanutEmu::updateJoypad();
      PeanutEmu::runFrame();
    }
    
    // Blast the 103KB framebuffer to the screen in one SPI transaction
    DisplayEmu::renderFrame();

    // 4. Throttling to ~59.73 Hz (16742 microseconds per frame)
    // then spin-wait for the final ~2ms to guarantee sub-millisecond precision
    // without FreeRTOS tick rounding bias.
    unsigned long elapsed = micros() - frameStart;
    if (elapsed < 16742) {
      unsigned long wait = 16742 - elapsed;
      if (wait > 2000) {
        delay((wait - 2000) / 1000); // Sleep and power-gate core
      }
      
      // Spin-wait the remainder
      unsigned long lastYield = micros();
      while (micros() - frameStart < 16742) {
        if (micros() - lastYield >= 1000) {
          yield(); // Watchdog safety
          lastYield = micros();
        }
      }
    } else {
      // Frame took longer than budget
      droppedFrames++;
      totalDroppedFramesThisSecond++;
    }

    // 5. Calculate FPS
    frames++;
    unsigned long now = millis();
    if (now - lastTime >= 1000) {
      if (totalDroppedFramesThisSecond > 0) {
        Serial.printf("FPS: %d (Dropped frames this sec: %d, Total dropped: %d)\n", frames, totalDroppedFramesThisSecond, droppedFrames);
      } else {
        Serial.printf("FPS: %d\n", frames);
      }
      frames = 0;
      totalDroppedFramesThisSecond = 0;
      lastTime = now;
    }
  }
}
