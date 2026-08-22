#pragma once

#include <Arduino.h>

namespace Emulator {
  // Initialize the emulator with ROM data.
  // Returns true on success, false on failure (e.g. invalid ROM).
  bool begin();

  // Update the emulator's joypad state based on physical buttons.
  void updateJoypad();

  // Run one frame of the emulator (approx 16.7ms of game time).
  void runFrame();
}
