"""Behavioral firmware and host-core validation tests for all Tier-2 consoles."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from host_test_support import build_and_run_firmware_cxx, build_and_run_wasi_c

REPO_ROOT = Path(__file__).resolve().parents[1]
FW_SRC = REPO_ROOT / "firmware" / "BmoGameboy" / "src"
THIRD_PARTY = FW_SRC / "third_party"

_TIER2_CXX_BEHAVIOR = r"""
#include <stdint.h>
#include <string.h>
#include <vector>

#include "cores/core_registry.h"
#include "storage/battery_save.h"
#include "storage/path_key.h"
#include "storage/save_state_format.h"
#include "test_check.h"
#include "ui/console_catalog.h"

constexpr bool verifyTier2Descriptors() {
  constexpr RomType kTier2Types[6] = {
      ROM_GENESIS, ROM_SNES, ROM_WSWAN, ROM_NGP, ROM_LYNX, ROM_COLEM,
  };
  for (int k = 0; k < 6; ++k) {
    const int idx = 9 + k;
    const auto& desc = CoreRegistry::detail::kCores[idx];
    if (desc.emulatorIndex != idx || desc.romType != kTier2Types[k]) return false;
    if (!desc.start || !desc.stepFrame || !desc.destroy || !desc.saveState || !desc.loadState) return false;
  }
  return true;
}
static_assert(verifyTier2Descriptors(), "Tier-2 core registry descriptors mismatch");

int main() {
  static const RomType kTier2Types[6] = {
      ROM_GENESIS, ROM_SNES, ROM_WSWAN, ROM_NGP, ROM_LYNX, ROM_COLEM,
  };
  static const char* kTier2Paths[6] = {
      "/roms/md/sonic3.md", "/roms/snes/chrono.sfc", "/roms/ws/klonoa.wsc",
      "/roms/ngp/motm.ngc", "/roms/lynx/chips.lnx", "/roms/coleco/dk.col",
  };

  for (int k = 0; k < 6; ++k) {
    const int idx = 9 + k;
    const ConsoleCatalog::ConsoleSpec* spec = ConsoleCatalog::findByType(kTier2Types[k]);
    BMO_CHECK(spec != nullptr && spec->emulatorIndex == idx);
    BMO_CHECK(spec->viewW > 0 && spec->viewH > 0);
    BMO_CHECK(spec->viewX + spec->viewW <= 320 && spec->viewY + spec->viewH <= 240);
    BMO_CHECK(ConsoleCatalog::determineTypeFromFilename(kTier2Paths[k]) == kTier2Types[k]);

    // Verify SaveStateFormat BSS2 validation accepts each Tier-2 core ID 9..14.
    SaveStateFormat::HeaderV2 hdr{};
    hdr.magic = SaveStateFormat::MAGIC_V2;
    hdr.version = SaveStateFormat::VERSION_V2;
    hdr.headerSize = sizeof(hdr);
    hdr.coreId = static_cast<uint32_t>(idx);
    hdr.pathKeyHash = RomKey::crc32RelativePath(kTier2Paths[k]);
    hdr.romSize = 65536U;
    hdr.stateSize = 512U;
    hdr.ramSize = 0U;
    hdr.crc32 = 0x13572468U;
    SaveStateFormat::DecodedHeader decoded{};
    BMO_CHECK(SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr),
                                            sizeof(hdr) + 512U, static_cast<uint8_t>(idx),
                                            hdr.pathKeyHash, 65536U, 512U, 0U, &decoded));
    BMO_CHECK(decoded.header.coreId == static_cast<uint32_t>(idx));
  }

  // SNES battery sizing across LoROM + 512-byte SMC copier header + HiROM.
  std::vector<uint8_t> snesLoRom(512U + 0x10000U, 0U);
  const size_t loOff = 512U + 0x7FB0U;
  snesLoRom[loOff + 0x25U] = 0x20U;
  snesLoRom[loOff + 0x26U] = 0x02U;
  snesLoRom[loOff + 0x28U] = 0x03U; // 8 KB SRAM
  snesLoRom[loOff + 0x2CU] = 0xFFU;
  snesLoRom[loOff + 0x2DU] = 0xFEU;
  snesLoRom[loOff + 0x2EU] = 0x00U;
  snesLoRom[loOff + 0x2FU] = 0x01U;
  BMO_CHECK(BatterySave::snesBatteryBytes(snesLoRom.data(), snesLoRom.size()) == 8192U);

  std::vector<uint8_t> lynxRom(128U, 0U);
  memcpy(lynxRom.data(), "LYNX", 4);
  lynxRom[0x3C] = 0x01U;
  BMO_CHECK(CoreRegistry::detail::hasLynxBattery(lynxRom.data(), lynxRom.size()));
  lynxRom[0x3C] = 0x00U;
  BMO_CHECK(!CoreRegistry::detail::hasLynxBattery(lynxRom.data(), lynxRom.size()));
  return 0;
}
"""

_RACE_DRIVER_C = r"""
#include <stdint.h>
#include <stdlib.h>

extern int test_ngp_core(void);

void* heap_caps_malloc(size_t s, uint32_t c) { (void)c; return malloc(s ? s : 1U); }
void* heap_caps_calloc(size_t n, size_t s, uint32_t c) { (void)c; return calloc(n ? n : 1U, s ? s : 1U); }
void* heap_caps_realloc(void* p, size_t s, uint32_t c) { (void)c; return realloc(p, s ? s : 1U); }
void heap_caps_free(void* p) { free(p); }

int main(void) {
  return test_ngp_core();
}
"""


class TestTier2FirmwareValidation(unittest.TestCase):
    """Compiled C/C++ behavioral checks across all 6 Tier-2 platforms."""

    def test_tier2_registry_catalog_battery_and_state_cxx(self) -> None:
        build_and_run_firmware_cxx(self, _TIER2_CXX_BEHAVIOR)

    def test_tier2_ngp_race_core_wasi(self) -> None:
        race_dir = THIRD_PARTY / "race"
        c_sources = [
            race_dir / "cz80.c",
            race_dir / "cz80_support.c",
            race_dir / "flash.c",
            race_dir / "graphics.c",
            race_dir / "input.c",
            race_dir / "neopopsound.c",
            race_dir / "ngpBios.c",
            race_dir / "race-memory.c",
            race_dir / "sound.c",
            race_dir / "state.c",
            race_dir / "tlcs900h.c",
            REPO_ROOT / "tests" / "host" / "bridge_ngp.c",
        ]
        with tempfile.TemporaryDirectory() as tmp:
            driver = Path(tmp) / "ngp_driver.c"
            driver.write_text(_RACE_DRIVER_C, encoding="utf-8")
            res = build_and_run_wasi_c(
                self,
                [driver, *c_sources],
                include_dirs=(race_dir, FW_SRC),
            )
            self.assertIn("[HOST_CORE] NGP_COLOR STATUS=PASS", res.stdout)


if __name__ == "__main__":
    unittest.main()
