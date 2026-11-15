// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include <stdint.h>

#include "../storage/atomic_fs.h"

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

struct BootBreadcrumb {
  uint32_t magic;
  uint8_t state;
  int8_t emulatorIndex;
  uint16_t flags;
  uint32_t crc32;
};

constexpr uint32_t kBreadcrumbMagic = 0x424D4F42U; // "BMOB"
constexpr uint16_t kBreadcrumbFormatVersion = 1U;
constexpr uint8_t kBreadcrumbMaxStateCount = 8U;

inline constexpr uint32_t computeBootBreadcrumbCrc(uint32_t magic, uint8_t state, int8_t emulatorIndex,
                                                   uint16_t flags) {
  const uint8_t raw[8] = {
      static_cast<uint8_t>(magic & 0xFFU),
      static_cast<uint8_t>((magic >> 8) & 0xFFU),
      static_cast<uint8_t>((magic >> 16) & 0xFFU),
      static_cast<uint8_t>((magic >> 24) & 0xFFU),
      state,
      static_cast<uint8_t>(emulatorIndex),
      static_cast<uint8_t>(flags & 0xFFU),
      static_cast<uint8_t>((flags >> 8) & 0xFFU),
  };
  return AtomicFs::crc32Update(raw, sizeof(raw), 0xFFFFFFFFU) ^ 0xFFFFFFFFU;
}

inline constexpr BootBreadcrumb makeBootBreadcrumb(uint8_t state, int8_t emulatorIndex,
                                                   uint16_t flags = kBreadcrumbFormatVersion) {
  return BootBreadcrumb{kBreadcrumbMagic, state, emulatorIndex, flags,
                        computeBootBreadcrumbCrc(kBreadcrumbMagic, state, emulatorIndex, flags)};
}

inline constexpr bool isValidBootBreadcrumb(const BootBreadcrumb& breadcrumb,
                                            uint8_t maxStateCount = kBreadcrumbMaxStateCount,
                                            int8_t maxEmulatorCount = 32) {
  return breadcrumb.magic == kBreadcrumbMagic && breadcrumb.flags == kBreadcrumbFormatVersion &&
         breadcrumb.state < maxStateCount && breadcrumb.emulatorIndex >= -1 &&
         breadcrumb.emulatorIndex < maxEmulatorCount &&
         breadcrumb.crc32 ==
             computeBootBreadcrumbCrc(breadcrumb.magic, breadcrumb.state, breadcrumb.emulatorIndex, breadcrumb.flags);
}

} // namespace BmoApp
