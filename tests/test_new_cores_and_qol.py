"""Executed C/C++ behavioral tests for Atari 2600/Lynx, ColecoVision/SG-1000, NGPC, and QoL modules."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from host_test_support import build_and_run_firmware_cxx, build_and_run_native_cxx, build_and_run_wasi_c

REPO_ROOT = Path(__file__).resolve().parents[1]
FW_SRC = REPO_ROOT / "firmware" / "BmoGameboy" / "src"
THIRD_PARTY = FW_SRC / "third_party"

_COLECO_SG_DRIVER_C = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smsplus_bmo.h"

void* heap_caps_malloc(size_t s, uint32_t c) { (void)c; return malloc(s ? s : 1U); }
void* heap_caps_calloc(size_t n, size_t s, uint32_t c) { (void)c; return calloc(n ? n : 1U, s ? s : 1U); }
void* heap_caps_realloc(void* p, size_t s, uint32_t c) { (void)c; return realloc(p, s ? s : 1U); }
void heap_caps_free(void* p) { free(p); }

static int run_tms_console(BmoSmsConsoleMode mode, uint8_t* bios8k) {
  uint8_t rom[8192];
  memset(rom, 0, sizeof(rom));
  const uint8_t z80_prog[] = {
      0x3EU, 0xC0U, 0xD3U, 0xBFU, /* LD A,C0h ; OUT (BFh),A */
      0x3EU, 0x81U, 0xD3U, 0xBFU, /* LD A,81h ; OUT (BFh),A (VDP R1 enable) */
      0x3EU, 0x5AU, 0xD3U, 0xBFU, /* LD A,5Ah ; OUT (BFh),A */
      0x3EU, 0x87U, 0xD3U, 0xBFU, /* LD A,87h ; OUT (BFh),A (VDP R7 backdrop) */
      0x18U, 0xFEU                /* JR -2 */
  };
  if (mode == BMO_SMS_CONSOLE_COLECO) {
    rom[0] = 0x55U;
    rom[1] = 0xAAU;
    rom[0x0A] = 0x20U;
    rom[0x0B] = 0x80U;
    memcpy(rom + 0x20U, z80_prog, sizeof(z80_prog));
  } else {
    memcpy(rom, z80_prog, sizeof(z80_prog));
  }

  if (!bmo_sms_init(rom, sizeof(rom), mode, bios8k)) {
    return 10;
  }
  bmo_sms_set_input(BMO_SMS_INPUT_BUTTON1 | (5 << BMO_SMS_COLECO_KEYPAD_SHIFT), BMO_SMS_SYS_START);
  for (int f = 0; f < 16; ++f) {
    bmo_sms_run_frame(0);
  }
  uint16_t* panel = (uint16_t*)calloc(256U * 192U, sizeof(uint16_t));
  if (!panel) {
    bmo_sms_destroy();
    return 11;
  }
  bmo_sms_render_panel565(panel);
  int nz = 0;
  for (size_t i = 0; i < 256U * 192U; ++i) {
    if (panel[i] != 0U) {
      ++nz;
    }
  }
  free(panel);
  bmo_sms_destroy();
  return nz > 1000 ? 0 : 12;
}

int main(void) {
  uint8_t coleco_bios[8192];
  memset(coleco_bios, 0xC9U, sizeof(coleco_bios));
  /* RST 00h -> jump straight to cartridge entry $8020. */
  coleco_bios[0] = 0xC3U;
  coleco_bios[1] = 0x20U;
  coleco_bios[2] = 0x80U;

  if (run_tms_console(BMO_SMS_CONSOLE_COLECO, coleco_bios) != 0) {
    fprintf(stderr, "ColecoVision Z80/TMS9918 execution failed\n");
    return 1;
  }
  if (run_tms_console(BMO_SMS_CONSOLE_SG1000, NULL) != 0) {
    fprintf(stderr, "SG-1000 Z80/TMS9918 execution failed\n");
    return 2;
  }
  return 0;
}
"""

