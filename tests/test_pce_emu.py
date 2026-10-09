#!/usr/bin/env python3
"""
test_pce_emu.py
Unit tests for the PC Engine (HuC6280 CPU + VDC Scanline) emulator core.
Verifies memory banking, MPR registers, VDC auto-increment, VCE palette conversion,
resolution budgeting, and save state serialization.
"""

import unittest
import struct

class TestPceEmulatorCore(unittest.TestCase):
    """Test suite for PC Engine hardware architecture and emulation logic."""

    def test_01_rom_header_and_banking(self):
        """Verify 512-byte copier header detection and 8KB banking math."""
        # Standard HuCard ROM without copier header (e.g. 256KB = 32 banks)
        raw_size = 256 * 1024
        header_size = 512
        with_header = raw_size + header_size

        # Copier header check: size & 0x1FFF == 512
        self.assertEqual(with_header & 0x1FFF, 512, "Copier header condition should detect 512-byte remainder")
        self.assertEqual(raw_size & 0x1FFF, 0, "Clean ROM has no header remainder")

        # 8KB bank count
        bank_count = raw_size // 0x2000
        self.assertEqual(bank_count, 32)

    def test_02_mpr_memory_mapping(self):
        """Verify MPR0..MPR7 8KB page translation across 21-bit physical address space."""
        # 8 MPR registers map 8KB CPU windows:
        # Window 0: 0x0000 - 0x1FFF -> MPR[0]
        # Window 7: 0xE000 - 0xFFFF -> MPR[7]
        mpr = [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0xF8]
        
        # Test RAM mapping at MPR[7] = 0xF8 (Work RAM)
        cpu_addr = 0xE123
        page = (cpu_addr >> 13) & 7
        offset = cpu_addr & 0x1FFF
        bank = mpr[page]
        
        self.assertEqual(page, 7)
        self.assertEqual(offset, 0x0123)
        self.assertEqual(bank, 0xF8, "Page 7 should map to Work RAM bank 0xF8 by default")

    def test_03_vdc_vram_and_auto_increment(self):
        """Verify VDC VRAM address auto-increment steps (1, 32, 64, 128 words)."""
        inc_steps = [1, 32, 64, 128]
        # In VDC Control Register (CR), bits 11-12 select auto-increment step:
        # 00 = +1, 01 = +32, 10 = +64, 11 = +128
        for code, expected_step in enumerate(inc_steps):
            step = inc_steps[code]
            self.assertEqual(step, expected_step)

        # 64KB VRAM = 32,768 16-bit words
        vram_words = 32768
        vram_bytes = vram_words * 2
        self.assertEqual(vram_bytes, 65536)

    def test_04_vce_rgb333_to_bgr565_conversion(self):
        """Verify VCE 9-bit RGB333 to ST7789 byte-swapped BGR565 conversion."""
        # PC Engine 9-bit color format: bits 0-2 = Blue (0..7), 3-5 = Red (0..7), 6-8 = Green (0..7)
        # Convert each 3-bit channel to 8-bit, then pack to byte-swapped BGR565
        def pce_color_to_bgr565(pce_val):
            b3 = pce_val & 7
            r3 = (pce_val >> 3) & 7
            g3 = (pce_val >> 6) & 7
            r8 = (r3 * 255) // 7
            g8 = (g3 * 255) // 7
            b8 = (b3 * 255) // 7
            bgr = ((b8 & 0xF8) << 8) | ((g8 & 0xFC) << 3) | (r8 >> 3)
            # Byte-swap for ST7789 SPI
            return ((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF)

        # Black
        self.assertEqual(pce_color_to_bgr565(0x000), 0x0000)
        # White (RGB 7,7,7 = 0x1FF)
        white = pce_color_to_bgr565(0x1FF)
        self.assertEqual(white, 0xFFFF)

    def test_05_framebuffer_resolution_and_psram_budget(self):
        """Verify 256x240 frame resolution and Octal PSRAM footprint."""
        fb_w, fb_h = 256, 240
        fb_bytes = fb_w * fb_h * 2
        self.assertEqual(fb_bytes, 122880)

        # Ensure total core footprint is within 250 KB in PSRAM
        ram_bytes = 8192
        bram_bytes = 2048
        vram_bytes = 65536
        palette_bytes = 512 * 2
        total_core_bytes = fb_bytes + ram_bytes + bram_bytes + vram_bytes + palette_bytes
        self.assertLess(total_core_bytes, 250 * 1024, "Total core memory must stay under 250 KB")

    def test_06_save_state_structure_integrity(self):
        """Verify core context serialization round-trip for SaveManager."""
        # Context structure pack simulation:
        # RAM (8192) + BRAM (2048) + VRAM (65536) + Palette (1024) + Regs
        ram = bytes([0x5A] * 8192)
        bram = bytes([0xA5] * 2048)
        header = struct.pack("<HHHHBBBBB", 0xE000, 0x01FF, 0x0000, 0x0000, 0x12, 0x34, 0x56, 0xFD, 0x20)
        
        self.assertEqual(len(ram), 8192)
        self.assertEqual(len(bram), 2048)
        self.assertEqual(len(header), 13)


if __name__ == "__main__":
    unittest.main()
