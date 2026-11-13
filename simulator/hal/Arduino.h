// Arduino.h - minimal Arduino-ESP32 compatible core for the BMO WebAssembly build.
// The firmware sources and the Adafruit GFX/ST7789 libraries are compiled
// unmodified against this header; hardware access is routed to the host
// simulator in sim_hw.cpp.
#ifndef Arduino_h
#define Arduino_h

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>

#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "soc/gpio_reg.h"

#define PI         3.1415926535897932384626433832795
#define HALF_PI    1.5707963267948966192313216916398
#define TWO_PI     6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

#define LSBFIRST 0
#define MSBFIRST 1

#define HIGH 0x1
#define LOW  0x0

#define INPUT        0x01
#define OUTPUT       0x03
#define PULLUP       0x04
#define INPUT_PULLUP 0x05
#define PULLDOWN     0x08
#define INPUT_PULLDOWN 0x09

#define _min(a, b)                ((a) < (b) ? (a) : (b))
#define _max(a, b)                ((a) > (b) ? (a) : (b))
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define radians(deg)              ((deg) * DEG_TO_RAD)
#define degrees(rad)              ((rad) * RAD_TO_DEG)
#define sq(x)                     ((x) * (x))
#define lowByte(w)  ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
#define bitRead(value, bit)  (((value) >> (bit)) & 0x01)
#define bitSet(value, bit)   ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bit(b) (1UL << (b))
#define _BV(b) (1UL << (b))

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PGM_P
#define PGM_P const char *
#endif
#define PSTR(s) (s)
#define pgm_read_byte(addr)  (*(const unsigned char *)(addr))
#define pgm_read_word(addr)  (*(const unsigned short *)(addr))
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#define pgm_read_pointer(addr) (*(void *const *)(addr))
#define memcpy_P memcpy
#define strlen_P strlen
#define strcpy_P strcpy
#define strncpy_P strncpy
#define strcmp_P strcmp
#define snprintf_P snprintf
#define vsnprintf_P vsnprintf

typedef bool boolean;
typedef uint8_t byte;
typedef unsigned int word;

#ifdef __cplusplus
extern "C" {
#endif

unsigned long millis(void);
unsigned long micros(void);
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void yield(void);
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);
void ets_delay_us(uint32_t us);

#ifdef __cplusplus
}

#include <algorithm>
#include <cmath>

#include "WString.h"
#include "Stream.h"
#include "Printable.h"
#include "Print.h"
#include "HardwareSerial.h"

using std::abs;
using std::max;
using std::min;
using std::round;

long map(long, long, long, long, long);
void setup(void);
void loop(void);
#endif

#endif
