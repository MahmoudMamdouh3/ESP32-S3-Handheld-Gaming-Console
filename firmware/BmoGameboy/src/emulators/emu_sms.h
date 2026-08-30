#pragma once

#include <stdint.h>
#include <stddef.h>

class SmsEmu {
public:
  static bool begin(const uint8_t* romData, size_t romSize, bool isGameGear = false);
  static void updateJoypad();
  static void runFrame();
  static void destroy();
};
