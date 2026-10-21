#!/usr/bin/env python3
"""
test_new_cores_and_qol.py
Comprehensive validation suite for:
  1. Atari 2600 (MOS 6507 + TIA + RIOT 6532) C engine & banking/video invariants
  2. ColecoVision / SG-1000 (Z80A + TMS9918A VDP) C engine & Mode 1/2/sprite invariants
  3. Atari Lynx (WDC 65SC02 + Mikey/Suzy SCB blitter & hardware multiply) C engine
  4. SNK Neo Geo Pocket Color (TLCS-900/H + K1GE/K2GE dual planes & sprites) C engine
  5. Super Nintendo / SFC (WDC 65C816 + S-PPU BG Modes 0/1, OAM, & 8-channel DMA) C engine
  6. PICO-8 Fantasy Console (32KB RAM map, graphics primitives, .p8 parser, bytecode runner)
  7. Unified 14-Core Persistence Contract & BmoGameboy.ino dispatch wiring
  8. Quality of Life (QoL) features: Buttons A/B Swap & Turbo, SDCard Recent History,
     SaveManager BMP Screenshot Dump, and Konami Code Easter Egg.
"""

import unittest
from pathlib import Path

WORKSPACE    = Path(__file__).resolve().parents[1]
FIRMWARE_DIR = WORKSPACE / "firmware" / "BmoGameboy"
SRC_DIR      = FIRMWARE_DIR / "src"


class TestAtari2600CoreEngine(unittest.TestCase):
    """Verify Atari 2600 (stella/atari.c & atari.h) MOS 6507 + TIA + RIOT implementation."""

    def setUp(self):
        self.h_text = (SRC_DIR / "vendor" / "stella" / "atari.h").read_text(encoding="utf-8")
        self.c_text = (SRC_DIR / "vendor" / "stella" / "atari.c").read_text(encoding="utf-8")

    def test_01_no_stub_sentinel_and_psram_alloc(self):
        self.assertNotIn("STUB_ENGINE", self.c_text)
        self.assertIn("MALLOC_CAP_SPIRAM", self.c_text)

    def test_02_6507_cpu_and_address_mirroring(self):
        # 13-bit address bus ($0000-$1FFF)
        self.assertIn("addr & 0x1FFF", self.c_text)
        # Key 6507 opcodes and helpers
        for fn in ("atari_adc", "atari_sbc", "atari_cmp", "atari_cpu_step"):
            self.assertIn(fn, self.c_text)

    def test_03_tia_playfield_sprites_and_wsync(self):
        self.assertIn("wsync_halt", self.c_text)
        self.assertIn("atari_get_playfield_bit", self.c_text)
        self.assertIn("atari_render_scanline", self.c_text)
        self.assertIn("atari_hm_delta", self.c_text)

    def test_04_riot_6532_timer_and_bankswitching(self):
        self.assertIn("atari_step_riot", self.c_text)
        self.assertIn("atari_check_bankswitch", self.c_text)
        self.assertIn("0x1FF8", self.c_text)  # F8 bankswitch hotspot


class TestColecoVisionCoreEngine(unittest.TestCase):
    """Verify ColecoVision / SG-1000 (colem/colem.c & colem.h) Z80A + TMS9918A VDP engine."""

    def setUp(self):
        self.h_text = (SRC_DIR / "vendor" / "colem" / "colem.h").read_text(encoding="utf-8")
        self.c_text = (SRC_DIR / "vendor" / "colem" / "colem.c").read_text(encoding="utf-8")

    def test_01_tms9918a_vdp_and_palette(self):
        self.assertNotIn("STUB_ENGINE", self.c_text)
        self.assertIn("TMS9918_RGB", self.c_text)
        self.assertIn("colem_render_scanline", self.c_text)
        self.assertIn("vdp_regs", self.h_text)

    def test_02_z80_io_ports_and_nmi(self):
        self.assertIn("colem_io_read", self.c_text)
        self.assertIn("colem_io_write", self.c_text)
        self.assertIn("keypad_mode", self.c_text)
        self.assertIn("0x0066", self.c_text)  # Z80 NMI vector


class TestAtariLynxCoreEngine(unittest.TestCase):
    """Verify Atari Lynx (lynx/lynx.c & lynx.h) WDC 65SC02 + Mikey & Suzy engine."""

    def setUp(self):
        self.h_text = (SRC_DIR / "vendor" / "lynx" / "lynx.h").read_text(encoding="utf-8")
        self.c_text = (SRC_DIR / "vendor" / "lynx" / "lynx.c").read_text(encoding="utf-8")

    def test_01_mikey_rgb444_palette_and_suzy_scb_blitter(self):
        self.assertNotIn("STUB_ENGINE", self.c_text)
        self.assertIn("lynx_rgb444_to_bgr565", self.c_text)
        self.assertIn("lynx_execute_scb", self.c_text)
        self.assertIn("palette_cache", self.h_text)

    def test_02_suzy_hardware_multiply_and_65sc02_cmos_opcodes(self):
        # Suzy hardware 16x16 multiply at $FC52-$FC63
        self.assertIn("reg == 0x52", self.c_text)
        self.assertIn("lynx_cpu_step", self.c_text)
        # 65SC02 BRA (0x80) & STZ (0x64, 0x9C)
        self.assertIn("case 0x80:", self.c_text)
        self.assertIn("case 0x64:", self.c_text)


