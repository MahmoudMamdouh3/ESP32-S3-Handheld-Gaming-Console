"""Behavioral firmware and host-core validation tests for all Tier-1 consoles."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

from host_test_support import build_and_run_firmware_cxx, build_and_run_native_cxx, build_and_run_wasi_c

REPO_ROOT = Path(__file__).resolve().parents[1]
FW_SRC = REPO_ROOT / "firmware" / "BmoGameboy" / "src"
THIRD_PARTY = FW_SRC / "third_party"

_TIER1_CXX_BEHAVIOR = r"""
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "cores/core_registry.h"
#include "cores/doom_wad_path.h"
#include "cores/psram_zone_budget.h"
#include "hal/config.h"
#include "hal/psram_alloc.h"
#include "storage/battery_save.h"
#include "test_check.h"
#include "ui/console_catalog.h"

constexpr bool verifyTier1Descriptors() {
  constexpr RomType kTier1Types[9] = {
      ROM_GBC, ROM_GB, ROM_NES, ROM_WAD, ROM_SMS, ROM_GG, ROM_PCE, ROM_ATARI, ROM_PICO8,
  };
  for (int i = 0; i < 9; ++i) {
    const auto& desc = CoreRegistry::detail::kCores[i];
    if (desc.emulatorIndex != i || desc.romType != kTier1Types[i]) return false;
  }
  return CoreRegistry::detail::kCores[0].supportsDmgPalette && CoreRegistry::detail::kCores[1].supportsDmgPalette &&
         !CoreRegistry::detail::kCores[2].supportsDmgPalette;
}
static_assert(verifyTier1Descriptors(), "Tier-1 core registry descriptors mismatch");

