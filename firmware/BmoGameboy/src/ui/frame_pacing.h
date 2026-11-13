#pragma once

#include <stdint.h>

// Emulator index values are assigned in BmoGameboy.ino when a ROM launches.
// These are nominal host-loop budgets, not measured panel or core timings.
namespace FramePacing {
constexpr int WALNUT_GBC = 0;
constexpr int PEANUT_GB = 1;
constexpr int DOOM = 3;
constexpr int PICO8 = 8;

constexpr uint32_t targetFrameUs(int emulatorIndex) {
  switch (emulatorIndex) {
  // Peanut GB derives this cadence from 4,194,304 / 70,224 in its core.
  case WALNUT_GBC:
  case PEANUT_GB:
    return 16742U;
  // The vendored DOOM timer defines TICRATE as 35.
  case DOOM:
    return 28571U;
  // PICO-8 cartridges are presented at their nominal 30 Hz host cadence.
  case PICO8:
    return 33333U;
  // 60 Hz NTSC-like default for cores without region-specific timing metadata.
  default:
    return 16667U;
  }
}
} // namespace FramePacing
