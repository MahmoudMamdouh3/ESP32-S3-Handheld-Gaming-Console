#pragma once


// -----------------------------------------------------------------------
// config.h - Central pin/hardware configuration.
//
// This is the single source of truth for wiring. If you rewire anything,
// change it here - nothing else in the firmware should hardcode a pin
// number.
//
// Board: ESP32-S3-N16R8 (16MB flash, 8MB octal PSRAM)
// NOTE: GPIO 33-37 are reserved internally for octal PSRAM on this board
// and must never be used as regular GPIO. GPIO 0/3/45/46 are strapping
// pins (affect boot mode) and are avoided for buttons.
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------------
// HARDWARE FEATURE FLAGS (See .agents/rules/01_hardware.md)
// -----------------------------------------------------------------------------
#define FEATURE_SD_CARD 1
#define FEATURE_AUDIO 0
#define FEATURE_BATTERY_MONITOR 0

// ---------- Display: ST7789, 2.4", 240x320, SPI ----------
#define TFT_SCK    12
#define TFT_MOSI   11
#define TFT_RST    9
#define TFT_DC     8
#define TFT_CS     10
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ---------- SD Card Module (shares SPI bus with display) ----------
// SCK and MOSI are shared with the display (GPIO 12 and 11).
#define SD_MISO    15
#define SD_CS      13

// ---------- Buttons (Game Boy layout) ----------
// Each button: one leg -> GPIO, other leg -> GND.
// Internal pull-ups used (INPUT_PULLUP), so a press reads LOW.
#define BTN_UP     4
#define BTN_DOWN   5
#define BTN_LEFT   6
#define BTN_RIGHT  7
#define BTN_A      16
#define BTN_B      17
#define BTN_START  18
#define BTN_SELECT 21

// ---------- Audio: I2S DAC (e.g. MAX98357A) ----------
#define I2S_BCLK   38
#define I2S_LRC    39
#define I2S_DIN    40

// ---------- Battery Management ----------
#define BATTERY_ADC_PIN 1

// ---------- I2C / FRAM (Planned/Optional) ----------
#define I2C_SDA    43
#define I2C_SCL    44

// -----------------------------------------------------------------------------
// LOGGING & DIAGNOSTICS (See .agents/rules/16_logging_and_diagnostics.md)
// -----------------------------------------------------------------------------
#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

// ERROR
#if LOG_LEVEL >= LOG_LEVEL_ERROR
  #define LOG_ERROR(fmt, ...) Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
  #define LOG_ERROR_STR(str)  Serial.println("[ERROR] " str)
#else
  #define LOG_ERROR(fmt, ...) do {} while(0)
  #define LOG_ERROR_STR(str)  do {} while(0)
#endif

// WARN
#if LOG_LEVEL >= LOG_LEVEL_WARN
  #define LOG_WARN(fmt, ...) Serial.printf("[WARN] " fmt "\n", ##__VA_ARGS__)
  #define LOG_WARN_STR(str)  Serial.println("[WARN] " str)
#else
  #define LOG_WARN(fmt, ...) do {} while(0)
  #define LOG_WARN_STR(str)  do {} while(0)
#endif

// INFO
#if LOG_LEVEL >= LOG_LEVEL_INFO
  #define LOG_INFO(fmt, ...) Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__)
  #define LOG_INFO_STR(str)  Serial.println("[INFO] " str)
#else
  #define LOG_INFO(fmt, ...) do {} while(0)
  #define LOG_INFO_STR(str)  do {} while(0)
#endif

// DEBUG
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
  #define LOG_DEBUG(fmt, ...) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
  #define LOG_DEBUG_STR(str)  Serial.println("[DEBUG] " str)
#else
  #define LOG_DEBUG(fmt, ...) do {} while(0)
  #define LOG_DEBUG_STR(str)  do {} while(0)
#endif
