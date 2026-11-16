"""Behavioral tests for the Bandai WonderSwan / WSC (Oswan) emulator core."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from host_test_support import build_and_run_firmware_cxx, build_and_run_wasi_c

REPO_ROOT = Path(__file__).resolve().parents[1]
THIRD_PARTY = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "third_party"

_WSWAN_DRIVER_C = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WS.h"
#include "WSFileio.h"
#include "WSHard.h"
#include "WSInput.h"

extern int test_wswan_core(void);

int main(void) {
  if (test_wswan_core() != 0) {
    return 1;
  }

  /* Verify real Oswan SRAM bank allocation (ramType=0x01) + 0xB5 keypad mux. */
  const size_t rom_size = 0x20000U;
  uint8_t* rom = (uint8_t*)calloc(1U, rom_size);
  if (!rom) {
    return 2;
  }
  rom[0x1FFF0] = 0xEBU; /* infinite loop at FFFF:0000 */
  rom[0x1FFF1] = 0xFEU;
  rom[0x1FFF6] = 0x01U; /* developer ID */
  rom[0x1FFF7] = 0x01U; /* WSC capable */
  rom[0x1FFFA] = 0x02U; /* 1 Mbit ROM */
  rom[0x1FFFB] = 0x01U; /* 64 Kbit SRAM */

  WsInit();
  if (WsCreateFromMemory(rom, rom_size) != 0) {
    WsDeInit();
    free(rom);
    return 3;
  }
  WsReset();

  if (RAMBanks <= 0 || RAMSize <= 0 || !RAMMap || !RAMMap[0]) {
    WsDeInit();
    free(rom);
    fprintf(stderr, "Expected Oswan cartridge SRAM allocation\n");
    return 4;
  }

  /* Verify 0xB5 keypad matrix selection reflects written control bits 0x10 / 0x20 / 0x40. */
  WriteIO(0xB5U, 0x10U);
  const BYTE k10 = ReadIO(0xB5U);
  WriteIO(0xB5U, 0x20U);
  const BYTE k20 = ReadIO(0xB5U);
  WriteIO(0xB5U, 0x40U);
  const BYTE k40 = ReadIO(0xB5U);
  WsDeInit();
  free(rom);

  if ((k10 & 0xF0U) != 0x10U || (k20 & 0xF0U) != 0x20U || (k40 & 0xF0U) != 0x40U) {
    fprintf(stderr, "Keypad mux readback mismatch: 0x%02X 0x%02X 0x%02X\n", k10, k20, k40);
    return 5;
  }
  return 0;
}
"""

_WSWAN_CXX_BEHAVIOR = r"""
#include <stdint.h>
#include <vector>

#include "cores/core_registry.h"
#include "storage/battery_save.h"
#include "test_check.h"
#include "ui/console_catalog.h"

int main() {
  static_assert(CoreRegistry::detail::kCores[11].emulatorIndex == 11, "WSwan core index must be 11");
  static_assert(CoreRegistry::detail::kCores[11].romType == ROM_WSWAN, "Core 11 must be ROM_WSWAN");
  static_assert(CoreRegistry::detail::kCores[11].hasActiveBatteryRam != nullptr,
                "WSwan supports active battery check");

  const ConsoleCatalog::ConsoleSpec* spec = ConsoleCatalog::findByType(ROM_WSWAN);
  BMO_CHECK(spec != nullptr);
  BMO_CHECK(spec->viewX == 48 && spec->viewY == 48 && spec->viewW == 224 && spec->viewH == 144);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("klonoa.ws") == ROM_WSWAN);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("ff4.WSC") == ROM_WSWAN);

  BMO_CHECK(CoreRegistry::detail::hasExtension("klonoa.ws", ".ws"));
  BMO_CHECK(!CoreRegistry::detail::hasExtension("klonoa.wsc", ".ws"));
  BMO_CHECK(CoreRegistry::detail::hasExtension("klonoa.WSC", ".wsc"));

  std::vector<uint8_t> wsRom(0x100U, 0U);
  const uint8_t noBat[] = {0x00U, 0xFFU};
  for (uint8_t code : noBat) {
    wsRom[wsRom.size() - 5U] = code;
    BMO_CHECK(!BatterySave::wswanHeaderHasBattery(wsRom.data(), wsRom.size()));
  }
  const uint8_t yesBat[] = {0x01U, 0x02U, 0x03U, 0x04U, 0x10U, 0x20U, 0x50U};
  for (uint8_t code : yesBat) {
    wsRom[wsRom.size() - 5U] = code;
    BMO_CHECK(BatterySave::wswanHeaderHasBattery(wsRom.data(), wsRom.size()));
  }
  BMO_CHECK(!BatterySave::wswanHeaderHasBattery(wsRom.data(), 12U));
  return 0;
}
"""


class TestWSwanEmulatorCore(unittest.TestCase):
    """Compiled WASI + C++17 behavioral checks for Oswan and WonderSwan firmware metadata."""

    def test_wswan_oswan_v30mz_keypad_mux_and_sram_wasi(self) -> None:
        osw_dir = THIRD_PARTY / "oswan"
        c_sources = [
            osw_dir / "WS.c",
            osw_dir / "WSApu.c",
            osw_dir / "WSFileio.c",
            osw_dir / "WSInput.c",
            osw_dir / "WSRender.c",
            osw_dir / "cpu" / "nec.c",
            REPO_ROOT / "tests" / "host" / "bridge_wswan.c",
        ]
        with tempfile.TemporaryDirectory() as tmp:
            driver = Path(tmp) / "wswan_driver.c"
            driver.write_text(_WSWAN_DRIVER_C, encoding="utf-8")
            res = build_and_run_wasi_c(
                self,
                [driver, *c_sources],
                include_dirs=(osw_dir, osw_dir / "cpu", REPO_ROOT / "firmware" / "BmoGameboy" / "src"),
            )
            self.assertIn("[HOST_CORE] WONDERSWAN STATUS=PASS", res.stdout)

    def test_wswan_battery_header_and_color_routing_cxx(self) -> None:
        build_and_run_firmware_cxx(self, _WSWAN_CXX_BEHAVIOR)


if __name__ == "__main__":
    unittest.main()
