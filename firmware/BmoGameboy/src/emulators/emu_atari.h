#pragma once

#include <stdint.h>
#include <stddef.h>

class AtariEmu {
public:
  static bool begin(const uint8_t* romData, size_t romSize);
  static void updateJoypad();
  static void runFrame();
  static void destroy();
};
