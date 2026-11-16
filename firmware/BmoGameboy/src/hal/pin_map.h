// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

// -----------------------------------------------------------------------------
// pin_map.h - Fixed ESP32-S3-N16R8 GPIO wiring map.
//
// NOTE: GPIO 33-37 are reserved internally for Octal PSRAM, 26-32 for QIO SPI
// Flash, and GPIO 0/3/45/46 are strapping pins.
// -----------------------------------------------------------------------------

// ---------- Display: ST7789, 2.4", 240x320, SPI ----------
#define TFT_SCK 12
#define TFT_MOSI 11
#define TFT_RST 9
#define TFT_DC 8
#define TFT_CS 10

// ---------- SD Card Module (shares SPI bus with display) ----------
#define SD_MISO 15
#define SD_CS 13

// ---------- Buttons (Game Boy layout, INPUT_PULLUP active-LOW) ----------
#define BTN_UP 4
#define BTN_DOWN 5
#define BTN_LEFT 6
#define BTN_RIGHT 7
#define BTN_A 16
#define BTN_B 17
#define BTN_START 18
#define BTN_SELECT 21

// ---------- Extended Buttons (configured, unsoldered on v1 board) ----------
#define BTN_X 14
#define BTN_Y 42
#define BTN_L 47
#define BTN_R 48

// ---------- Audio: I2S DAC (FEATURE_AUDIO=0 hard stop) ----------
#define I2S_BCLK 38
#define I2S_LRC 39
#define I2S_DIN 40

// ---------- Battery Management (FEATURE_BATTERY_MONITOR=0 hard stop) ----------
#define BATTERY_ADC_PIN 1

// ---------- I2C / FRAM (Planned/Optional) ----------
#define I2C_SDA 43
#define I2C_SCL 44
