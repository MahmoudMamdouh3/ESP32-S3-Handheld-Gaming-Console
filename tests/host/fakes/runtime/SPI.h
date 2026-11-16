#pragma once
#include <stdint.h>
#include <stddef.h>

class SPIClass {
public:
  void writeBytes(const uint8_t*, size_t) {}
};

inline SPIClass SPI;
