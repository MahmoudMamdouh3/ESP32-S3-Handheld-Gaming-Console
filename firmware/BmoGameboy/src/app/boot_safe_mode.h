// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include <stdint.h>

namespace BmoApp {

enum SystemState : uint8_t {
  STATE_BOOT_SPLASH = 0,
  STATE_CONSOLE_MENU,
  STATE_CONSOLE_MUSEUM,
  STATE_GAME_MENU,
  STATE_EMULATOR,
  STATE_PAUSE_MENU,
  STATE_DIAGNOSTICS,
  STATE_IDLE_MASCOT
};

struct BootSpiDecision {
  uint32_t spiHz;
  bool safeModeForced;
};

/**
 * Resolve boot display SPI clock from button state and persisted NVS clock.
 * Holding SELECT or B at boot forces the conservative fallback clock (40 MHz)
 * so unstable breadboard wiring can always recover cleanly.
 */
constexpr BootSpiDecision resolveBootDisplaySpiClock(bool selectPressed, bool bPressed, uint32_t savedClockHz,
                                                     uint32_t defaultClockHz, uint32_t fallbackClockHz) {
  if (selectPressed || bPressed) {
    return BootSpiDecision{fallbackClockHz, true};
  }
  const uint32_t resolved = (savedClockHz == fallbackClockHz) ? fallbackClockHz : defaultClockHz;
  return BootSpiDecision{resolved, false};
}

/**
 * Translate ESP-IDF esp_reset_reason_t numeric value into a human-readable label.
 */
constexpr const char* resetReasonName(int reasonCode) {
  switch (reasonCode) {
  case 1:
    return "ESP_RST_POWERON";
  case 2:
    return "ESP_RST_EXT";
  case 3:
    return "ESP_RST_SW";
  case 4:
    return "ESP_RST_PANIC";
  case 5:
    return "ESP_RST_INT_WDT";
  case 6:
    return "ESP_RST_TASK_WDT";
  case 7:
    return "ESP_RST_WDT";
  case 8:
    return "ESP_RST_DEEPSLEEP";
  case 9:
    return "ESP_RST_BROWNOUT";
  case 10:
    return "ESP_RST_SDIO";
  case 11:
    return "ESP_RST_USB";
  case 12:
    return "ESP_RST_JTAG";
  case 13:
    return "ESP_RST_EFUSE";
  case 14:
    return "ESP_RST_PWR_GLITCH";
  case 15:
    return "ESP_RST_CPU_LOCKUP";
  default:
    return "ESP_RST_UNKNOWN";
  }
}

/**
 * Return true when the reset reason indicates a crash, watchdog timeout, or brownout.
 */
constexpr bool isAbnormalResetReason(int reasonCode) {
  return reasonCode == 4 || reasonCode == 5 || reasonCode == 6 || reasonCode == 7 || reasonCode == 9 ||
         reasonCode == 14 || reasonCode == 15;
}

} // namespace BmoApp
