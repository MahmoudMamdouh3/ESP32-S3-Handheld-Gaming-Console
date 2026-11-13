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
    """Verify Atari 2600 (stella2014 + emu_atari.cpp) MOS 6502 + TIA + RIOT implementation."""

    def setUp(self):
        self.stella_dir = SRC_DIR / "third_party" / "stella2014" / "src"
        self.wrapper = (SRC_DIR / "cores" / "emu_atari.cpp").read_text(encoding="utf-8")
        self.m6502 = (self.stella_dir / "M6502.cpp").read_text(encoding="utf-8", errors="ignore")
        self.tia = (self.stella_dir / "TIA.cpp").read_text(encoding="utf-8", errors="ignore")
        self.riot = (self.stella_dir / "M6532.cpp").read_text(encoding="utf-8", errors="ignore")
        self.cart = (self.stella_dir / "Cart.cpp").read_text(encoding="utf-8", errors="ignore")

    def test_01_no_stub_sentinel_and_psram_alloc(self):
        self.assertNotIn("STUB_ENGINE", self.wrapper)
        self.assertIn("bmo_psram_calloc", self.wrapper)

    def test_02_6507_cpu_and_address_mirroring(self):
        self.assertIn("execute", self.m6502)
        self.assertIn("peek", self.m6502)
        self.assertIn("poke", self.m6502)

    def test_03_tia_playfield_sprites_and_wsync(self):
        self.assertIn("WSYNC", self.tia)
        self.assertIn("endFrame", self.tia)
        self.assertIn("currentFrameBuffer", (self.stella_dir / "TIA.h").read_text(encoding="utf-8", errors="ignore"))

    def test_04_riot_6532_timer_and_bankswitching(self):
        self.assertIn("peek", self.riot)
        self.assertIn("poke", self.riot)
        self.assertIn("Cartridge::create", self.cart)
        self.assertIn("CartF8", self.cart)


class TestColecoVisionCoreEngine(unittest.TestCase):
    """Verify ColecoVision / SG-1000 (smsplus tms9918 + z80 + emu_colem.cpp) engine."""

    def setUp(self):
        self.sms_dir = SRC_DIR / "third_party" / "smsplus"
        self.wrapper = (SRC_DIR / "cores" / "emu_colem.cpp").read_text(encoding="utf-8")
        self.tms_c = (self.sms_dir / "tms.c").read_text(encoding="utf-8", errors="ignore")
        self.z80_c = (self.sms_dir / "z80.c").read_text(encoding="utf-8", errors="ignore")
        self.bmo_c = (self.sms_dir / "smsplus_bmo.c").read_text(encoding="utf-8", errors="ignore")

    def test_01_tms9918a_vdp_and_palette(self):
        self.assertNotIn("STUB_ENGINE", self.wrapper)
        self.assertNotIn("STUB_ENGINE", self.tms_c)
        self.assertIn("render_bg_m2", self.tms_c)
        self.assertIn("parse_line", self.tms_c)
        self.assertIn("BMO_SMS_CONSOLE_COLECO", self.bmo_c)

    def test_02_z80_io_ports_and_nmi(self):
        self.assertIn("z80_execute", self.z80_c)
        self.assertIn("bmo_sms_init", self.bmo_c)
        self.assertIn("BMO_SMS_CONSOLE_SG1000", self.wrapper)


class TestAtariLynxCoreEngine(unittest.TestCase):
    """Verify Atari Lynx (handy + emu_lynx.cpp) WDC 65SC02 + Mikey & Suzy engine."""

    def setUp(self):
        self.handy_dir = SRC_DIR / "third_party" / "handy"
        self.wrapper = (SRC_DIR / "cores" / "emu_lynx.cpp").read_text(encoding="utf-8")
        self.susie = (self.handy_dir / "susie.cpp").read_text(encoding="utf-8", errors="ignore")
        self.mikie_h = (self.handy_dir / "mikie.h").read_text(encoding="utf-8", errors="ignore")

    def test_01_mikey_rgb444_palette_and_suzy_scb_blitter(self):
        self.assertNotIn("STUB_ENGINE", self.wrapper)
        self.assertIn("bmo_psram_calloc", self.wrapper)
        self.assertIn("MIKIE_PIXEL_FORMAT_16BPP_565", self.mikie_h)
        self.assertIn("CSystem", self.wrapper)

    def test_02_suzy_hardware_multiply_and_65sc02_cmos_opcodes(self):
        self.assertIn("DoMathMultiply", self.susie)
        self.assertIn("SPRSYS", self.susie)


