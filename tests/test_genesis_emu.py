"""Behavioral tests for the Sega Genesis (Gwenesis) emulator core."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from host_test_support import build_and_run_firmware_cxx, build_and_run_wasi_c

REPO_ROOT = Path(__file__).resolve().parents[1]
THIRD_PARTY = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "third_party"

_GENESIS_DRIVER_C = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern int test_genesis_core(void);

void* heap_caps_malloc(size_t s, uint32_t c) { (void)c; return malloc(s ? s : 1U); }
void* heap_caps_calloc(size_t n, size_t s, uint32_t c) { (void)c; return calloc(n ? n : 1U, s ? s : 1U); }
void* heap_caps_realloc(void* p, size_t s, uint32_t c) { (void)c; return realloc(p, s ? s : 1U); }
void heap_caps_free(void* p) { free(p); }
unsigned long millis(void) { static unsigned long t = 0; t += 16UL; return t; }

int main(void) {
  return test_genesis_core();
}
"""

_GENESIS_IO_MUX_C = r"""
#include <stdio.h>
#include "io/gwenesis_io.h"
#include "savestate/gwenesis_savestate.h"

void gwenesis_io_get_buttons(void) {}
SaveState* saveGwenesisStateOpenForWrite(const char* module_name) { (void)module_name; return NULL; }
SaveState* saveGwenesisStateOpenForRead(const char* module_name) { (void)module_name; return NULL; }
void saveGwenesisStateSetBuffer(SaveState* state, const char* key, const void* buf, size_t len) {
  (void)state; (void)key; (void)buf; (void)len;
}
void saveGwenesisStateGetBuffer(SaveState* state, const char* key, void* buf, size_t len) {
  (void)state; (void)key; (void)buf; (void)len;
}

int main(void) {
  gwenesis_io_write_ctrl(0x09U, 0x40U);
  for (int btn = 0; btn < 8; ++btn) {
    gwenesis_io_pad_release_button(0, btn);
  }
  gwenesis_io_write_ctrl(0x03U, 0x40U);
  if ((gwenesis_io_read_ctrl(0x03U) & 0x7FU) != 0x7FU) {
    fprintf(stderr, "Expected idle TH=1 state 0x7F\n");
    return 1;
  }
  gwenesis_io_write_ctrl(0x03U, 0x00U);
  if ((gwenesis_io_read_ctrl(0x03U) & 0x7FU) != 0x33U) {
    fprintf(stderr, "Expected idle TH=0 state 0x33\n");
    return 2;
  }

  /* Press Start (bit 7) + C (bit 4) + Up (bit 0) -> TH=1 sees C+Up, TH=0 sees Start+Up. */
  gwenesis_io_pad_press_button(0, 7);
  gwenesis_io_pad_press_button(0, 4);
  gwenesis_io_pad_press_button(0, 0);

  gwenesis_io_write_ctrl(0x03U, 0x40U);
  const unsigned int hi = gwenesis_io_read_ctrl(0x03U);
  if ((hi & 0x10U) != 0U || (hi & 0x01U) != 0U || (hi & 0x20U) == 0U) {
    fprintf(stderr, "TH=1 mux mismatch: 0x%02X\n", hi);
    return 3;
  }

  gwenesis_io_write_ctrl(0x03U, 0x00U);
  const unsigned int lo = gwenesis_io_read_ctrl(0x03U);
  if ((lo & 0x20U) != 0U || (lo & 0x10U) == 0U || (lo & 0x01U) != 0U) {
    fprintf(stderr, "TH=0 mux mismatch: 0x%02X\n", lo);
    return 4;
  }
  return 0;
}
"""

