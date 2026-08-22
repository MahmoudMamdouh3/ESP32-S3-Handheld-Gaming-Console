#pragma once

#include <Arduino.h>

namespace Emulator {
  // Call once in setup() with the selected ROM data and length.
  // Returns true on success, false on failure (e.g. invalid ROM header).
  bool begin(const uint8_t* rom_data, size_t rom_len);

  // Update the emulator's joypad state based on physical buttons.
  void updateJoypad();

  // Run one frame of the emulator (approx 16.7ms of game time).
  void runFrame();
}
