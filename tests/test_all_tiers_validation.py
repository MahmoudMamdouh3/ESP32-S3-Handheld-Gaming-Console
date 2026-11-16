"""Compiled behavioral, partition, key-parity, and doc-consistency tests across all 16 catalog entries."""

from __future__ import annotations

import csv
import itertools
import re
import sys
import unittest
from pathlib import Path

from host_test_support import build_and_run_firmware_cxx, build_and_run_native_cxx

REPO_ROOT = Path(__file__).resolve().parents[1]
FW_DIR = REPO_ROOT / "firmware" / "BmoGameboy"
FW_SRC = FW_DIR / "src"

sys.path.insert(0, str(REPO_ROOT / "scripts"))
import rom_key  # noqa: E402

_ALL_TIERS_CXX = r"""
#include <stdint.h>
#include <string.h>

#include "cores/core_registry.h"
#include "hal/config.h"
#include "hal/pin_map.h"
#include "storage/rom_file_filter.h"
#include "test_check.h"
#include "ui/console_catalog.h"
#include "ui/theme.h"

int main() {
  static_assert(FEATURE_BATTERY_MONITOR == 0, "Hardware hard stop: FEATURE_BATTERY_MONITOR=0");
  static_assert(FEATURE_AUDIO == 0, "Hardware hard stop: FEATURE_AUDIO=0");
  static_assert(ConsoleCatalog::CONSOLE_COUNT == 16, "1 Favorites + 15 hardware entries");

  bool seenIndex[16] = {};
  for (int i = 0; i < ConsoleCatalog::CONSOLE_COUNT; ++i) {
    const RomType t = ConsoleCatalog::CONSOLES[i];
    const ConsoleCatalog::ConsoleSpec* spec = ConsoleCatalog::findByType(t);
    BMO_CHECK(spec != nullptr);
    BMO_CHECK(spec->emulatorIndex >= -1 && spec->emulatorIndex < 15);
    BMO_CHECK(!seenIndex[spec->emulatorIndex + 1]);
    seenIndex[spec->emulatorIndex + 1] = true;
    BMO_CHECK(spec->viewW > 0 && spec->viewH > 0);
    BMO_CHECK(spec->viewX >= 0 && spec->viewY >= 0);
    BMO_CHECK(spec->viewX + spec->viewW <= 320 && spec->viewY + spec->viewH <= 240);
  }
  for (bool seen : seenIndex) {
    BMO_CHECK(seen);
  }

  for (const auto& route : ConsoleCatalog::kExtensionRoutes) {
    char upperName[64];
    snprintf(upperName, sizeof(upperName), "/roms/test_game%s", route.ext);
    BMO_CHECK(ConsoleCatalog::determineTypeFromFilename(upperName) == route.type);
    for (size_t k = 0; upperName[k] != '\0'; ++k) {
      if (upperName[k] >= 'a' && upperName[k] <= 'z') {
        upperName[k] = static_cast<char>(upperName[k] - 'a' + 'A');
      }
    }
    BMO_CHECK(ConsoleCatalog::determineTypeFromFilename(upperName) == route.type);
  }

  BMO_CHECK(RomFileFilter::isIgnoredName(".DS_Store"));
  BMO_CHECK(RomFileFilter::isIgnoredName("System Volume Information"));
  BMO_CHECK(Theme::PANEL_DEEP_TEAL != Theme::PANEL_DPAD_YELLOW);
  BMO_CHECK(Theme::swapBytes(Theme::PANEL_DEEP_TEAL) == Theme::makeUiColor(26, 75, 66));
  return 0;
}
"""


def _parse_size_or_offset(raw: str) -> int:
    text = raw.strip().lower()
    if text.endswith("k"):
        return int(text[:-1], 0) * 1024
    if text.endswith("m"):
        return int(text[:-1], 0) * 1024 * 1024
    return int(text, 0)


class TestAllConsolesFirmwareValidation(unittest.TestCase):
    """Cross-tier C++ invariants, Python/C++ hash parity, partition math, and clean-clone hygiene."""

    def test_all_16_catalog_and_15_core_invariants_cxx(self) -> None:
        build_and_run_firmware_cxx(self, _ALL_TIERS_CXX)

    def test_rom_key_python_vs_cpp_parity(self) -> None:
        sample_paths = [
            "gb/Tetris (World).gb",
            "/gbc/Zelda - Oracle of Ages (U) [C][!].gbc",
            "nes\\Super Mario Bros. (Japan, USA).nes",
            "snes/Chrono Trigger (USA).sfc",
            "pico-8/Celeste Classic.p8",
            "deep/sub/folder/Very Long Cartridge Name That Exceeds Twenty Three Chars.md",
            "123/game.a26",
        ]
        proc = build_and_run_native_cxx(
            self,
            [REPO_ROOT / "tests" / "host" / "rom_key_parity.cpp"],
            include_dirs=(FW_SRC / "storage",),
            args=tuple(sample_paths),
            workdir=REPO_ROOT,
        )
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        cpp_keys = [line.strip() for line in proc.stdout.splitlines() if line.strip()]
        self.assertEqual(len(cpp_keys), len(sample_paths))
        for raw_path, cpp_key in zip(sample_paths, cpp_keys, strict=True):
            self.assertEqual(cpp_key, rom_key.make_rom_key(raw_path), raw_path)

    def test_partitions_sketch_and_compatibility_doc_consistency(self) -> None:
        sketch_text = (FW_DIR / "sketch.yaml").read_text(encoding="utf-8")
        self.assertIn(
            "FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi",
            sketch_text,
        )

        rows: list[tuple[str, int, int]] = []
        with (FW_DIR / "partitions.csv").open(encoding="utf-8", newline="") as handle:
            for row in csv.reader(handle):
                if not row or row[0].strip().startswith("#"):
                    continue
                name = row[0].strip()
                offset = _parse_size_or_offset(row[3])
                size = _parse_size_or_offset(row[4])
                rows.append((name, offset, size))
        self.assertTrue(any(name == "app0" and size == 0x800000 for name, _, size in rows))
        rows.sort(key=lambda item: item[1])
        for (_, prev_off, prev_sz), (_, next_off, _) in itertools.pairwise(rows):
            self.assertLessEqual(prev_off + prev_sz, next_off)
        last_off, last_sz = rows[-1][1], rows[-1][2]
        self.assertLessEqual(last_off + last_sz, 16 * 1024 * 1024)

        compat_text = (REPO_ROOT / "docs" / "compatibility.md").read_text(encoding="utf-8")
        documented_types = set(re.findall(r"`(ROM_[A-Z0-9_]+)`", compat_text))
        self.assertIn("ROM_FAVORITES", documented_types)
        self.assertEqual(len(documented_types - {"ROM_FAVORITES"}), 15)

    def test_clean_clone_repository_hygiene(self) -> None:
        sys.path.insert(0, str(REPO_ROOT / "scripts"))
        import validate_repo

        issues = validate_repo.check_game_data() + validate_repo.check_structural_soundness()
        self.assertEqual(issues, [], "\n".join(issues))


if __name__ == "__main__":
    unittest.main()
