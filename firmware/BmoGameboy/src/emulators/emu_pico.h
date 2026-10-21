#pragma once

#include <stdint.h>
#include <stddef.h>

class PicoEmu {
public:
  static bool begin(const uint8_t* romData, size_t romSize);
  static void updateJoypad();
  static void runFrame();
  static void destroy();

  static bool saveBatteryRam(const char* romFilename);
  static bool loadBatteryRam(const char* romFilename);
  static bool saveState(const char* romFilename, int slot);
  static bool loadState(const char* romFilename, int slot);
};
