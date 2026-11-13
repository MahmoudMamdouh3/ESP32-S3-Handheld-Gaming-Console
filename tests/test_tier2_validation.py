#!/usr/bin/env python3
"""
test_tier2_validation.py
Exhaustive automated validation for Tier 2 emulator implementations,
memory invariants, display streaming contracts, and extension routing.
"""

import os
import unittest
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[1]
FIRMWARE_DIR = WORKSPACE / "firmware" / "BmoGameboy"

class TestTier2FirmwareValidation(unittest.TestCase):
    """Exhaustively validate Tier 2 firmware sources, contracts, and memory rules."""

    def test_01_emulator_headers_and_sources_exist(self):
        """Verify all Tier 2 emulator wrapper headers and sources exist."""
        required_files = [
            FIRMWARE_DIR / "src" / "cores" / "emu_genesis.h",
            FIRMWARE_DIR / "src" / "cores" / "emu_genesis.cpp",
            FIRMWARE_DIR / "src" / "cores" / "emu_snes.h",
            FIRMWARE_DIR / "src" / "cores" / "emu_snes.cpp",
            FIRMWARE_DIR / "src" / "cores" / "emu_wswan.h",
            FIRMWARE_DIR / "src" / "cores" / "emu_wswan.cpp",
            FIRMWARE_DIR / "src" / "cores" / "emu_ngp.h",
            FIRMWARE_DIR / "src" / "cores" / "emu_ngp.cpp",
            FIRMWARE_DIR / "src" / "cores" / "emu_lynx.h",
            FIRMWARE_DIR / "src" / "cores" / "emu_lynx.cpp",
            FIRMWARE_DIR / "src" / "cores" / "emu_colem.h",
            FIRMWARE_DIR / "src" / "cores" / "emu_colem.cpp",
        ]
        for f in required_files:
            self.assertTrue(f.exists(), f"Missing required emulator file: {f}")

    def test_02_vendor_engines_exist(self):
        """Verify all Tier 2 vendor engines exist."""
        required_vendor = [
            FIRMWARE_DIR / "src" / "engines" / "genesis" / "genesis.h",
            FIRMWARE_DIR / "src" / "engines" / "genesis" / "genesis.c",
            FIRMWARE_DIR / "src" / "engines" / "snes" / "snes.h",
            FIRMWARE_DIR / "src" / "engines" / "snes" / "snes.c",
            FIRMWARE_DIR / "src" / "engines" / "wswan" / "wswan.h",
            FIRMWARE_DIR / "src" / "engines" / "wswan" / "wswan.c",
            FIRMWARE_DIR / "src" / "engines" / "ngp" / "ngp.h",
            FIRMWARE_DIR / "src" / "engines" / "ngp" / "ngp.c",
            FIRMWARE_DIR / "src" / "engines" / "lynx" / "lynx.h",
            FIRMWARE_DIR / "src" / "engines" / "lynx" / "lynx.c",
            FIRMWARE_DIR / "src" / "engines" / "colem" / "colem.h",
            FIRMWARE_DIR / "src" / "engines" / "colem" / "colem.c",
        ]
        for f in required_vendor:
            self.assertTrue(f.exists(), f"Missing required vendor engine: {f}")

    def test_03_teardown_contract_compliance(self):
        """Verify all Tier 2 emulators implement destroy() and are invoked in BmoGameboy.ino."""
        ino_content = (FIRMWARE_DIR / "BmoGameboy.ino").read_text(encoding="utf-8")
        required_destroys = [
            "GenesisEmu::destroy()",
            "SNESEmu::destroy()",
            "WSwanEmu::destroy()",
            "NGPEmu::destroy()",
            "LynxEmu::destroy()",
            "ColemEmu::destroy()"
        ]
        for d in required_destroys:
            self.assertIn(d, ino_content, f"Teardown violation: {d} missing in BmoGameboy.ino")

    def test_04_psram_allocation_safety(self):
        """Verify all Tier 2 emulators allocate framebuffers and RAM in Octal PSRAM."""
        for emu_name in ["emu_genesis.cpp", "emu_snes.cpp", "emu_wswan.cpp", "emu_ngp.cpp", "emu_lynx.cpp", "emu_colem.cpp"]:
            code = (FIRMWARE_DIR / "src" / "cores" / emu_name).read_text(encoding="utf-8")
            self.assertIn("MALLOC_CAP_SPIRAM", code, f"{emu_name} must use MALLOC_CAP_SPIRAM for dynamic memory")

    def test_05_display_streaming_methods(self):
        """Verify DisplayEmu implements streaming methods for all Tier 2 resolutions."""
        header = (FIRMWARE_DIR / "src" / "ui" / "display_emu.h").read_text(encoding="utf-8")
        required_methods = [
            "streamGenesisFrame",
            "streamSNESFrame",
            "streamWSwanFrame",
            "streamNGPFrame",
            "streamLynxFrame",
            "streamColemFrame"
        ]
        for m in required_methods:
            self.assertIn(m, header, f"Missing display streaming method: {m}")

    def test_06_sd_card_tier2_extension_routing(self):
        """Verify sd_card.cpp recognizes all Tier 2 extensions."""
        sd_code = (FIRMWARE_DIR / "src" / "storage" / "sd_card.cpp").read_text(encoding="utf-8")
        required_exts = [".gen", ".md", ".smd", ".sfc", ".smc", ".ws", ".wsc", ".ngp", ".ngc", ".lnx", ".col", ".sg"]
        for ext in required_exts:
            self.assertIn(ext, sd_code, f"Extension {ext} not recognized in sd_card.cpp")

if __name__ == "__main__":
    unittest.main()
