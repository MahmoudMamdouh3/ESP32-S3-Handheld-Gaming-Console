#pragma once

#include <stddef.h>
#include <stdint.h>

namespace BatterySave {
constexpr bool gameBoyCartridgeHasBattery(uint8_t type) {
  switch (type) {
    case 0x03: // MBC1+RAM+BATTERY
    case 0x06: // MBC2+BATTERY
    case 0x09: // ROM+RAM+BATTERY
    case 0x0D: // MMM01+RAM+BATTERY
    case 0x0F: // MBC3+TIMER+BATTERY
    case 0x10: // MBC3+TIMER+RAM+BATTERY
    case 0x13: // MBC3+RAM+BATTERY
    case 0x1B: // MBC5+RAM+BATTERY
    case 0x1E: // MBC5+RUMBLE+RAM+BATTERY
    case 0x20: // MBC6+RAM+BATTERY
    case 0x22: // MBC7+SENSOR+RUMBLE+RAM+BATTERY
      return true;
    default:
      return false;
  }
}

constexpr size_t gameBoyBatteryBytes(uint8_t type, uint8_t ramSizeCode) {
  if (!gameBoyCartridgeHasBattery(type)) return 0;
  if (type == 0x06) return 512;
  if (type == 0x22) return 256;
  switch (ramSizeCode) {
    case 1: return 0x800;
    case 2: return 0x2000;
    case 3: return 0x8000;
    case 4: return 0x20000;
    case 5: return 0x10000;
    default: return 0;
  }
}

constexpr bool nesHeaderHasBattery(uint8_t flags6) {
  return (flags6 & 0x02U) != 0;
}

constexpr size_t snesLoRomBatteryBytes(uint8_t sramSizeCode) {
  if (sramSizeCode == 0 || sramSizeCode > 5) return 0;
  return static_cast<size_t>(1U) << (sramSizeCode + 10U);
}
}
