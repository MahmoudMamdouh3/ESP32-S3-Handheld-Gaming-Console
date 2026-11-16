#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef INPUT
#define INPUT 0x01
#endif
#ifndef OUTPUT
#define OUTPUT 0x03
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x05
#endif
#ifndef HIGH
#define HIGH 0x1
#endif
#ifndef LOW
#define LOW 0x0
#endif

class HostSerialFake {
public:
  void begin(unsigned long) {}
  void print(const char*) {}
  void println(const char*) {}
  void printf(const char*, ...) {}
};

inline HostSerialFake Serial;

inline unsigned long millis(void) {
  return static_cast<unsigned long>(HostRuntimeFake::g_micros / 1000ULL);
}
inline unsigned long micros(void) {
  return static_cast<unsigned long>(HostRuntimeFake::g_micros);
}
inline void delay(uint32_t ms) {
  HostRuntimeFake::g_micros += static_cast<uint64_t>(ms) * 1000ULL;
}
inline void delayMicroseconds(uint32_t us) {
  HostRuntimeFake::g_micros += us;
}
inline void ets_delay_us(uint32_t us) {
  HostRuntimeFake::g_micros += us;
}
inline void pinMode(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) {
  return HIGH;
}
