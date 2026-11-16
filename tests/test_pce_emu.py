"""Behavioral tests for the PC Engine / TurboGrafx-16 (HuExpress) emulator core."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from host_test_support import build_and_run_firmware_cxx, build_and_run_wasi_c

REPO_ROOT = Path(__file__).resolve().parents[1]
THIRD_PARTY = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "third_party"

_PCE_DRIVER_C = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pce-go.h"
#include "pce.h"

extern int test_pce_core(void);

int main(void) {
  if (test_pce_core() != 0) {
    return 1;
  }

  /* Verify 512-byte US copier header stripping ((romSize % 8192) == 512). */
  const size_t rom_size = 512U + 8192U;
  uint8_t* rom = (uint8_t*)calloc(1U, rom_size);
  if (!rom) {
    return 2;
  }
  rom[512U + 0x1FFE] = 0x00U;
  rom[512U + 0x1FFF] = 0xE0U;
  const uint8_t prog[] = {
      0xA9U, 0xF8U, 0x53U, 0x02U,        /* LDA #$F8 ; TAM #$02 */
      0xA9U, 0xFFU, 0x53U, 0x01U,        /* LDA #$FF ; TAM #$01 */
      0xA9U, 0x77U, 0x8DU, 0x00U, 0x20U, /* LDA #$77 ; STA $2000 */
      0x9CU, 0x02U, 0x04U,               /* STZ $0402 */
      0x9CU, 0x03U, 0x04U,               /* STZ $0403 */
      0xA9U, 0xDBU, 0x8DU, 0x04U, 0x04U, /* LDA #$DB ; STA $0404 */
      0xA9U, 0x01U, 0x8DU, 0x05U, 0x04U, /* LDA #$01 ; STA $0405 */
      0x80U, 0xFEU                       /* BRA -2 */
  };
  memcpy(rom + 512U, prog, sizeof(prog));

  if (InitPCE(22050, false) != 0 || LoadCardBorrowed(rom, rom_size) != 0) {
    ShutdownPCE();
    free(rom);
    return 3;
  }
  ResetPCE(false);
  for (int f = 0; f < 8; ++f) {
    StepPCE();
  }
  const uint8_t ram0 = PCE.RAM ? PCE.RAM[0] : 0U;
  const uint16_t vce0 = PCE.VCE.regs ? PCE.VCE.regs[0].W : 0U;
  const uint32_t rom_banks = PCE.ROM_SIZE;
  ShutdownPCE();
  free(rom);

  if (rom_banks != 1U || ram0 != 0x77U || vce0 != 0x01DBU) {
    fprintf(stderr, "Copier boot mismatch: banks=%u ram0=0x%02X vce0=0x%04X\n",
            (unsigned)rom_banks, ram0, vce0);
    return 4;
  }
  return 0;
}
"""

_PCE_CXX_BEHAVIOR = r"""
#include <stdint.h>
#include <string.h>

#include "cores/core_registry.h"
#include "storage/battery_save.h"
#include "storage/save_state_format.h"
#include "test_check.h"
#include "ui/console_catalog.h"

int main() {
  static_assert(CoreRegistry::detail::kCores[6].emulatorIndex == 6, "PCE core index must be 6");
  static_assert(CoreRegistry::detail::kCores[6].romType == ROM_PCE, "Core 6 must be ROM_PCE");

  const ConsoleCatalog::ConsoleSpec* spec = ConsoleCatalog::findByType(ROM_PCE);
  BMO_CHECK(spec != nullptr);
  BMO_CHECK(spec->viewX == 32 && spec->viewY == 0 && spec->viewW == 256 && spec->viewH == 240);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("/roms/pce/Rondo.pce") == ROM_PCE);

  const uint8_t validBram[4] = {'H', 'U', 'B', 'M'};
  const uint8_t badBram[4] = {'B', 'A', 'D', '!'};
  BMO_CHECK(BatterySave::pceBramHasHeader(validBram, sizeof(validBram)));
  BMO_CHECK(!BatterySave::pceBramHasHeader(badBram, sizeof(badBram)));
  BMO_CHECK(!BatterySave::pceBramHasHeader(validBram, 3U));

  SaveStateFormat::HeaderV2 hdr{};
  hdr.magic = SaveStateFormat::MAGIC_V2;
  hdr.version = SaveStateFormat::VERSION_V2;
  hdr.headerSize = sizeof(hdr);
  hdr.coreId = 6U;
  hdr.pathKeyHash = 0x11223344U;
  hdr.romSize = 262144U;
  hdr.stateSize = 1024U;
  hdr.ramSize = 2048U;
  hdr.crc32 = 0xA5A55A5AU;
  SaveStateFormat::DecodedHeader decoded{};
  BMO_CHECK(SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr),
                                          sizeof(hdr) + 3072U, 6U, 0x11223344U, 262144U,
                                          1024U, 2048U, &decoded));
  BMO_CHECK(decoded.header.stateSize == 1024U && decoded.header.ramSize == 2048U &&
            decoded.header.crc32 == 0xA5A55A5AU && decoded.payloadOffset == sizeof(hdr) && !decoded.isLegacyV1);
  BMO_CHECK(!SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr),
                                           sizeof(hdr) + 3072U, 7U, 0x11223344U, 262144U,
                                           1024U, 2048U, &decoded));
  return 0;
}
"""


class TestPceEmulatorCore(unittest.TestCase):
    """Compiled WASI + C++17 behavioral checks for HuExpress and PCE firmware metadata."""

    def test_pce_huexpress_copier_vce_and_boot_wasi(self) -> None:
        hu_dir = THIRD_PARTY / "huexpress"
        c_sources = [
            hu_dir / "gfx.c",
            hu_dir / "h6280.c",
            hu_dir / "pce-go.c",
            hu_dir / "pce.c",
            hu_dir / "psg.c",
            REPO_ROOT / "tests" / "host" / "bridge_pce.c",
        ]
        with tempfile.TemporaryDirectory() as tmp:
            driver = Path(tmp) / "pce_driver.c"
            driver.write_text(_PCE_DRIVER_C, encoding="utf-8")
            res = build_and_run_wasi_c(
                self,
                [driver, *c_sources],
                include_dirs=(hu_dir, REPO_ROOT / "firmware" / "BmoGameboy" / "src"),
            )
            self.assertIn("[HOST_CORE] PC_ENGINE STATUS=PASS", res.stdout)

    def test_pce_catalog_bram_and_state_format_cxx(self) -> None:
        build_and_run_firmware_cxx(self, _PCE_CXX_BEHAVIOR)


if __name__ == "__main__":
    unittest.main()
