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

// ---------- Display: ST7789, 2.4", 240x320, SPI ----------
#define TFT_SCK    12
#define TFT_MOSI   11
#define TFT_RST    9
#define TFT_DC     8
#define TFT_CS     10
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

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
