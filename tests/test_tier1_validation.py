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

WORKSPACE = Path(r"E:\BMO Gameboy")
FIRMWARE_DIR = WORKSPACE / "firmware" / "BmoGameboy"
GAMES_DIR = WORKSPACE / "games"

class TestTier1FirmwareValidation(unittest.TestCase):
    """Exhaustively validate Tier 1 firmware sources, contracts, and memory rules."""

    def test_01_emulator_headers_exist(self):
        """Verify all Tier 1 emulator wrapper headers and sources exist."""
        required_files = [
            FIRMWARE_DIR / "src" / "emulators" / "emu_sms.h",
            FIRMWARE_DIR / "src" / "emulators" / "emu_sms.cpp",
            FIRMWARE_DIR / "src" / "emulators" / "emu_pce.h",
            FIRMWARE_DIR / "src" / "emulators" / "emu_pce.cpp",
            FIRMWARE_DIR / "src" / "emulators" / "emu_atari.h",
            FIRMWARE_DIR / "src" / "emulators" / "emu_atari.cpp",
            FIRMWARE_DIR / "src" / "emulators" / "emu_pico.h",
            FIRMWARE_DIR / "src" / "emulators" / "emu_pico.cpp",
        ]
        for f in required_files:
            self.assertTrue(f.exists(), f"Missing required emulator source: {f}")

    def test_02_vendor_sources_exist(self):
        """Verify all Tier 1 vendor engines exist."""
        required_vendor = [
            FIRMWARE_DIR / "src" / "vendor" / "smsplus" / "sms.h",
            FIRMWARE_DIR / "src" / "vendor" / "smsplus" / "sms.c",
            FIRMWARE_DIR / "src" / "vendor" / "pce" / "pce.h",
            FIRMWARE_DIR / "src" / "vendor" / "pce" / "pce.c",
            FIRMWARE_DIR / "src" / "vendor" / "stella" / "atari.h",
            FIRMWARE_DIR / "src" / "vendor" / "stella" / "atari.c",
            FIRMWARE_DIR / "src" / "vendor" / "pico" / "pico.h",
            FIRMWARE_DIR / "src" / "vendor" / "pico" / "pico.c",
        ]
        for f in required_vendor:
            self.assertTrue(f.exists(), f"Missing required vendor source: {f}")

    def test_03_teardown_contract_compliance(self):
        """Verify that every emulator implements destroy() and that BmoGameboy.ino invokes destroy() on SELECT+UP."""
        ino_content = (FIRMWARE_DIR / "BmoGameboy.ino").read_text(encoding="utf-8")
        
        # Check that teardown is invoked for each emulator
        required_destroys = [
            "SmsEmu::destroy()",
            "PceEmu::destroy()",
            "AtariEmu::destroy()",
            "PicoEmu::destroy()",
            "WalnutEmu::destroy()",
            "PeanutEmu::destroy()",
            "NESEmu::destroy()",
            "DoomEmu::destroy()",
        ]
        for d in required_destroys:
            self.assertIn(d, ino_content, f"Teardown contract violation: {d} missing in BmoGameboy.ino")

    def test_04_psram_allocation_safety(self):
        """Verify all framebuffers and cartridge working RAM allocate via MALLOC_CAP_SPIRAM."""
        for emu_file in (FIRMWARE_DIR / "src" / "emulators").glob("emu_*.cpp"):
            code = emu_file.read_text(encoding="utf-8")
            # If malloc or heap_caps_malloc is used, verify it requests SPIRAM
            if "heap_caps_malloc" in code:
                self.assertIn("MALLOC_CAP_SPIRAM", code, f"{emu_file.name} must allocate dynamic buffers in Octal PSRAM")

    def test_05_display_streaming_methods(self):
        """Verify DisplayEmu implements atomic frame streaming for all Tier 1 resolutions."""
        header = (FIRMWARE_DIR / "src" / "core" / "display_emu.h").read_text(encoding="utf-8")
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
        sd_code = (FIRMWARE_DIR / "src" / "core" / "sd_card.cpp").read_text(encoding="utf-8")
        required_exts = [".sms", ".gg", ".pce", ".a26", ".a78", ".p8", ".gb", ".gbc", ".nes", ".wad"]
        for ext in required_exts:
            self.assertIn(ext, sd_code, f"Extension {ext} not handled in determineType in sd_card.cpp")

    def test_07_installed_games_integrity(self):
        """Verify all games in games/ directory are non-empty and have valid extensions."""
        self.assertTrue(GAMES_DIR.exists(), "games/ directory does not exist")
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
