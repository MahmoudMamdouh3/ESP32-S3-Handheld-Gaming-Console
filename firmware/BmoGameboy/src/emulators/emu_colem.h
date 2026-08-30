#pragma once
#include <stdint.h>
#include <stddef.h>

class ColemEmu {
public:
  static bool init(uint8_t* romData, size_t romSize);
  static void update();
  static void destroy();
  static bool isRunning();
};