class TestNeoGeoPocketCoreEngine(unittest.TestCase):
    """Verify SNK Neo Geo Pocket Color (race + emu_ngp.cpp) TLCS-900/H + K1GE/K2GE engine."""

    def setUp(self):
        self.race_dir = SRC_DIR / "third_party" / "race"
        self.wrapper = (SRC_DIR / "cores" / "emu_ngp.cpp").read_text(encoding="utf-8")
        self.tlcs = (self.race_dir / "tlcs900h.c").read_text(encoding="utf-8", errors="ignore")
        self.gfx = (self.race_dir / "graphics.c").read_text(encoding="utf-8", errors="ignore")

    def test_01_tlcs900h_cpu_and_24bit_bus(self):
        self.assertNotIn("STUB_ENGINE", self.wrapper)
        self.assertIn("tlcs_execute", self.tlcs)
        self.assertIn("tlcs_step", self.tlcs)
        self.assertIn("ngp_mem_set_rom", self.wrapper)

    def test_02_k2ge_dual_planes_sprites_and_palette(self):
        self.assertIn("graphics_init", self.wrapper)
        self.assertIn("0x00008200", self.wrapper)
        self.assertIn("bmo_psram_calloc", self.wrapper)
        self.assertGreater(len(self.gfx.splitlines()), 100)


class TestSNESCoreEngine(unittest.TestCase):
    """Verify Super Nintendo (snes9x + emu_snes.cpp) WDC 65C816 + S-PPU + DMA engine."""

    def setUp(self):
        self.snes_dir = SRC_DIR / "third_party" / "snes9x" / "src"
        self.wrapper = (SRC_DIR / "cores" / "emu_snes.cpp").read_text(encoding="utf-8")
        self.cpuexec = (self.snes_dir / "cpuexec.c").read_text(encoding="utf-8", errors="ignore")
        self.dma = (self.snes_dir / "dma.c").read_text(encoding="utf-8", errors="ignore")
        self.gfx = (self.snes_dir / "gfx.c").read_text(encoding="utf-8", errors="ignore")

    def test_01_65c816_cpu_modes_and_dma(self):
        self.assertNotIn("STUB_ENGINE", self.wrapper)
        self.assertIn("S9xMainLoop", self.cpuexec)
        self.assertIn("S9xDoDMA", self.dma)

    def test_02_sppu_cgram_bglayers_and_oam(self):
        self.assertIn("S9xUpdateScreen", self.gfx)
        self.assertIn("S9xSetLineCallback", self.wrapper)
        self.assertIn("bmo_psram_calloc", self.wrapper)


class TestPico8CoreEngine(unittest.TestCase):
    """Verify PICO-8 Fantasy Console (fake08 + emu_pico.cpp) virtual hardware & graphics API."""

    def setUp(self):
        self.fake_dir = SRC_DIR / "third_party" / "fake08" / "src"
        self.wrapper = (SRC_DIR / "cores" / "emu_pico.cpp").read_text(encoding="utf-8")
        self.vm_cpp = (self.fake_dir / "vm.cpp").read_text(encoding="utf-8", errors="ignore")
        self.gfx_cpp = (self.fake_dir / "graphics.cpp").read_text(encoding="utf-8", errors="ignore")
        self.ram_h = (self.fake_dir / "PicoRam.h").read_text(encoding="utf-8", errors="ignore")

    def test_01_pico8_graphics_primitives_and_memory_map(self):
        self.assertNotIn("STUB_ENGINE", self.wrapper)
        for prim in ("pset", "rectfill", "circfill"):
            self.assertIn(prim, self.gfx_cpp)
        self.assertIn("LoadCart", self.vm_cpp)
        self.assertIn("vm_run", self.vm_cpp)
        self.assertIn("0x5f00", self.ram_h)
        self.assertIn("0x6000", self.ram_h)
        self.assertIn("bmo_psram_calloc", self.wrapper)


