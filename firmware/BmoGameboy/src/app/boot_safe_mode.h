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

} // namespace BmoApp