_GENESIS_CXX_BEHAVIOR = r"""
#include <stdint.h>
#include <string.h>
#include <vector>

#include "cores/core_registry.h"
#include "storage/battery_save.h"
#include "storage/rom_file_filter.h"
#include "test_check.h"
#include "ui/console_catalog.h"

int main() {
  static_assert(CoreRegistry::detail::kCores[9].emulatorIndex == 9, "Genesis core index must be 9");
  static_assert(CoreRegistry::detail::kCores[9].romType == ROM_GENESIS, "Core 9 must be ROM_GENESIS");
  static_assert(CoreRegistry::detail::kCores[9].hasActiveBatteryRam != nullptr,
                "Genesis supports active battery check");

  const ConsoleCatalog::ConsoleSpec* spec = ConsoleCatalog::findByType(ROM_GENESIS);
  BMO_CHECK(spec != nullptr);
  BMO_CHECK(spec->viewX == 0 && spec->viewY == 8 && spec->viewW == 320 && spec->viewH == 224);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("/roms/md/Sonic2.md") == ROM_GENESIS);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("streets.GEN") == ROM_GENESIS);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("gunstar.smd") == ROM_GENESIS);

  std::vector<uint8_t> rom(0x200U, 0U);
  BMO_CHECK(!BatterySave::genesisHeaderHasBattery(rom.data(), rom.size()));

  rom[0x1B0] = 'R';
  rom[0x1B1] = 'A';
  rom[0x1B4] = 0x00U; rom[0x1B5] = 0x20U; rom[0x1B6] = 0x00U; rom[0x1B7] = 0x00U;
  rom[0x1B8] = 0x00U; rom[0x1B9] = 0x20U; rom[0x1BA] = 0x3FU; rom[0x1BB] = 0xFFU;
  BMO_CHECK(BatterySave::genesisHeaderHasBattery(rom.data(), rom.size()));

  // Inverted range or >16 KB span must return false.
  rom[0x1B8] = 0x00U; rom[0x1B9] = 0x1FU;
  BMO_CHECK(!BatterySave::genesisHeaderHasBattery(rom.data(), rom.size()));
  rom[0x1B8] = 0x00U; rom[0x1B9] = 0x30U;
  BMO_CHECK(!BatterySave::genesisHeaderHasBattery(rom.data(), rom.size()));
  BMO_CHECK(!BatterySave::genesisHeaderHasBattery(rom.data(), 0x1B0U));

  const uint8_t segaHdr[4] = {'S', 'E', 'G', 'A'};
  const uint8_t badHdr[4] = {'T', 'E', 'X', 'T'};
  BMO_CHECK(RomFileFilter::isPlausibleFile("sonic.md", 0x200U, nullptr, 0U, segaHdr, 4U));
  BMO_CHECK(!RomFileFilter::isPlausibleFile("readme.md", 0x200U, nullptr, 0U, badHdr, 4U));
  BMO_CHECK(!RomFileFilter::isPlausibleFile("tiny.md", 32U, nullptr, 0U, segaHdr, 4U));
  return 0;
}
"""


class TestGenesisEmulatorCore(unittest.TestCase):
    """Compiled WASI + C++17 behavioral checks for Gwenesis and Genesis firmware metadata."""

    def test_genesis_core_boot_vdp_and_th_mux_wasi(self) -> None:
        gwen_dir = THIRD_PARTY / "gwenesis"
        c_sources = [
            gwen_dir / "bus" / "gwenesis_bus.c",
            gwen_dir / "cpus" / "M68K" / "m68kcpu.c",
            gwen_dir / "cpus" / "Z80" / "Z80.c",
            gwen_dir / "io" / "gwenesis_io.c",
            gwen_dir / "savestate" / "gwenesis_savestate.c",
            gwen_dir / "sound" / "gwenesis_sn76489.c",
            gwen_dir / "sound" / "ym2612.c",
            gwen_dir / "sound" / "z80inst.c",
            gwen_dir / "vdp" / "gwenesis_vdp_gfx.c",
            gwen_dir / "vdp" / "gwenesis_vdp_mem.c",
            REPO_ROOT / "tests" / "host" / "bridge_genesis.c",
        ]
        with tempfile.TemporaryDirectory() as tmp:
            driver = Path(tmp) / "genesis_driver.c"
            driver.write_text(_GENESIS_DRIVER_C, encoding="utf-8")
            res = build_and_run_wasi_c(
                self,
                [driver, *c_sources],
                include_dirs=(gwen_dir, REPO_ROOT / "firmware" / "BmoGameboy" / "src"),
                forced_includes=("stdint.h",),
            )
            self.assertIn("[HOST_CORE] GENESIS_68K STATUS=PASS", res.stdout)

            io_mux_driver = Path(tmp) / "genesis_io_mux.c"
            io_mux_driver.write_text(_GENESIS_IO_MUX_C, encoding="utf-8")
            build_and_run_wasi_c(
                self,
                [io_mux_driver, gwen_dir / "io" / "gwenesis_io.c"],
                include_dirs=(gwen_dir,),
                forced_includes=("stddef.h", "stdint.h"),
            )

    def test_genesis_battery_header_catalog_and_filter_cxx(self) -> None:
        build_and_run_firmware_cxx(self, _GENESIS_CXX_BEHAVIOR)


if __name__ == "__main__":
    unittest.main()
