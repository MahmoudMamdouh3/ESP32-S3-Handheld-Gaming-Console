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

inline size_t snesBatteryBytes(const uint8_t* romData, size_t romSize) {
  if (!romData || romSize < 0x8000U) return 0;
  const size_t base = ((romSize & 0x3FFU) == 512U) ? 512U : 0U;
  auto scoreHeader = [&](size_t offset, bool isHi, bool isExHi) -> int {
    if (offset + 0x40U > romSize) return -1;
    const uint16_t comp = static_cast<uint16_t>(romData[offset + 0x2C] | (romData[offset + 0x2D] << 8));
    const uint16_t csum = static_cast<uint16_t>(romData[offset + 0x2E] | (romData[offset + 0x2F] << 8));
    const uint8_t mapMode = romData[offset + 0x25] & 0x0FU;
    int score = 0;
    if ((comp ^ csum) == 0xFFFFU && csum != 0) score += 4;
    if (!isHi && !isExHi && (mapMode == 0 || mapMode == 2 || mapMode == 3)) score += 2;
    if (isHi && !isExHi && (mapMode == 1 || mapMode == 5)) score += 2;
    if (isExHi && mapMode == 5) score += 3;
    return score;
  };
  const size_t loOff = base + 0x7FB0U;
  const size_t hiOff = base + 0xFFB0U;
  const size_t exHiOff = base + 0x40FFB0U;
  int bestScore = scoreHeader(loOff, false, false);
  size_t bestOffset = loOff;
  const int hiScore = scoreHeader(hiOff, true, false);
  if (hiScore > bestScore) { bestScore = hiScore; bestOffset = hiOff; }
  const int exScore = scoreHeader(exHiOff, true, true);
  if (exScore > bestScore) { bestScore = exScore; bestOffset = exHiOff; }
  if (bestOffset + 0x28U >= romSize) return 0;
  const uint8_t sramSizeCode = romData[bestOffset + 0x28U];
  return snesLoRomBatteryBytes(sramSizeCode);
}

inline bool pceBramHasHeader(const uint8_t* bram, size_t len) {
  return bram && len >= 4 &&
         bram[0] == 'H' && bram[1] == 'U' && bram[2] == 'B' && bram[3] == 'M';
}
}
