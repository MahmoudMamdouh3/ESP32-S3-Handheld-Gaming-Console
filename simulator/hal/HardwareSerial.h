#pragma once
// USB-CDC serial port. Output goes to the simulator's serial log panel.
#include <stdint.h>
#include <stddef.h>
#include "Stream.h"

class HardwareSerial : public Stream {
public:
  void begin(unsigned long baud = 115200) { (void)baud; }
  void end() {}
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buf, size_t size) override;
  using Print::write;
  operator bool() const { return true; }
};

extern HardwareSerial Serial;
