// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#ifdef __cplusplus
#include <stddef.h>

namespace BmoPins {

inline constexpr int kActiveHardwarePins[] = {
    TFT_SCK,   TFT_MOSI, TFT_RST, TFT_DC,    TFT_CS,     SD_MISO, SD_CS, BTN_UP, BTN_DOWN, BTN_LEFT,
    BTN_RIGHT, BTN_A,    BTN_B,   BTN_START, BTN_SELECT, BTN_X,   BTN_Y, BTN_L,  BTN_R,
};

inline constexpr int kReservedEsp32S3Pins[] = {
    0,  // Strapping pin (Boot mode)
    3,  // Strapping pin (JTAG signal source)
    26, // SPICS1 (Flash/PSRAM)
    27, // SPIHD
    28, // SPIWP
    29, // SPICS0
    30, // SPICLK
    31, // SPIQ
    32, // SPID
    33, // SPIIO4 (Octal PSRAM)
    34, // SPIIO5 (Octal PSRAM)
    35, // SPIIO6 (Octal PSRAM)
    36, // SPIIO7 (Octal PSRAM)
    37, // SPIDQS (Octal PSRAM)
    45, // Strapping pin (VDD_SPI voltage)
    46, // Strapping pin (ROM message printing / boot mode)
};

constexpr bool hasDuplicatePins(const int* pins, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    for (size_t j = i + 1; j < count; ++j) {
      if (pins[i] == pins[j]) return true;
    }
  }
  return false;
}

constexpr bool usesReservedPin(const int* pins, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (pins[i] < 0 || pins[i] > 48) return true;
    for (size_t r = 0; r < sizeof(kReservedEsp32S3Pins) / sizeof(kReservedEsp32S3Pins[0]); ++r) {
      if (pins[i] == kReservedEsp32S3Pins[r]) return true;
    }
  }
  return false;
}

static_assert(!hasDuplicatePins(kActiveHardwarePins, sizeof(kActiveHardwarePins) / sizeof(kActiveHardwarePins[0])),
              "Duplicate GPIO assignment detected in config.h active pin table!");
static_assert(!usesReservedPin(kActiveHardwarePins, sizeof(kActiveHardwarePins) / sizeof(kActiveHardwarePins[0])),
              "Assigned GPIO in config.h collides with ESP32-S3 strapping (0,3,45,46) or Flash/Octal PSRAM (26-37)!");

// Exact soldered perfboard pin-value snapshot assertions (Hard Stop contract):
static_assert(TFT_SCK == 12 && TFT_MOSI == 11 && SD_MISO == 15 && TFT_CS == 10 && TFT_DC == 8 && TFT_RST == 9 &&
                  SD_CS == 13,
              "Soldered SPI pin assignment changed without owner approval!");
static_assert(BTN_UP == 4 && BTN_DOWN == 5 && BTN_LEFT == 6 && BTN_RIGHT == 7 && BTN_A == 16 && BTN_B == 17 &&
                  BTN_START == 18 && BTN_SELECT == 21,
              "Soldered 8-button GPIO mapping changed without owner approval!");
static_assert(BTN_X == 14 && BTN_Y == 42 && BTN_L == 47 && BTN_R == 48,
              "Configured extended button GPIO mapping changed without owner approval!");

} // namespace BmoPins
#endif