_NEW_CORES_CONTRACT_CXX = r"""
#include <stdint.h>
#include <string.h>
#include <vector>

#include "cores/core_registry.h"
#include "storage/rom_file_filter.h"
#include "test_check.h"
#include "ui/console_catalog.h"

int main() {
  static_assert(CoreRegistry::detail::kCores[7].romType == ROM_ATARI, "Core 7 = Atari 2600");
  static_assert(CoreRegistry::detail::kCores[12].romType == ROM_NGP, "Core 12 = NGP");
  static_assert(CoreRegistry::detail::kCores[13].romType == ROM_LYNX, "Core 13 = Lynx");
  static_assert(CoreRegistry::detail::kCores[14].romType == ROM_COLEM, "Core 14 = ColecoVision");

  // Validate heuristic filtering for headerless .col and .sg binary ROMs vs plain text docs.
  const uint8_t colecoSig[32] = {0xAAU, 0x55U, 0x00U, 0x01U, 0x00U, 0x80U};
  const uint8_t asciiDoc[32] = {'T', 'h', 'i', 's', ' ', 'i', 's', ' ', 'n', 'o', 't', ' ', 'a', ' ', 'R', 'O',
                                'M', ' ', 'f', 'i', 'l', 'e', ' ', 'a', 't', ' ', 'a', 'l', 'l', '!', '\n', 0};
  BMO_CHECK(RomFileFilter::isPlausibleFile("donkey.col", 8192U, colecoSig, sizeof(colecoSig), nullptr, 0U));
  BMO_CHECK(!RomFileFilter::isPlausibleFile("notes.sg", 1024U, asciiDoc, sizeof(asciiDoc), nullptr, 0U));

  std::vector<uint8_t> binSg(8192U, 0xFFU);
  binSg[0] = 0x3EU;
  binSg[1] = 0x01U;
  binSg[2] = 0xC3U;
  binSg[3] = 0x00U;
  BMO_CHECK(RomFileFilter::isPlausibleFile("flicky.sg", binSg.size(), binSg.data(), 32U, nullptr, 0U));

  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("pitfall.a26") == ROM_ATARI);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("msx.sg") == ROM_COLEM);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("ladybug.col") == ROM_COLEM);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("california.lnx") == ROM_LYNX);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("snk_capcom.ngc") == ROM_NGP);
  return 0;
}
"""

_HANDY_DRIVER_CPP = r"""
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "handy.h"

int main() {
  std::vector<uint8_t> rom(10U + 0x0101U, 0xEAU);
  rom[0] = 0x80U;
  rom[1] = 0x08U;
  rom[2] = 0x04U;
  rom[3] = 0x04U;
  rom[4] = 0x01U;
  rom[5] = 0x01U;
  memcpy(&rom[6], "BS93", 4);
  const uint8_t prog[] = {
      0xA9U, 0x5CU, 0x85U, 0x80U,
      0xA9U, 0x0DU, 0x8DU, 0x92U, 0xFDU,
      0xA9U, 0x00U, 0x8DU, 0x94U, 0xFDU,
      0xA9U, 0x20U, 0x8DU, 0x95U, 0xFDU,
      0xA9U, 0x0FU, 0x8DU, 0xA0U, 0xFDU,
      0x8DU, 0xA1U, 0xFDU,
      0x8DU, 0xB0U, 0xFDU,
      0x8DU, 0xB1U, 0xFDU,
      0xA2U, 0x00U, 0xA9U, 0xFFU,
      0x9DU, 0x00U, 0x20U, 0x9DU, 0x00U, 0x21U,
      0xE8U, 0xD0U, 0xF7U, 0x80U, 0xFEU,
  };
  memcpy(&rom[10], prog, sizeof(prog));

  uint16_t* raw_fb = static_cast<uint16_t*>(calloc(HANDY_SCREEN_WIDTH * HANDY_SCREEN_HEIGHT, sizeof(uint16_t)));
  if (!raw_fb) {
    return 1;
  }
  gAudioEnabled = 0;
  gPrimaryFrameBuffer = reinterpret_cast<UBYTE*>(raw_fb);
  CSystem lynx(rom.data(), static_cast<ULONG>(rom.size()), MIKIE_PIXEL_FORMAT_16BPP_565, HANDY_AUDIO_SAMPLE_FREQ);
  if (lynx.mFileType == HANDY_FILETYPE_ILLEGAL) {
    gPrimaryFrameBuffer = nullptr;
    free(raw_fb);
    return 2;
  }
  lynx.mMikie->SetRotation(MIKIE_NO_ROTATE);
  for (int f = 0; f < 50; ++f) {
    lynx.SetButtonData(0);
    lynx.UpdateFrame(true);
  }
  const UBYTE* ram = lynx.GetRamPointer();
  const uint8_t marker = ram ? ram[0x80] : 0U;
  int nz = 0;
  for (int i = 0; i < HANDY_SCREEN_WIDTH * HANDY_SCREEN_HEIGHT; ++i) {
    if (raw_fb[i] != 0U) {
      ++nz;
    }
  }
  gPrimaryFrameBuffer = nullptr;
  free(raw_fb);
  return (marker == 0x5CU && nz >= 100) ? 0 : 3;
}
"""


