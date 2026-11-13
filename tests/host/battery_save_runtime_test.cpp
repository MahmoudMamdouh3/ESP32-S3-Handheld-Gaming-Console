#include "battery_save.h"
#include "test_check.h"

#include <stdint.h>
#include <string.h>

namespace {

void writeU16LE(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFFU);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFFU);
}

void writeU32BE(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>((v >> 24) & 0xFFU);
  p[1] = static_cast<uint8_t>((v >> 16) & 0xFFU);
  p[2] = static_cast<uint8_t>((v >> 8) & 0xFFU);
  p[3] = static_cast<uint8_t>(v & 0xFFU);
}

} // namespace

int main() {
  // SNES cartridge type classification (low nibble {2, 5, 6, 9, A})
  BMO_CHECK(!BatterySave::snesCartridgeHasBattery(0x00U));
  BMO_CHECK(!BatterySave::snesCartridgeHasBattery(0x01U));
  BMO_CHECK(BatterySave::snesCartridgeHasBattery(0x02U));
  BMO_CHECK(!BatterySave::snesCartridgeHasBattery(0x03U));
  BMO_CHECK(BatterySave::snesCartridgeHasBattery(0x05U));
  BMO_CHECK(BatterySave::snesCartridgeHasBattery(0x06U));
  BMO_CHECK(BatterySave::snesCartridgeHasBattery(0x09U));
  BMO_CHECK(BatterySave::snesCartridgeHasBattery(0x0AU));
  BMO_CHECK(BatterySave::snesCartridgeHasBattery(0x15U));
  BMO_CHECK(!BatterySave::snesCartridgeHasBattery(0x13U));

  // SNES LoROM header scoring & battery byte decoding
  static uint8_t snesRom[0x8000];
  memset(snesRom, 0, sizeof(snesRom));
  const size_t loromBase = 0x7FC0U;
  memcpy(&snesRom[loromBase], "TEST SNES ROM        ", 21);
  snesRom[loromBase + 21] = 0x20U; // LoROM
  snesRom[loromBase + 22] = 0x02U; // ROM + RAM + Battery
  snesRom[loromBase + 23] = 0x05U; // 32 KiB ROM
  snesRom[loromBase + 24] = 0x03U; // 8 KiB SRAM (1024 << 3)
  snesRom[loromBase + 25] = 0x01U; // NTSC
  writeU16LE(&snesRom[loromBase + 28], 0x55AAU);
  writeU16LE(&snesRom[loromBase + 30], 0xAA55U); // complement ^ checksum == 0xFFFF
  BMO_CHECK(BatterySave::snesBatteryBytes(snesRom, sizeof(snesRom)) == 8192U);

  // Corrupted complement+checksum lowers score below 0 -> rejected
  writeU16LE(&snesRom[loromBase + 28], 0x0000U);
  writeU16LE(&snesRom[loromBase + 30], 0x0000U);
  snesRom[loromBase + 21] = 0x21U; // mismatched mode at LoROM offset
  BMO_CHECK(BatterySave::snesBatteryBytes(snesRom, sizeof(snesRom)) == 0U);

  // Restore valid checksum/mode, flip cartType to non-battery -> rejected even if sramSizeByte > 0
  snesRom[loromBase + 21] = 0x20U;
  snesRom[loromBase + 22] = 0x01U; // ROM + RAM (no battery)
  writeU16LE(&snesRom[loromBase + 28], 0x55AAU);
  writeU16LE(&snesRom[loromBase + 30], 0xAA55U);
  BMO_CHECK(BatterySave::snesBatteryBytes(snesRom, sizeof(snesRom)) == 0U);

  // Genesis 'RA' SRAM header parsing
  uint8_t genRom[0x200];
  memset(genRom, 0, sizeof(genRom));
  BMO_CHECK(!BatterySave::genesisHeaderHasBattery(genRom, 0x100U));
  BMO_CHECK(!BatterySave::genesisHeaderHasBattery(genRom, sizeof(genRom)));
  genRom[0x1B0] = 'R';
  genRom[0x1B1] = 'A';
  writeU32BE(&genRom[0x1B4], 0x00200001U);
  writeU32BE(&genRom[0x1B8], 0x00203FFFU); // span = 0x3FFE <= 0x4000
  BMO_CHECK(BatterySave::genesisHeaderHasBattery(genRom, sizeof(genRom)));
  writeU32BE(&genRom[0x1B8], 0x00210000U); // span > 0x4000
  BMO_CHECK(!BatterySave::genesisHeaderHasBattery(genRom, sizeof(genRom)));
  writeU32BE(&genRom[0x1B8], 0x00200000U); // end < start
  BMO_CHECK(!BatterySave::genesisHeaderHasBattery(genRom, sizeof(genRom)));

  // WonderSwan footer SRAM/EEPROM header parsing (romSize - 5)
  uint8_t wsRom[64];
  memset(wsRom, 0, sizeof(wsRom));
  BMO_CHECK(!BatterySave::wswanHeaderHasBattery(wsRom, 10U));
  BMO_CHECK(!BatterySave::wswanHeaderHasBattery(wsRom, sizeof(wsRom)));
  const uint8_t wsValid[] = {0x01U, 0x02U, 0x03U, 0x04U, 0x10U, 0x20U, 0x50U};
  for (uint8_t code : wsValid) {
    wsRom[sizeof(wsRom) - 5U] = code;
    BMO_CHECK(BatterySave::wswanHeaderHasBattery(wsRom, sizeof(wsRom)));
  }
  wsRom[sizeof(wsRom) - 5U] = 0x05U;
  BMO_CHECK(!BatterySave::wswanHeaderHasBattery(wsRom, sizeof(wsRom)));

  // PC Engine BRAM 'HUBM' header check
  const uint8_t validBram[8] = {'H', 'U', 'B', 'M', 0, 0, 0, 0};
  const uint8_t emptyBram[8] = {0};
  BMO_CHECK(BatterySave::pceBramHasHeader(validBram, sizeof(validBram)));
  BMO_CHECK(!BatterySave::pceBramHasHeader(emptyBram, sizeof(emptyBram)));
  BMO_CHECK(!BatterySave::pceBramHasHeader(validBram, 3U));

  return 0;
}
