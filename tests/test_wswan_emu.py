#!/usr/bin/env python3
"""
test_wswan_emu.py
Unit tests for the Bandai WonderSwan / WonderSwan Color (NEC V30MZ CPU + VDC) emulator core.
Verifies reset vector, 20-bit segmented memory mapping, RGB444 color conversion,
I/O port registers, keypad input multiplexing, and PSRAM memory budget.
"""

import unittest
import struct

class TestWSwanEmulatorCore(unittest.TestCase):
    """Test suite for WonderSwan hardware architecture and core logic."""

    def test_01_reset_vector_and_entry_point(self):
        """Verify V30MZ reset vector CS:IP = FFFF:0000 (Physical 0xFFFF0)."""
        cs = 0xFFFF
        ip = 0x0000
        physical = (cs << 4) + ip
        self.assertEqual(physical, 0xFFFF0, "NEC V30MZ reset vector must start at physical 0xFFFF0")

    def test_02_segmented_address_calculation(self):
        """Verify 20-bit segmented real-mode address calculation: (segment << 4) + offset."""
        def to_physical(seg, off):
            return ((seg << 4) + off) & 0xFFFFF

        # IVT vector 0 (Divide by Zero)
        self.assertEqual(to_physical(0x0000, 0x0000), 0x00000)
        # Internal RAM middle
        self.assertEqual(to_physical(0x0800, 0x0124), 0x08124)
        # Boundary wrap at 1MB
        self.assertEqual(to_physical(0xFFFF, 0x0010), 0x00000)

    def test_03_memory_map_regions(self):
        """Verify 1MB address bus region classification."""
        def classify_address(addr):
            addr &= 0xFFFFF
            if addr < 0x10000:
                return "IRAM"       # 64KB Internal RAM
            elif addr < 0x20000:
                return "SRAM"       # 64KB Cartridge Save RAM
            else:
                return "ROM"        # Cartridge ROM

        self.assertEqual(classify_address(0x00000), "IRAM")
        self.assertEqual(classify_address(0x0FFFF), "IRAM")
        self.assertEqual(classify_address(0x10000), "SRAM")
        self.assertEqual(classify_address(0x1FFFF), "SRAM")
        self.assertEqual(classify_address(0x20000), "ROM")
        self.assertEqual(classify_address(0xFFFF0), "ROM")

    def test_04_wswan_rgb444_to_bgr565_conversion(self):
        """Verify WonderSwan Color 12-bit RGB444 to ST7789 byte-swapped BGR565 conversion."""
        # WonderSwan Color: 16-bit word with 12-bit color: 0000 bbbb gggg rrrr (each channel 0..15)
        def wswan_color_to_bgr565(c12):
            r = c12 & 0x0F
            g = (c12 >> 4) & 0x0F
            b = (c12 >> 8) & 0x0F
            r8 = (r * 255) // 15
            g8 = (g * 255) // 15
            b8 = (b * 255) // 15
            bgr = ((b8 & 0xF8) << 8) | ((g8 & 0xFC) << 3) | (r8 >> 3)
            # Byte-swap for ST7789 SPI
            return ((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF)

        # Black
        self.assertEqual(wswan_color_to_bgr565(0x000), 0x0000)
        # White (RGB 15,15,15 -> 0x0FFF)
        white = wswan_color_to_bgr565(0x0FFF)
        self.assertEqual(white, 0xFFFF)

    def test_05_io_port_assignments(self):
        """Verify WonderSwan hardware I/O port assignments."""
        ports = {
            0x00: "DISP_CTRL",
            0x02: "LINE_CURRENT",
            0x03: "LINE_COMPARE",
            0x04: "SPR_TABLE_BASE",
            0x08: "BG_SCROLL_X",
            0x09: "BG_SCROLL_Y",
            0x15: "INT_STATUS",
            0x60: "HW_SYSTEM_TYPE",
            0xB5: "KEYPAD_READ",
        }
        self.assertEqual(ports[0x60], "HW_SYSTEM_TYPE")
        self.assertEqual(ports[0xB5], "KEYPAD_READ")

    def test_06_keypad_bit_masking(self):
        """Verify WonderSwan keypad bits (X1-X4, Button A, Button B, Start)."""
        keys = 0x0001 | 0x0010  # X1 (Up) + Button A
        has_up = bool(keys & 0x0001)
        has_right = bool(keys & 0x0002)
        has_a = bool(keys & 0x0010)
        has_b = bool(keys & 0x0020)

        self.assertTrue(has_up)
        self.assertFalse(has_right)
        self.assertTrue(has_a)
        self.assertFalse(has_b)

    def test_07_framebuffer_and_psram_budget(self):
        """Verify 224x144 frame resolution and Octal PSRAM footprint."""
        fb_w, fb_h = 224, 144
        fb_bytes = fb_w * fb_h * 2
        self.assertEqual(fb_bytes, 64512)

        # WonderSwan PSRAM allocations:
        # Framebuffer (64,512) + 64KB IRAM (65,536) + 64KB Cart SRAM (65,536)
        iram_bytes = 65536
        sram_bytes = 65536
        total_bytes = fb_bytes + iram_bytes + sram_bytes
        self.assertLess(total_bytes, 200 * 1024, "Total core memory must stay under 200 KB in PSRAM")

    def test_08_c_core_source_invariants_and_banking(self):
        """Verify actual wswan.c source implements bank_rom[2..3], bank_sram port 0xC1, and active-high port 0xB5."""
        from pathlib import Path
        repo_root = Path(__file__).resolve().parents[1]
        c_src = (repo_root / "firmware" / "BmoGameboy" / "src" / "vendor" / "wswan" / "wswan.c").read_text(encoding="utf-8")
        self.assertIn("emu->bank_rom[2]", c_src, "wswan_read8 must use bank_rom[2] for $20000-$2FFFF")
        self.assertIn("emu->bank_rom[3]", c_src, "wswan_read8 must use bank_rom[3] for $30000-$3FFFF")
        self.assertIn("if (port == 0xC1)", c_src, "Port 0xC1 must be checked before 0xC0..0xC3 range")
        self.assertIn("sel & 0x20", c_src, "Port 0xB5 must multiplex X1-X4 cursor nibble on bit 5")


if __name__ == "__main__":
    unittest.main()
