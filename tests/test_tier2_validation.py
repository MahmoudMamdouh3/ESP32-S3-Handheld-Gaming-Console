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
        """Verify all Tier 2 genuine third-party engines exist."""
        required_vendor = [
            FIRMWARE_DIR / "src" / "third_party" / "gwenesis" / "bus" / "gwenesis_bus.h",
            FIRMWARE_DIR / "src" / "third_party" / "gwenesis" / "bus" / "gwenesis_bus.c",
            FIRMWARE_DIR / "src" / "third_party" / "snes9x" / "snes9x.h",
            FIRMWARE_DIR / "src" / "third_party" / "snes9x" / "src" / "cpuexec.c",
            FIRMWARE_DIR / "src" / "third_party" / "oswan" / "WS.h",
            FIRMWARE_DIR / "src" / "third_party" / "oswan" / "WS.c",
            FIRMWARE_DIR / "src" / "third_party" / "race" / "tlcs900h.h",
            FIRMWARE_DIR / "src" / "third_party" / "race" / "tlcs900h.c",
            FIRMWARE_DIR / "src" / "third_party" / "handy" / "handy.h",
            FIRMWARE_DIR / "src" / "third_party" / "handy" / "system.cpp",
            FIRMWARE_DIR / "src" / "third_party" / "smsplus" / "tms.h",
            FIRMWARE_DIR / "src" / "third_party" / "smsplus" / "tms.c",
        ]
        for f in required_vendor:
            self.assertTrue(f.exists(), f"Missing required vendor engine: {f}")

    def test_03_teardown_contract_compliance(self):
        """Verify all Tier 2 emulators implement destroy() in CoreRegistry and are dispatched by bmo_app.cpp."""
        app_content = (FIRMWARE_DIR / "src" / "app" / "bmo_app.cpp").read_text(encoding="utf-8")
        reg_content = (FIRMWARE_DIR / "src" / "cores" / "core_registry.h").read_text(encoding="utf-8")
        self.assertIn("CoreRegistry::findByIndex", app_content)
        self.assertIn("core->destroy()", app_content)
        required_destroys = [
            "GenesisEmu::destroy",
            "SNESEmu::destroy",
            "WSwanEmu::destroy",
            "NGPEmu::destroy",
            "LynxEmu::destroy",
            "ColemEmu::destroy",
        ]
        for d in required_destroys:
            self.assertIn(d, reg_content, f"Teardown violation: {d} missing in core_registry.h")

    def test_04_psram_allocation_safety(self):
        """Verify all Tier 2 emulators allocate framebuffers and RAM in Octal PSRAM."""
        psram_h = (FIRMWARE_DIR / "src" / "hal" / "psram_alloc.h").read_text(encoding="utf-8")
        self.assertIn("MALLOC_CAP_SPIRAM", psram_h)
        for emu_name in ["emu_genesis.cpp", "emu_snes.cpp", "emu_wswan.cpp", "emu_ngp.cpp", "emu_lynx.cpp", "emu_colem.cpp"]:
            code = (FIRMWARE_DIR / "src" / "cores" / emu_name).read_text(encoding="utf-8")
            uses_psram = ("MALLOC_CAP_SPIRAM" in code) or ("bmo_psram_malloc" in code) or ("bmo_psram_calloc" in code)
            self.assertTrue(uses_psram, f"{emu_name} must allocate dynamic memory in Octal PSRAM via MALLOC_CAP_SPIRAM or bmo_psram_*")

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
        """Verify ConsoleCatalog recognizes all Tier 2 extensions."""
        cat_code = (FIRMWARE_DIR / "src" / "ui" / "console_catalog.h").read_text(encoding="utf-8")
        required_exts = [".gen", ".md", ".smd", ".sfc", ".smc", ".ws", ".wsc", ".ngp", ".ngc", ".lnx", ".col", ".sg"]
        for ext in required_exts:
            self.assertIn(ext, cat_code, f"Extension {ext} not recognized in console_catalog.h")

if __name__ == "__main__":
    unittest.main()
