// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
//
// firmware/bringup/bringup.ino — Minimal First-Power-On Hardware Bring-Up Sketch
//
// Purpose:
//   Verify the soldered ESP32-S3-DevKitC-1-N16R8 perfboard step-by-step before
//   flashing the full 3.29 MB multi-console firmware:
//     1. Print reset reason, internal DRAM heap, and 8 MiB Octal PSRAM status.
//     2. Run a 1 MiB Octal PSRAM write/readback pattern check.
//     3. Initialize the 2.4" ST7789 (240x320) display at safe 40 MHz SPI and draw RGB color bars.
//     4. Probe MicroSD card mount on shared FSPI (SCK=12, MOSI=11, MISO=15, CS=13) at 25 MHz.
//     5. Poll and echo all 8 soldered buttons (UP=4, DOWN=5, LEFT=6, RIGHT=7, A=16, B=17, START=18, SELECT=21).
//
// Hard Stops Enforced:
//   - Never reads GPIO 1 (FEATURE_BATTERY_MONITOR=0) or drives GPIO 38/39/40 (FEATURE_AUDIO=0).
//   - Never touches strapping pins (0, 3, 45, 46) or Octal PSRAM pins (33-37).

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#define TFT_MOSI 11
#define TFT_SCK 12
#define TFT_CS 10
#define TFT_DC 8
#define TFT_RST 9
#define SD_MISO 15
#define SD_CS 13
#define BTN_UP 4
#define BTN_DOWN 5
#define BTN_LEFT 6
#define BTN_RIGHT 7
#define BTN_A 16
#define BTN_B 17
#define BTN_START 18
#define BTN_SELECT 21
#define BTN_X 14
#define BTN_Y 42
#define BTN_L 47
#define BTN_R 48
#define I2S_BCLK 38
#define I2S_LRC 39
#define I2S_DIN 40
#define BATTERY_ADC_PIN 1
#define I2C_SDA 43
#define I2C_SCL 44

#include "../BmoGameboy/src/hal/pins_check.h"

namespace {

constexpr int kTftSck = TFT_SCK;
constexpr int kTftMosi = TFT_MOSI;
constexpr int kSdMiso = SD_MISO;
constexpr int kTftCs = TFT_CS;
constexpr int kTftDc = TFT_DC;
constexpr int kTftRst = TFT_RST;
constexpr int kSdCs = SD_CS;

constexpr uint32_t kSafeTftSpiHz = 40000000UL;
constexpr uint32_t kSdSpiHz = 25000000UL;

struct ButtonDef {
  int pin;
  const char* name;
};

constexpr ButtonDef kWiredButtons[8] = {
    {BTN_UP, "UP"},     {BTN_DOWN, "DOWN"}, {BTN_LEFT, "LEFT"},   {BTN_RIGHT, "RIGHT"},
    {BTN_A, "A"},       {BTN_B, "B"},       {BTN_START, "START"}, {BTN_SELECT, "SELECT"},
};

Adafruit_ST7789 tft(&SPI, kTftCs, kTftDc, kTftRst);
uint8_t g_lastButtonMask = 0xFF;

bool runPsramPatternTest(size_t testBytes) {
  uint32_t* buf = static_cast<uint32_t*>(heap_caps_malloc(testBytes, MALLOC_CAP_SPIRAM));
  if (!buf) {
    return false;
  }
  const size_t words = testBytes / sizeof(uint32_t);
  for (size_t i = 0; i < words; ++i) {
    buf[i] = static_cast<uint32_t>(0xA5A50000UL ^ (i * 2654435761UL));
  }
  bool ok = true;
  for (size_t i = 0; i < words; ++i) {
    const uint32_t expected = static_cast<uint32_t>(0xA5A50000UL ^ (i * 2654435761UL));
    if (buf[i] != expected) {
      ok = false;
      break;
    }
  }
  heap_caps_free(buf);
  return ok;
}

void drawColorBarsAndHeader(bool psramOk, size_t psramFree, size_t dramFree, bool sdOk, uint64_t sdSizeMb) {
  tft.fillScreen(ST77XX_BLACK);

  // Top color bars (Red, Green, Blue, Yellow, Cyan, Magenta, White, Teal)
  const uint16_t bars[8] = {
      ST77XX_RED,     ST77XX_GREEN, ST77XX_BLUE,  ST77XX_YELLOW,
      ST77XX_CYAN,    ST77XX_MAGENTA, ST77XX_WHITE, 0x5D93,
  };
  for (int i = 0; i < 8; ++i) {
    tft.fillRect(i * 40, 0, 40, 36, bars[i]);
  }

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(8, 44);
  tft.println("BMO HANDHELD - HARDWARE BRING-UP v1.0");

  tft.setCursor(8, 60);
  tft.printf("DRAM Free : %u B\n", static_cast<unsigned>(dramFree));
  tft.setCursor(8, 72);
  tft.printf("PSRAM 8MB : %s (%u B free)\n", psramOk ? "PASS" : "FAIL", static_cast<unsigned>(psramFree));
  tft.setCursor(8, 84);
  if (sdOk) {
    tft.printf("MicroSD   : PASS (%llu MB)\n", static_cast<unsigned long long>(sdSizeMb));
  } else {
    tft.println("MicroSD   : NOT MOUNTED (insert FAT32 SD)");
  }
  tft.setCursor(8, 96);
  tft.println("TFT Clock : 40 MHz Safe Mode (RGB Bars)");
  tft.setCursor(8, 114);
  tft.println("Press each button (UP/DOWN/LEFT/RIGHT/A/B/ST/SEL):");
}

void renderButtonStates(uint8_t mask) {
  for (int i = 0; i < 8; ++i) {
    const bool pressed = (mask & (1U << i)) != 0;
    const int col = i % 4;
    const int row = i / 4;
    const int x = 8 + col * 76;
    const int y = 132 + row * 42;
    const uint16_t fill = pressed ? ST77XX_GREEN : 0x2124;
    const uint16_t text = pressed ? ST77XX_BLACK : ST77XX_WHITE;
    tft.fillRect(x, y, 70, 34, fill);
    tft.drawRect(x, y, 70, 34, ST77XX_WHITE);
    tft.setTextColor(text, fill);
    tft.setCursor(x + 8, y + 13);
    tft.print(kWiredButtons[i].name);
  }
}

} // namespace

