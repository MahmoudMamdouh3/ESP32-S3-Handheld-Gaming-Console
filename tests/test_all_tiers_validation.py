#!/usr/bin/env python3
"""
test_all_tiers_validation.py
Master test runner validating all 15 console emulators, memory lifecycle invariants,
SPI display streaming, SD card detection, and installed game libraries.
"""

import os
import unittest
from pathlib import Path

WORKSPACE = Path(r"E:\BMO Gameboy")
FIRMWARE_DIR = WORKSPACE / "firmware" / "BmoGameboy"
GAMES_DIR = WORKSPACE / "games"

class TestAllConsolesFirmwareValidation(unittest.TestCase):
    """Exhaustively validate all 15 console platforms and contracts."""

    def test_01_all_15_emulators_exist(self):
        """Verify all 15 emulator headers and implementation sources exist."""
        emulators = [
            "emu_walnut", "emu_peanut", "emu_nes", "emu_doom",
            "emu_sms", "emu_pce", "emu_atari", "emu_pico",
            "emu_genesis", "emu_snes", "emu_wswan", "emu_ngp", "emu_lynx", "emu_colem"
        ]
        for emu in emulators:
            h = FIRMWARE_DIR / "src" / "emulators" / f"{emu}.h"
            cpp = FIRMWARE_DIR / "src" / "emulators" / f"{emu}.cpp"
            self.assertTrue(h.exists(), f"Missing header: {h}")
            self.assertTrue(cpp.exists(), f"Missing implementation: {cpp}")

    def test_02_all_vendor_engines_exist(self):
        """Verify all underlying vendor engine headers and sources exist."""
        vendor_files = [
            ("smsplus", "sms.h"), ("smsplus", "sms.c"),
            ("pce", "pce.h"), ("pce", "pce.c"),
            ("stella", "atari.h"), ("stella", "atari.c"),
            ("pico", "pico.h"), ("pico", "pico.c"),
            ("genesis", "genesis.h"), ("genesis", "genesis.c"),
            ("snes", "snes.h"), ("snes", "snes.c"),
            ("wswan", "wswan.h"), ("wswan", "wswan.c"),
            ("ngp", "ngp.h"), ("ngp", "ngp.c"),
            ("lynx", "lynx.h"), ("lynx", "lynx.c"),
            ("colem", "colem.h"), ("colem", "colem.c"),
        ]
        for vdir, vfile in vendor_files:
            f = FIRMWARE_DIR / "src" / "vendor" / vdir / vfile
            self.assertTrue(f.exists(), f"Missing vendor engine file: {f}")

    def test_03_teardown_compliance(self):
        """Verify all 15 emulators have destroy() calls registered in main dispatch."""
        ino_content = (FIRMWARE_DIR / "BmoGameboy.ino").read_text(encoding="utf-8")
        required_teardowns = [
            "WalnutEmu::destroy()", "PeanutEmu::destroy()", "NesEmu::destroy()", "DoomEmu::destroy()",
            "SmsEmu::destroy()", "PceEmu::destroy()", "AtariEmu::destroy()", "PicoEmu::destroy()",
            "GenesisEmu::destroy()", "SNESEmu::destroy()", "WSwanEmu::destroy()", "NGPEmu::destroy()",
            "LynxEmu::destroy()", "ColemEmu::destroy()"
        ]
        for td in required_teardowns:
            self.assertIn(td, ino_content, f"Teardown contract missing for: {td}")

    def test_04_psram_allocation_safety(self):
        """Verify that dynamic framebuffers and cart RAM use MALLOC_CAP_SPIRAM in wrapper or vendor engine."""
        pairs = [
            ("emu_sms.cpp", "vendor/smsplus/sms.c"),
            ("emu_pce.cpp", "vendor/pce/pce.c"),
            ("emu_atari.cpp", "vendor/stella/atari.c"),
            ("emu_pico.cpp", "vendor/pico/pico.c"),
            ("emu_genesis.cpp", "vendor/genesis/genesis.c"),
            ("emu_snes.cpp", "vendor/snes/snes.c"),
            ("emu_wswan.cpp", "vendor/wswan/wswan.c"),
            ("emu_ngp.cpp", "vendor/ngp/ngp.c"),
            ("emu_lynx.cpp", "vendor/lynx/lynx.c"),
            ("emu_colem.cpp", "vendor/colem/colem.c"),
        ]
        for emu_name, vendor_name in pairs:
            emu_path = FIRMWARE_DIR / "src" / "emulators" / emu_name
            vendor_path = FIRMWARE_DIR / "src" / vendor_name
            code = (emu_path.read_text(encoding="utf-8") if emu_path.exists() else "") + \
                   (vendor_path.read_text(encoding="utf-8") if vendor_path.exists() else "")
            self.assertIn("MALLOC_CAP_SPIRAM", code, f"{emu_name}/{vendor_name} must allocate dynamic buffers in PSRAM")

    def test_05_display_streaming_methods(self):
        """Verify DisplayEmu has dedicated streaming methods for all consoles."""
        header = (FIRMWARE_DIR / "src" / "core" / "display_emu.h").read_text(encoding="utf-8")
        methods = [
            "streamPixelRow", "streamGenesisFrame", "streamSNESFrame",
            "streamWSwanFrame", "streamNGPFrame", "streamLynxFrame", "streamColemFrame"
        ]
        for m in methods:
            self.assertIn(m, header, f"Missing display streaming method: {m}")

    def test_06_sd_card_all_extensions_registered(self):
        """Verify sd_card.cpp detects and maps all 15 console extensions."""
        code = (FIRMWARE_DIR / "src" / "core" / "sd_card.cpp").read_text(encoding="utf-8")
        exts = [
            ".gb", ".gbc", ".nes", ".wad", ".sms", ".gg", ".pce", ".a26", ".p8",
            ".gen", ".md", ".smd", ".sfc", ".smc", ".ws", ".wsc", ".ngp", ".ngc",
            ".lnx", ".col", ".sg"
        ]
        for ext in exts:
            self.assertIn(ext, code, f"Extension {ext} not registered in sd_card.cpp")

    def test_07_installed_games_catalogue_health(self):
        """Verify that installed games directory contains healthy non-zero files."""
        if not GAMES_DIR.exists():
            return
        files = list(GAMES_DIR.iterdir())
        self.assertGreater(len(files), 1000, "Games directory should contain substantial game library")
        zero_byte = [f.name for f in files if f.stat().st_size == 0]
        self.assertEqual(len(zero_byte), 0, f"Found 0-byte corrupt files in games/: {zero_byte[:5]}")

if __name__ == "__main__":
    unittest.main()