int main() {
  static_assert(FEATURE_BATTERY_MONITOR == 0, "Battery ADC must stay 0");
  static_assert(FEATURE_AUDIO == 0, "I2S DAC must stay 0");
  static_assert(TFT_CS == 10 && TFT_DC == 8 && TFT_RST == 9 && TFT_SCK == 12 && TFT_MOSI == 11, "TFT pins fixed");
  static_assert(SD_CS == 13 && SD_MISO == 15, "SD FSPI pins fixed");

  static const RomType kTier1Types[9] = {
      ROM_GBC, ROM_GB, ROM_NES, ROM_WAD, ROM_SMS, ROM_GG, ROM_PCE, ROM_ATARI, ROM_PICO8,
  };
  static const char* kTier1Paths[9] = {
      "/roms/gbc/zelda.gbc", "/roms/gb/tetris.gb", "/roms/nes/mario.nes",
      "/roms/doom/doom1.wad", "/roms/sms/sonic.sms", "/roms/gg/shinobi.gg",
      "/roms/pce/bonk.pce", "/roms/atari/pitfall.a26", "/roms/pico8/celeste.p8",
  };

  for (int i = 0; i < 9; ++i) {
    const ConsoleCatalog::ConsoleSpec* spec = ConsoleCatalog::findByType(kTier1Types[i]);
    BMO_CHECK(spec != nullptr);
    BMO_CHECK(spec->emulatorIndex == i);
    BMO_CHECK(spec->viewW > 0 && spec->viewH > 0);
    BMO_CHECK(spec->viewX >= 0 && spec->viewY >= 0);
    BMO_CHECK(spec->viewX + spec->viewW <= 320 && spec->viewY + spec->viewH <= 240);
    BMO_CHECK(ConsoleCatalog::determineTypeFromFilename(kTier1Paths[i]) == kTier1Types[i]);
  }

  char wadPath[DoomWadPath::MAX_WAD_PATH_BYTES] = {};
  BMO_CHECK(DoomWadPath::build("DOOM1.WAD", wadPath, sizeof(wadPath)));
  BMO_CHECK(strcmp(wadPath, "/sd/DOOM1.WAD") == 0);
  BMO_CHECK(!DoomWadPath::build(nullptr, wadPath, sizeof(wadPath)));
  BMO_CHECK(!DoomWadPath::build("", wadPath, sizeof(wadPath)));

  BMO_CHECK(DoomEmu::computeZoneBytes(2U * 1024U * 1024U) == 0U);
  const size_t sixMbZone = DoomEmu::computeZoneBytes(6U * 1024U * 1024U);
  BMO_CHECK(sixMbZone >= DoomEmu::DOOM_ZONE_FLOOR_BYTES && sixMbZone <= 6U * 1024U * 1024U);
  BMO_CHECK(DoomEmu::computeZoneBytes(12U * 1024U * 1024U) == DoomEmu::DOOM_ZONE_CAP_BYTES);

  BMO_CHECK(BatterySave::gameBoyBatteryBytes(0x13U, 3U) == 0x8000U);
  BMO_CHECK(BatterySave::nesHeaderHasBattery(0x02U));
  BMO_CHECK(!BatterySave::nesHeaderHasBattery(0x01U));

  void* overflowed = bmo_psram_calloc((SIZE_MAX / 2U) + 1U, 4U);
  BMO_CHECK(overflowed == nullptr);
  return 0;
}
"""

_SMS_GG_DRIVER_C = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smsplus_bmo.h"

void* heap_caps_malloc(size_t s, uint32_t c) { (void)c; return malloc(s ? s : 1U); }
void* heap_caps_calloc(size_t n, size_t s, uint32_t c) { (void)c; return calloc(n ? n : 1U, s ? s : 1U); }
void* heap_caps_realloc(void* p, size_t s, uint32_t c) { (void)c; return realloc(p, s ? s : 1U); }
void heap_caps_free(void* p) { free(p); }

static int run_sms_or_gg(BmoSmsConsoleMode mode) {
  uint8_t rom[32768];
  memset(rom, 0, sizeof(rom));
  /* Set Mode 4 (VDP R0=0x06, R1=0x40, R7=0x00) and write sprite/backdrop CRAM[0x10..0x11]. */
  const uint8_t z80_prog[] = {
      0xF3U,                      /* DI */
      0x3EU, 0x06U, 0xD3U, 0xBFU, /* LD A,06h ; OUT (BFh),A */
      0x3EU, 0x80U, 0xD3U, 0xBFU, /* LD A,80h ; OUT (BFh),A (VDP R0 Mode4) */
      0x3EU, 0x40U, 0xD3U, 0xBFU, /* LD A,40h ; OUT (BFh),A */
      0x3EU, 0x81U, 0xD3U, 0xBFU, /* LD A,81h ; OUT (BFh),A (VDP R1 Display On) */
      0x3EU, 0x00U, 0xD3U, 0xBFU, /* LD A,00h ; OUT (BFh),A */
      0x3EU, 0x87U, 0xD3U, 0xBFU, /* LD A,87h ; OUT (BFh),A (VDP R7 Backdrop = sprite[0]) */
      0x3EU, 0x00U, 0xD3U, 0xBFU, /* LD A,00h ; OUT (BFh),A */
      0x3EU, 0xC0U, 0xD3U, 0xBFU, /* LD A,C0h ; OUT (BFh),A (CRAM[0] = BG color 0 for GG) */
      0x3EU, 0xFFU, 0xD3U, 0xBEU, /* LD A,FFh ; OUT (BEh),A */
      0x3EU, 0x0FU, 0xD3U, 0xBEU, /* LD A,0Fh ; OUT (BEh),A */
      0x3EU, 0x10U, 0xD3U, 0xBFU, /* LD A,10h ; OUT (BFh),A */
      0x3EU, 0xC0U, 0xD3U, 0xBFU, /* LD A,C0h ; OUT (BFh),A (SMS CRAM[16] = byte 0x10) */
      0x3EU, 0x0FU, 0xD3U, 0xBEU, /* LD A,0Fh ; OUT (BEh),A */
      0x3EU, 0x0FU, 0xD3U, 0xBEU, /* LD A,0Fh ; OUT (BEh),A */
      0x3EU, 0x20U, 0xD3U, 0xBFU, /* LD A,20h ; OUT (BFh),A */
      0x3EU, 0xC0U, 0xD3U, 0xBFU, /* LD A,C0h ; OUT (BFh),A (GG CRAM[16] = byte 0x20..0x21) */
      0x3EU, 0xFFU, 0xD3U, 0xBEU, /* LD A,FFh ; OUT (BEh),A */
      0x3EU, 0x0FU, 0xD3U, 0xBEU, /* LD A,0Fh ; OUT (BEh),A (GG RGB444 white) */
      0x18U, 0xFEU                /* JR -2 */
  };
  memcpy(rom, z80_prog, sizeof(z80_prog));
  if (!bmo_sms_init(rom, sizeof(rom), mode, NULL)) {
    return 10;
  }
  bmo_sms_set_input(BMO_SMS_INPUT_BUTTON1, 0);
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
  return nz > 500 ? 0 : 12;
}

int main(void) {
  if (run_sms_or_gg(BMO_SMS_CONSOLE_SMS) != 0) {
    fprintf(stderr, "SMS frame verification failed\n");
    return 1;
  }
  if (run_sms_or_gg(BMO_SMS_CONSOLE_GG) != 0) {
    fprintf(stderr, "Game Gear frame verification failed\n");
    return 2;
  }
  return 0;
}
"""


