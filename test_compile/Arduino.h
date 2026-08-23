#pragma once
#include <stdint.h>
#include <stddef.h>
#define PROGMEM
class Serial_ { public: void println(const char*); void printf(const char*, ...); void flush(); }; extern Serial_ Serial; void delay(int); unsigned long micros(); void esp_restart();