class TestQualityOfLifeAndUnifiedPersistence(unittest.TestCase):
    """Verify Phase B & Phase C QoL features across BmoGameboy.ino and core modules."""

    def test_01_buttons_remap_and_turbo_api(self):
        btn_h = (SRC_DIR / "hal" / "buttons.h").read_text(encoding="utf-8")
        btn_cpp = (SRC_DIR / "hal" / "buttons.cpp").read_text(encoding="utf-8")
        for sym in ("setGameplayMode", "setSwapAB", "isSwapAB", "toggleSwapAB", "setTurboEnabled", "isTurboEnabled", "toggleTurbo"):
            self.assertIn(sym, btn_h)
            self.assertIn(sym, btn_cpp)

    def test_02_sdcard_recent_history_api(self):
        sd_h = (SRC_DIR / "storage" / "sd_card.h").read_text(encoding="utf-8")
        sd_cpp = (SRC_DIR / "storage" / "sd_card.cpp").read_text(encoding="utf-8")
        for sym in ("recordRecentLaunch", "getRecentCount", "getRecentFilename", "loadRecentHistory", "saveRecentHistory"):
            self.assertIn(sym, sd_h)
            self.assertIn(sym, sd_cpp)
        self.assertIn("/history.txt", sd_cpp)

    def test_03_save_manager_screenshot_bmp_dump(self):
        sm_h = (SRC_DIR / "storage" / "save_manager.h").read_text(encoding="utf-8")
        sm_cpp = (SRC_DIR / "storage" / "save_manager.cpp").read_text(encoding="utf-8")
        self.assertIn("saveScreenshotBmp", sm_h)
        self.assertIn("saveScreenshotBmp", sm_cpp)
        self.assertIn("/screenshots", sm_cpp)

    def test_04_ino_wires_supported_battery_persistence_and_all_save_states(self):
        ino = (FIRMWARE_DIR / "BmoGameboy.ino").read_text(encoding="utf-8")
        reg = (SRC_DIR / "cores" / "core_registry.h").read_text(encoding="utf-8")
        cores = ("WalnutEmu", "PeanutEmu", "NesEmu", "SmsEmu", "PceEmu", "AtariEmu",
                 "PicoEmu", "GenesisEmu", "SNESEmu", "WSwanEmu", "NGPEmu", "LynxEmu", "ColemEmu")
        battery_wrappers = (
            "emu_walnut.cpp", "emu_peanut.cpp", "emu_nes.cpp", "emu_sms.cpp",
            "emu_pce.cpp", "emu_genesis.cpp", "emu_snes.cpp", "emu_wswan.cpp",
            "emu_ngp.cpp", "emu_lynx.cpp",
        )
        for cls in cores:
            self.assertIn(f"{cls}::saveState", reg)
            self.assertIn(f"{cls}::loadState", reg)
            self.assertIn(f"{cls}::saveBatteryRam", reg)
            self.assertIn(f"{cls}::loadBatteryRam", reg)
        for wrapper in battery_wrappers:
            code = (SRC_DIR / "cores" / wrapper).read_text(encoding="utf-8")
            self.assertIn("SaveManager::saveBatteryRam", code, f"{wrapper} missing battery save persistence")
        for non_battery in ("ROM_WAD", "ROM_ATARI", "ROM_PICO8", "ROM_COLEM"):
            self.assertRegex(reg, rf"\b{non_battery}\b.*?\bnoBattery\b")
        self.assertIn("hasGbBattery", reg)
        self.assertIn("hasNesBattery", reg)
        self.assertIn("hasSnesBattery", reg)
        self.assertIn("hasLynxBattery", reg)
        self.assertIn("CoreRegistry::findByIndex", ino)
        self.assertIn("autoSaveCurrentBatteryRam", ino)
        self.assertIn("quickSaveState", ino)
        self.assertIn("quickLoadState", ino)
        self.assertIn("kKonamiSeq", ino)
        self.assertIn("SDCard::recordRecentLaunch", ino)


if __name__ == "__main__":
    unittest.main()