void setup() {
  // Deselect both SPI chip-select lines immediately before starting SPI
  pinMode(kTftCs, OUTPUT);
  digitalWrite(kTftCs, HIGH);
  pinMode(kSdCs, OUTPUT);
  digitalWrite(kSdCs, HIGH);

  for (const auto& btn : kWiredButtons) {
    pinMode(btn.pin, INPUT_PULLUP);
  }

  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("==========================================");
  Serial.println(" BMO HANDHELD - HARDWARE BRING-UP SKETCH");
  Serial.println("==========================================");
  Serial.printf("[BOOT] esp_reset_reason = %d\n", static_cast<int>(esp_reset_reason()));

  const size_t dramFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t dramMin = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  const size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  const size_t psramLargest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  const bool psramOk = (psramTotal >= 4 * 1024 * 1024) && runPsramPatternTest(1024 * 1024);

  Serial.printf("[MEM]  Internal DRAM free=%u B (min=%u B)\n", static_cast<unsigned>(dramFree),
                static_cast<unsigned>(dramMin));
  Serial.printf("[PSRAM] total=%u B, free=%u B, largest_block=%u B, 1MiB_memtest=%s\n",
                static_cast<unsigned>(psramTotal), static_cast<unsigned>(psramFree),
                static_cast<unsigned>(psramLargest), psramOk ? "PASS" : "FAIL");

  SPI.begin(kTftSck, kSdMiso, kTftMosi, -1);

  // Initialize ST7789 240x320 in landscape (320x240) at safe 40 MHz SPI
  tft.init(240, 320);
  tft.setSPISpeed(kSafeTftSpiHz);
  tft.setRotation(1);
  Serial.println("[TFT]  ST7789 initialized at 40 MHz (rotation=1, 320x240)");

  // Probe MicroSD on shared SPI bus while TFT_CS is HIGH
  digitalWrite(kTftCs, HIGH);
  const bool sdOk = SD.begin(kSdCs, SPI, kSdSpiHz);
  uint64_t sdSizeMb = 0;
  if (sdOk) {
    sdSizeMb = SD.cardSize() / (1024ULL * 1024ULL);
    Serial.printf("[SD]   PASS: Mounted at 25 MHz, cardSize=%llu MB\n", static_cast<unsigned long long>(sdSizeMb));
  } else {
    Serial.println("[SD]   WARN: SD.begin(GPIO 13) failed or no FAT32 card inserted.");
  }

  drawColorBarsAndHeader(psramOk, psramFree, dramFree, sdOk, sdSizeMb);
}

void loop() {
  uint8_t mask = 0;
  for (int i = 0; i < 8; ++i) {
    if (digitalRead(kWiredButtons[i].pin) == LOW) {
      mask |= static_cast<uint8_t>(1U << i);
    }
  }
  if (mask != g_lastButtonMask) {
    g_lastButtonMask = mask;
    renderButtonStates(mask);
    Serial.printf("[BTN]  mask=0x%02X :", mask);
    for (int i = 0; i < 8; ++i) {
      if (mask & (1U << i)) {
        Serial.printf(" %s(GPIO%d)", kWiredButtons[i].name, kWiredButtons[i].pin);
      }
    }
    if (mask == 0) {
      Serial.print(" (all released)");
    }
    Serial.println();
  }
  delay(20);
}
