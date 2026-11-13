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

// Optional compile-time features (disabled by default for clean public MIT builds)
#ifndef BMO_ENABLE_BAKED_ROMS
#define BMO_ENABLE_BAKED_ROMS 0
#endif

#ifndef BMO_ENABLE_DOOM
#define BMO_ENABLE_DOOM 0
#endif

// ---------- Display: ST7789, 2.4", 240x320, SPI ----------
#define TFT_SCK    12
#define TFT_MOSI   11
#define TFT_RST    9
#define TFT_DC     8
#define TFT_CS     10
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
#ifndef TFT_SPI_HZ
#define TFT_SPI_HZ 80000000UL
#endif
#define TFT_SPI_FALLBACK_HZ 40000000UL // Diagnostics comparison only; default remains 80 MHz.

#if TFT_SPI_HZ > 62500000UL
#pragma message("TFT_SPI_HZ exceeds the ST7789VW 62.5 MHz published serial-write limit; board timing is unmeasured")
#endif

// ---------- SD Card Module (shares SPI bus with display) ----------
// SCK and MOSI are shared with the display (GPIO 12 and 11).
#define SD_MISO    15
#define SD_CS      13
#define SD_SPI_HZ  25000000UL

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

// ---------- Extended Buttons (SNES / Genesis / DOOM layout) ----------
// X/Y face buttons and L/R shoulder triggers. Same wiring as the buttons
// above (GPIO -> switch -> GND, INPUT_PULLUP). Pins 42, 47 and 48 are above
// GPIO 31, so Buttons::update() reads them from GPIO_IN1_REG.
// None of these pins are strapping (0/3/45/46), PSRAM (33-37), or used by
// the display, SD card, I2S, or I2C assignments in this file.
#define BTN_X      14
#define BTN_Y      42
#define BTN_L      47
#define BTN_R      48

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
