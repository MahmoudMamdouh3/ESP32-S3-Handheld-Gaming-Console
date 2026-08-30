#pragma once
#include <stdint.h>
#include <stddef.h>

class NGPEmu {
public:
  static bool init(uint8_t* romData, size_t romSize, bool isColor);
  static void update();
  static void destroy();
  static bool isRunning();
};
