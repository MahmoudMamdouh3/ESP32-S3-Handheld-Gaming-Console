#pragma once

#include <Arduino.h>

namespace PeanutEmu {
// Call once before running frames with the selected ROM data and length.
// Returns true on success, false on failure (e.g. invalid ROM header).
bool init(const uint8_t* rom_data, size_t rom_len);

// Poll joypad state and run one frame of the emulator (approx 16.7ms of game time).
void update();
void destroy();

// Non-Volatile Battery RAM & Save State APIs
bool saveBatteryRam(const char* romFilename);
bool loadBatteryRam(const char* romFilename);
bool saveState(const char* romFilename, int slot);
bool loadState(const char* romFilename, int slot);
} // namespace PeanutEmu