class TestTier1FirmwareValidation(unittest.TestCase):
    """Compiled C/C++ behavioral checks covering Tier-1 consoles and repo policies."""

    def test_tier1_registry_catalog_and_hal_safety_cxx(self) -> None:
        build_and_run_firmware_cxx(self, _TIER1_CXX_BEHAVIOR)

    def test_tier1_sms_and_gamegear_cores_wasi(self) -> None:
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
            driver = Path(tmp) / "sms_gg_driver.c"
            driver.write_text(_SMS_GG_DRIVER_C, encoding="utf-8")
            build_and_run_wasi_c(
                self,
                [driver, *c_sources],
                include_dirs=(sms_dir, FW_SRC),
            )

    def test_tier1_peanut_opcodes_and_gb_common_native(self) -> None:
        inc_dirs = (
            REPO_ROOT / "tests" / "host" / "fakes" / "runtime",
            REPO_ROOT / "tests" / "host" / "fakes",
            REPO_ROOT / "tests" / "host",
            FW_SRC,
            FW_SRC / "cores",
            FW_SRC / "hal",
            FW_SRC / "storage",
            FW_SRC / "ui",
        )
        res1 = build_and_run_native_cxx(
            self,
            [REPO_ROOT / "tests" / "host" / "peanut_opcode_test.cpp"],
            include_dirs=inc_dirs,
            defines=("BMO_HOST_TEST=1", "LOG_LEVEL=0"),
            run_cwd=REPO_ROOT,
        )
        self.assertEqual(res1.returncode, 0, res1.stdout + res1.stderr)

        res2 = build_and_run_native_cxx(
            self,
            [REPO_ROOT / "tests" / "host" / "gb_error_and_bmp_test.cpp", FW_SRC / "cores" / "gb_common.cpp"],
            include_dirs=inc_dirs,
            defines=("BMO_HOST_TEST=1", "LOG_LEVEL=0"),
            run_cwd=REPO_ROOT,
        )
        self.assertEqual(res2.returncode, 0, res2.stdout + res2.stderr)

    def test_validate_repo_clean_clone_policy(self) -> None:
        sys.path.insert(0, str(REPO_ROOT / "scripts"))
        import validate_repo

        issues: list[str] = []
        issues.extend(validate_repo.check_python_files())
        issues.extend(validate_repo.check_firmware_safety_guardrails())
        issues.extend(validate_repo.check_flash_budget_static())
        issues.extend(validate_repo.check_structural_soundness())
        issues.extend(validate_repo.check_game_data())
        self.assertEqual(issues, [], "\n".join(issues))


if __name__ == "__main__":
    unittest.main()