class TestNeoGeoPocketCoreEngine(unittest.TestCase):
    """Verify SNK Neo Geo Pocket Color (ngp/ngp.c & ngp.h) TLCS-900/H + K1GE/K2GE engine."""

    def setUp(self):
        self.h_text = (SRC_DIR / "vendor" / "ngp" / "ngp.h").read_text(encoding="utf-8")
        self.c_text = (SRC_DIR / "vendor" / "ngp" / "ngp.c").read_text(encoding="utf-8")

    def test_01_tlcs900h_cpu_and_24bit_bus(self):
        self.assertNotIn("STUB_ENGINE", self.c_text)
        self.assertIn("ngp_cpu_step", self.c_text)
        self.assertIn("0x200000", self.c_text)  # Cartridge ROM base
        self.assertIn("0x006F82", self.c_text)  # Joypad register

    def test_02_k2ge_dual_planes_sprites_and_palette(self):
        self.assertIn("ngp_render_plane", self.c_text)
        self.assertIn("ngp_render_scanline", self.c_text)
        self.assertIn("ngp_rgb444_to_bgr565", self.c_text)
        self.assertIn("0x008200", self.c_text)  # K2GE Color Palette RAM


class TestSNESCoreEngine(unittest.TestCase):
    """Verify Super Nintendo (snes/snes.c & snes.h) WDC 65C816 + S-PPU + DMA engine."""

    def setUp(self):
        self.h_text = (SRC_DIR / "vendor" / "snes" / "snes.h").read_text(encoding="utf-8")
        self.c_text = (SRC_DIR / "vendor" / "snes" / "snes.c").read_text(encoding="utf-8")

    def test_01_65c816_cpu_modes_and_dma(self):
        self.assertNotIn("STUB_ENGINE", self.c_text)
        self.assertIn("snes_cpu_step", self.c_text)
        self.assertIn("snes_run_dma", self.c_text)
        self.assertIn("case 0xFB:", self.c_text)  # XCE
        self.assertIn("case 0xC2:", self.c_text)  # REP
        self.assertIn("case 0xE2:", self.c_text)  # SEP

    def test_02_sppu_cgram_bglayers_and_oam(self):
        self.assertIn("snes_bgr555_to_bgr565_swapped", self.c_text)
        self.assertIn("snes_render_bg_layer", self.c_text)
        self.assertIn("snes_render_scanline", self.c_text)
        self.assertIn("snes_destroy", self.c_text)


class TestPico8CoreEngine(unittest.TestCase):
    """Verify PICO-8 Fantasy Console (pico/pico.c & pico.h) virtual hardware & graphics API."""

    def setUp(self):
        self.h_text = (SRC_DIR / "vendor" / "pico" / "pico.h").read_text(encoding="utf-8")
        self.c_text = (SRC_DIR / "vendor" / "pico" / "pico.c").read_text(encoding="utf-8")

    def test_01_pico8_graphics_primitives_and_memory_map(self):
        self.assertNotIn("STUB_ENGINE", self.c_text)
        for prim in ("pico_cls", "pico_pset", "pico_pget", "pico_rectfill", "pico_circfill", "pico_spr", "pico_map"):
            self.assertIn(prim, self.h_text)
            self.assertIn(prim, self.c_text)
        # Draw state ($5F00), Screen palette ($5F10), Screen buffer ($6000)
        self.assertIn("0x5F00", self.c_text)
        self.assertIn("0x5F10", self.c_text)
        self.assertIn("0x6000", self.c_text)


class TestQualityOfLifeAndUnifiedPersistence(unittest.TestCase):
    """Verify Phase B & Phase C QoL features across BmoGameboy.ino and core modules."""

    def test_01_buttons_remap_and_turbo_api(self):
        btn_h = (SRC_DIR / "core" / "buttons.h").read_text(encoding="utf-8")
        btn_cpp = (SRC_DIR / "core" / "buttons.cpp").read_text(encoding="utf-8")
        for sym in ("setGameplayMode", "setSwapAB", "isSwapAB", "toggleSwapAB", "setTurboEnabled", "isTurboEnabled", "toggleTurbo"):
            self.assertIn(sym, btn_h)
            self.assertIn(sym, btn_cpp)

    def test_02_sdcard_recent_history_api(self):
        sd_h = (SRC_DIR / "core" / "sd_card.h").read_text(encoding="utf-8")
        sd_cpp = (SRC_DIR / "core" / "sd_card.cpp").read_text(encoding="utf-8")
        for sym in ("recordRecentLaunch", "getRecentCount", "getRecentFilename", "loadRecentHistory", "saveRecentHistory"):
            self.assertIn(sym, sd_h)
            self.assertIn(sym, sd_cpp)
        self.assertIn("/history.txt", sd_cpp)

    def test_03_save_manager_screenshot_bmp_dump(self):
        sm_h = (SRC_DIR / "core" / "save_manager.h").read_text(encoding="utf-8")
        sm_cpp = (SRC_DIR / "core" / "save_manager.cpp").read_text(encoding="utf-8")
        self.assertIn("saveScreenshotBmp", sm_h)
        self.assertIn("saveScreenshotBmp", sm_cpp)
        self.assertIn("/screenshots", sm_cpp)

    def test_04_ino_wires_all_persistence_and_konami_code(self):
        ino = (FIRMWARE_DIR / "BmoGameboy.ino").read_text(encoding="utf-8")
        for cls in ("WalnutEmu", "PeanutEmu", "NesEmu", "SmsEmu", "PceEmu", "AtariEmu",
                    "PicoEmu", "GenesisEmu", "SNESEmu", "WSwanEmu", "NGPEmu", "LynxEmu", "ColemEmu"):
            self.assertIn(f"{cls}::saveBatteryRam", ino)
            self.assertIn(f"{cls}::loadBatteryRam", ino)
            self.assertIn(f"{cls}::saveState", ino)
            self.assertIn(f"{cls}::loadState", ino)
        self.assertIn("kKonamiSeq", ino)
        self.assertIn("SDCard::recordRecentLaunch", ino)


if __name__ == "__main__":
    unittest.main()
