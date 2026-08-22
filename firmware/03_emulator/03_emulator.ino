#include "config.h"
#include "buttons.h"
#include "emulator.h"
#include "display_emu.h"
#include <SPI.h>

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println("Milestone 3: Game Boy Emulator Core");

  // Critical: initialize shared SPI bus first
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

void loop() {
  unsigned long frameStart = micros();

  // 1. Read hardware buttons
  Buttons::update();

  // 2. Map buttons to emulator joypad
  Emulator::updateJoypad();

  // 3. Run exactly one frame
  Emulator::runFrame();

  // 4. Throttling to ~59.73 Hz (16742 microseconds per frame)
  // We use a pure micros() busy-wait to avoid FreeRTOS tick rounding bias from delay(),
  // but call yield() every 1ms to ensure watchdog safety.
  unsigned long lastYield = micros();
  while (micros() - frameStart < 16742) {
    if (micros() - lastYield >= 1000) {
      yield();
      lastYield = micros();
    }
  }

  // 5. Calculate FPS
  frames++;
  unsigned long now = millis();
  if (now - lastTime >= 1000) {
    Serial.printf("FPS: %d\n", frames);
    frames = 0;
    lastTime = now;
  }
}