class TestNewCoresAndQolBehavior(unittest.TestCase):
    """Compiled firmware & emulator behavior verification for QoL and new Tier-1/2 platforms."""

    def test_qol_buttons_diagnostics_konami_and_save_manager_native(self) -> None:
        inc_dirs = (
            REPO_ROOT / "tests" / "host" / "fakes" / "runtime",
            REPO_ROOT / "tests" / "host" / "fakes",
            REPO_ROOT / "tests" / "host",
            FW_SRC,
            FW_SRC / "app",
            FW_SRC / "cores",
            FW_SRC / "hal",
            FW_SRC / "storage",
            FW_SRC / "ui",
        )
        res_app = build_and_run_native_cxx(
            self,
            [
                REPO_ROOT / "tests" / "host" / "app_behavior_test.cpp",
                FW_SRC / "app" / "app_diagnostics.cpp",
                FW_SRC / "app" / "app_menus.cpp",
                FW_SRC / "hal" / "buttons.cpp",
                FW_SRC / "hal" / "profiler.cpp",
                FW_SRC / "hal" / "spi_arbiter.cpp",
            ],
            include_dirs=inc_dirs,
            defines=("BMO_HOST_TEST=1", "LOG_LEVEL=0", "FEATURE_PROFILER=1"),
            workdir=REPO_ROOT,
        )
        self.assertEqual(res_app.returncode, 0, res_app.stdout + res_app.stderr)

        res_save = build_and_run_native_cxx(
            self,
            [
                REPO_ROOT / "tests" / "host" / "save_manager_behavior_test.cpp",
                FW_SRC / "storage" / "save_manager.cpp",
                FW_SRC / "hal" / "spi_arbiter.cpp",
            ],
            include_dirs=inc_dirs,
            defines=("BMO_HOST_TEST=1", "LOG_LEVEL=0"),
            workdir=REPO_ROOT,
        )
        self.assertEqual(res_save.returncode, 0, res_save.stdout + res_save.stderr)

    def test_coleco_sg1000_smsplus_wasi(self) -> None:
        sms_dir = THIRD_PARTY / "smsplus"
        c_sources = [
            sms_dir / "render.c",
            sms_dir / "sms.c",
            sms_dir / "smsplus_bmo.c",
            sms_dir / "sn76496.c",
            sms_dir / "system.c",
            sms_dir / "tms.c",
            sms_dir / "vdp.c",
            sms_dir / "z80.c",
        ]
        with tempfile.TemporaryDirectory() as tmp:
            driver = Path(tmp) / "coleco_sg_driver.c"
            driver.write_text(_COLECO_SG_DRIVER_C, encoding="utf-8")
            build_and_run_wasi_c(self, [driver, *c_sources], include_dirs=(sms_dir, FW_SRC))

    def test_lynx_handy_engine_wasi(self) -> None:
        handy_dir = THIRD_PARTY / "handy"
        cpp_sources = [
            handy_dir / "cart.cpp",
            handy_dir / "eeprom.cpp",
            handy_dir / "lynxdec.cpp",
            handy_dir / "mikie.cpp",
            handy_dir / "susie.cpp",
            handy_dir / "system.cpp",
        ]
        with tempfile.TemporaryDirectory() as tmp:
            driver = Path(tmp) / "handy_driver.cpp"
            driver.write_text(_HANDY_DRIVER_CPP, encoding="utf-8")
            build_and_run_wasi_c(
                self,
                [driver, *cpp_sources],
                include_dirs=(handy_dir,),
                defines=("ANSI_GCC",),
            )

    def test_new_cores_filter_and_registry_contract_cxx(self) -> None:
        build_and_run_firmware_cxx(self, _NEW_CORES_CONTRACT_CXX)


if __name__ == "__main__":
    unittest.main()
