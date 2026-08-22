#include "config.h"
#include "buttons.h"
#include "emulator.h"
#include "display_emu.h"
#include <SPI.h>

void setup() {
  Serial.begin(115200);
  
  // Wait for the USB CDC Serial Monitor to connect so we don't miss output.
  // Note: On ESP32-S3 Native USB, this pauses until the monitor is opened!
  while (!Serial) delay(10);
  delay(100);

  Serial.println("\n\n--- BOOTING ---");

  Serial.println("Milestone 3: Game Boy Emulator Core");

  // Critical: initialize shared SPI bus first
  // TODO(Milestone 2): Update to SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, TFT_CS) when SD card is added
  SPI.begin(TFT_SCK, -1, TFT_MOSI);

  // Initialize hardware modules
  Buttons::begin();
  DisplayEmu::begin();

  // Initialize the emulator
  if (!Emulator::begin()) {
    Serial.println("Failed to start emulator. Check errors.");
    while (1) delay(100);
  }
}

unsigned long lastTime = 0;
int frames = 0;
int droppedFrames = 0;
int totalDroppedFramesThisSecond = 0;

void loop() {
  unsigned long frameStart = micros();

  // 1. Read hardware buttons
  // Relies on the ~16.7ms frame cadence for de facto debounce — see buttons.h
  Buttons::update();

  // 2. Map buttons to emulator joypad
  Emulator::updateJoypad();

  // 3. Run exactly one frame
  Emulator::runFrame();

  // 4. Throttling to ~59.73 Hz (16742 microseconds per frame)
  // Hybrid approach for battery life: sleep for the bulk of the time,
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
