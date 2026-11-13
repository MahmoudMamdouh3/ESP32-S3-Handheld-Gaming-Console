#!/usr/bin/env python3
"""
test_tier1_validation.py
Exhaustive automated validation for Tier 1 emulator implementations,
memory invariants, display streaming contracts, and installed ROM integrity.
"""

import os
import re
import sys
import unittest
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[1]
FIRMWARE_DIR = WORKSPACE / "firmware" / "BmoGameboy"
GAMES_DIR = WORKSPACE / "games"

class TestTier1FirmwareValidation(unittest.TestCase):
    """Exhaustively validate Tier 1 firmware sources, contracts, and memory rules."""

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
        """Verify that every Tier 1 emulator destroy() is registered in CoreRegistry and dispatched by BmoGameboy.ino."""
        ino_content = (FIRMWARE_DIR / "BmoGameboy.ino").read_text(encoding="utf-8")
        reg_content = (FIRMWARE_DIR / "src" / "cores" / "core_registry.h").read_text(encoding="utf-8")
        self.assertIn("CoreRegistry::findByIndex", ino_content)
        self.assertIn("core->destroy()", ino_content)

        required_destroys = [
            "SmsEmu::destroy",
            "PceEmu::destroy",
            "AtariEmu::destroy",
            "PicoEmu::destroy",
            "WalnutEmu::destroy",
            "PeanutEmu::destroy",
            "NesEmu::destroy",
            "DoomEmu::destroy",
        ]
        for d in required_destroys:
            self.assertIn(d, reg_content, f"Teardown contract violation: {d} missing in core_registry.h")

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
        required_methods = [
            "streamSMSFrame",
            "streamPCEFrame",
            "streamAtariFrame",
            "streamPicoFrame",
        ]
        for m in required_methods:
            self.assertIn(m, header, f"Missing display streaming method: {m}")

    def test_06_sd_card_rom_types_and_extensions(self):
        """Verify sd_card.cpp recognizes all Tier 1 extensions."""
        sd_code = (FIRMWARE_DIR / "src" / "storage" / "sd_card.cpp").read_text(encoding="utf-8")
        required_exts = [".sms", ".gg", ".pce", ".a26", ".p8", ".gb", ".gbc", ".nes", ".wad"]
        for ext in required_exts:
            self.assertIn(ext, sd_code, f"Extension {ext} not handled in determineType in sd_card.cpp")

    def test_07_installed_games_integrity(self):
        """Verify all games in games/ directory are non-empty and have valid extensions."""
        if not GAMES_DIR.is_dir():
            self.skipTest("games/ is local-only and is not included in clean checkouts")
        files = list(GAMES_DIR.iterdir())
        self.assertGreater(len(files), 10000, f"Expected > 10,000 games installed, found {len(files)}")
        
        valid_exts = {
            ".gb", ".gbc", ".nes", ".wad", ".sms", ".gg", ".pce", ".a26", ".p8",
            ".col", ".sfc", ".smc", ".gen", ".md", ".smd", ".ws", ".wsc", ".ngp", ".ngc", ".lnx", ".sg"
        }
        empty_files = []
        invalid_exts = []
        long_names = []

        for f in files:
            if not f.is_file():
                continue
            ext = f.suffix.lower()
            if ext not in valid_exts:
                invalid_exts.append(f.name)
            if f.stat().st_size == 0:
                empty_files.append(f.name)
            if len(f.name) > 63:
                long_names.append(f.name)

        self.assertEqual(len(empty_files), 0, f"Found 0-byte corrupt files: {empty_files[:5]}")
        self.assertEqual(len(invalid_exts), 0, f"Found files with invalid extensions: {invalid_exts[:5]}")
        self.assertEqual(len(long_names), 0, f"Found filenames exceeding 63 bytes buffer limit: {long_names[:5]}")

if __name__ == "__main__":
    unittest.main()
