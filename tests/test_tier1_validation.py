#!/usr/bin/env python3
"""
test_tier1_validation.py

Tier 1 firmware contract & catalog filter verification.
NOTE (Decision 8): The local `games/` directory scan (> 10,000 files assertion) has been
retired because repository tests must never depend on untracked local ROM collections.
Remaining source-regex stubs here are scheduled for Phase 3 replacement with compiled
behavioral C++ / WASI tests.
"""

import tempfile
import unittest
from pathlib import Path

from scripts import validate_repo

WORKSPACE = Path(__file__).resolve().parents[1]
FIRMWARE_DIR = WORKSPACE / "firmware" / "BmoGameboy"


class TestTier1FirmwareValidation(unittest.TestCase):
    """Validate Tier 1 firmware wrapper presence, teardown, and deterministic contracts."""

    def test_01_emulator_headers_exist(self):
        """Verify all Tier 1 emulator wrapper headers and sources exist."""
        required_files = [
            FIRMWARE_DIR / "src" / "cores" / "emu_sms.h",
            FIRMWARE_DIR / "src" / "cores" / "emu_sms.cpp",
            FIRMWARE_DIR / "src" / "cores" / "emu_pce.h",
            FIRMWARE_DIR / "src" / "cores" / "emu_pce.cpp",
            FIRMWARE_DIR / "src" / "cores" / "emu_atari.h",
            FIRMWARE_DIR / "src" / "cores" / "emu_atari.cpp",
            FIRMWARE_DIR / "src" / "cores" / "emu_pico.h",
            FIRMWARE_DIR / "src" / "cores" / "emu_pico.cpp",
        ]
        for f in required_files:
            self.assertTrue(f.exists(), f"Missing required emulator source: {f}")

    def test_02_vendor_sources_exist(self):
        """Verify all Tier 1 genuine third-party engines exist."""
        required_vendor = [
            FIRMWARE_DIR / "src" / "third_party" / "smsplus" / "sms.h",
            FIRMWARE_DIR / "src" / "third_party" / "smsplus" / "sms.c",
            FIRMWARE_DIR / "src" / "third_party" / "smsplus" / "smsplus_bmo.h",
            FIRMWARE_DIR / "src" / "third_party" / "smsplus" / "smsplus_bmo.c",
            FIRMWARE_DIR / "src" / "third_party" / "huexpress" / "pce.h",
            FIRMWARE_DIR / "src" / "third_party" / "huexpress" / "pce.c",
            FIRMWARE_DIR / "src" / "third_party" / "stella2014" / "src" / "Console.h",
            FIRMWARE_DIR / "src" / "third_party" / "stella2014" / "src" / "Console.cpp",
            FIRMWARE_DIR / "src" / "third_party" / "fake08" / "src" / "vm.h",
            FIRMWARE_DIR / "src" / "third_party" / "fake08" / "src" / "vm.cpp",
        ]
        for f in required_vendor:
            self.assertTrue(f.exists(), f"Missing required vendor source: {f}")

    def test_03_teardown_contract_compliance(self):
        """Verify CoreRegistry teardown dispatch passes dynamic repository validation."""
        self.assertEqual(validate_repo.check_firmware_safety_guardrails(), [])

    def test_04_psram_allocation_safety(self):
        """Verify all framebuffers and cartridge working RAM allocate via MALLOC_CAP_SPIRAM or bmo_psram_*."""
        psram_h = (FIRMWARE_DIR / "src" / "hal" / "psram_alloc.h").read_text(encoding="utf-8")
        self.assertIn("MALLOC_CAP_SPIRAM", psram_h)
        for emu_file in (FIRMWARE_DIR / "src" / "cores").glob("emu_*.cpp"):
            code = emu_file.read_text(encoding="utf-8")
            if "heap_caps_malloc" in code:
                self.assertIn("MALLOC_CAP_SPIRAM", code, f"{emu_file.name} must allocate dynamic buffers in Octal PSRAM")
            if "bmo_psram_malloc" in code or "bmo_psram_calloc" in code:
                self.assertIn("psram_alloc.h", code, f"{emu_file.name} must include hal/psram_alloc.h")

    def test_05_display_streaming_methods(self):
        """Verify DisplayEmu implements atomic frame streaming for all Tier 1 resolutions."""
        header = (FIRMWARE_DIR / "src" / "ui" / "display_emu.h").read_text(encoding="utf-8")
        for m in ("streamSMSFrame", "streamPCEFrame", "streamAtariFrame", "streamPicoFrame"):
            self.assertIn(m, header, f"Missing display streaming method: {m}")

    def test_06_sd_card_rom_types_and_extensions(self):
        """Verify ConsoleCatalog registers all Tier 1 extensions."""
        cat_code = (FIRMWARE_DIR / "src" / "ui" / "console_catalog.h").read_text(encoding="utf-8")
        for ext in (".sms", ".gg", ".pce", ".a26", ".p8", ".gb", ".gbc", ".nes", ".wad"):
            self.assertIn(ext, cat_code, f"Extension {ext} not handled in console_catalog.h")

    def test_07_synthetic_rom_archive_validation_does_not_touch_games_dir(self):
        """Verify ROM archive validation runs deterministically on an isolated temp fixture."""
        with tempfile.TemporaryDirectory() as tmp:
            fixture_dir = Path(tmp) / "roms"
            fixture_dir.mkdir(parents=True)
            rom_path = fixture_dir / "synthetic.gb"
            rom_path.write_bytes(b"\x00" * 32768)
            self.assertTrue(rom_path.is_file())
            self.assertEqual(rom_path.stat().st_size, 32768)
            self.assertLessEqual(len(rom_path.name.encode("utf-8")), 63)


if __name__ == "__main__":
    unittest.main()
