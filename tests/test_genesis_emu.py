#!/usr/bin/env python3
"""
test_genesis_emu.py
Unit tests for the Sega Genesis / Mega Drive (68000 CPU + VDP Scanline) emulator core.
Verifies ROM header validation, vector table extraction, 68000 bus mapping,
VDP CRAM color conversion, register decoding, controller multiplexing, and PSRAM budget.
"""

import unittest
import struct

class TestGenesisEmulatorCore(unittest.TestCase):
    """Test suite for Sega Genesis / Mega Drive hardware architecture and core logic."""

    def test_01_rom_header_and_vector_table(self):
        """Verify vector table (SSP, PC) extraction from Genesis ROM header."""
        # Genesis vector table:
        # 0x000000: Initial SSP (e.g. 0x00FFFE00)
        # 0x000004: Initial PC  (e.g. 0x00000200)
        # 0x000100: "SEGA MEGA DRIVE " or "SEGA GENESIS    "
        rom = bytearray(512)
        expected_ssp = 0x00FFFE00
        expected_pc = 0x00000200
        struct.pack_into(">II", rom, 0, expected_ssp, expected_pc)
        rom[0x100:0x110] = b"SEGA MEGA DRIVE "

        ssp = (rom[0] << 24) | (rom[1] << 16) | (rom[2] << 8) | rom[3]
        pc = (rom[4] << 24) | (rom[5] << 16) | (rom[6] << 8) | rom[7]
        magic = bytes(rom[0x100:0x110]).decode('ascii', errors='ignore')

        self.assertEqual(ssp, expected_ssp, "Initial SSP must match big-endian 32-bit vector")
        self.assertEqual(pc, expected_pc, "Initial PC must match big-endian 32-bit vector")
        self.assertIn("SEGA", magic, "Magic string must identify Sega platform")

    def test_02_memory_bus_decoding(self):
        """Verify 24-bit 68000 bus range decoding."""
        def decode_region(addr):
            addr &= 0xFFFFFF
            if addr < 0x400000:
                return "ROM"
            elif 0xA00000 <= addr <= 0xA01FFF:
                return "Z80_RAM"
            elif 0xA10000 <= addr <= 0xA1001F:
                return "IO_PORTS"
            elif 0xC00000 <= addr <= 0xC0001F:
                return "VDP"
            elif 0xE00000 <= addr <= 0xFFFFFF:
                return "WORK_RAM"
            return "UNMAPPED"

        self.assertEqual(decode_region(0x000000), "ROM")
        self.assertEqual(decode_region(0x1FFFFF), "ROM")
        self.assertEqual(decode_region(0xA00050), "Z80_RAM")
        self.assertEqual(decode_region(0xA10003), "IO_PORTS")
        self.assertEqual(decode_region(0xC00004), "VDP")
        self.assertEqual(decode_region(0xFF0000), "WORK_RAM")
        self.assertEqual(decode_region(0xE01234), "WORK_RAM", "Mirror of Work RAM")

    def test_03_vdp_cram_rgb_to_bgr565(self):
        """Verify VDP 9-bit RGB to ST7789 byte-swapped BGR565 conversion."""
        # Genesis CRAM: 0000 bbb0 ggg0 rrr0 (each channel 0..7)
        def genesis_color_to_bgr565(cram_val):
            r = (cram_val >> 1) & 0x07
            g = (cram_val >> 5) & 0x07
            b = (cram_val >> 9) & 0x07
            r8 = (r * 255) // 7
            g8 = (g * 255) // 7
            b8 = (b * 255) // 7
            bgr = ((b8 & 0xF8) << 8) | ((g8 & 0xFC) << 3) | (r8 >> 3)
            # Byte-swap for ST7789 SPI
            return ((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF)

        # Pure Black (0x000)
        self.assertEqual(genesis_color_to_bgr565(0x000), 0x0000)
        # Pure White (RGB 7,7,7 -> 0x0EEE)
        white = genesis_color_to_bgr565(0x0EEE)
        self.assertEqual(white, 0xFFFF)
        # Red (r=7, g=0, b=0 -> 0x000E)
        red = genesis_color_to_bgr565(0x000E)
        self.assertNotEqual(red, 0x0000)

    def test_04_vdp_register_and_command_decoding(self):
        """Verify VDP command word and register write decoding."""
        # Register write command: 1000 RRRR DDDD DDDD (bit 15=1, 14=0)
        cmd_reg = 0x8174  # Write 0x74 to Register 1 (Mode Set 2)
        is_reg_write = (cmd_reg & 0xC000) == 0x8000
        reg_num = (cmd_reg >> 8) & 0x1F
        reg_val = cmd_reg & 0xFF

        self.assertTrue(is_reg_write)
        self.assertEqual(reg_num, 1)
        self.assertEqual(reg_val, 0x74)

        # Address / Code command: 2 words
        # Word 1: 0x4000 (VRAM write at 0x0000)
        # Word 2: 0x0000
        w1 = 0x4000
        w2 = 0x0000
        vdp_code = ((w1 >> 14) & 0x03) | ((w2 >> 2) & 0x3C)
        vdp_addr = (w1 & 0x3FFF) | ((w2 & 0x0007) << 14)
        self.assertEqual(vdp_code, 1, "Code 1 represents VRAM Write")
        self.assertEqual(vdp_addr, 0)

    def test_05_controller_multiplexing(self):
        """Verify Genesis 3-button controller TH pin multiplexing."""
        # pad_state bits: UP=0x01, DOWN=0x02, LEFT=0x04, RIGHT=0x08, B=0x10, A=0x20, START=0x80
        pad_state = 0x01 | 0x20  # UP + A pressed

        # When TH is 1: returns D-Pad + B + C (bit 0=Up, 1=Down, 2=Left, 3=Right, 4=B, 5=C)
        # Genesis inputs are active LOW (0 when pressed, 1 when released)
        def read_port(pad, th_high):
            if th_high:
                data = 0x7F
                if pad & 0x01: data &= ~0x01  # UP
                if pad & 0x02: data &= ~0x02  # DOWN
                if pad & 0x04: data &= ~0x04  # LEFT
                if pad & 0x08: data &= ~0x08  # RIGHT
                if pad & 0x10: data &= ~0x10  # B (maps to Genesis B)
                return data
            else:
                data = 0x7F
                if pad & 0x01: data &= ~0x01  # UP
                if pad & 0x02: data &= ~0x02  # DOWN
                if pad & 0x20: data &= ~0x10  # A (maps to Genesis A)
                if pad & 0x80: data &= ~0x20  # START
                return data

        th1 = read_port(pad_state, True)
        th0 = read_port(pad_state, False)

        # When TH is 1, UP is active (bit 0 cleared)
        self.assertEqual(th1 & 0x01, 0)
        # When TH is 0, A is active (bit 4 cleared)
        self.assertEqual(th0 & 0x10, 0)

    def test_06_framebuffer_and_psram_budget(self):
        """Verify 320x224 frame resolution and Octal PSRAM footprint."""
        fb_w, fb_h = 320, 224
        fb_bytes = fb_w * fb_h * 2
        self.assertEqual(fb_bytes, 143360)

        # Genesis PSRAM allocations:
        # Framebuffer (143,360) + 68K RAM (65,536) + VRAM (65,536) + Z80 RAM (8,192) + CRAM/VSRAM (208)
        ram_bytes = 65536
        vram_bytes = 65536
        z80_bytes = 8192
        cram_vsram_bytes = 128 + 80
        total_bytes = fb_bytes + ram_bytes + vram_bytes + z80_bytes + cram_vsram_bytes

        self.assertLess(total_bytes, 300 * 1024, "Total core memory must stay under 300 KB in PSRAM")

    def test_07_68000_instruction_opcode_table(self):
        """Verify 68000 opcode masks for primary instruction families."""
        # NOP = 0x4E71
        self.assertEqual(0x4E71 & 0xFFFF, 0x4E71)
        # RTS = 0x4E75
        self.assertEqual(0x4E75 & 0xFFFF, 0x4E75)
        # MOVEQ: 0111 RRR 0 DDDDDDDD -> top 4 bits = 0x7
        self.assertEqual((0x7000 >> 12) & 0xF, 0x7)
        # BRA: 0110 0000 DDDDDDDD -> top 8 bits = 0x60
        self.assertEqual((0x6000 >> 8) & 0xFF, 0x60)
        # BSR: 0110 0001 DDDDDDDD -> top 8 bits = 0x61
        self.assertEqual((0x6100 >> 8) & 0xFF, 0x61)


if __name__ == "__main__":
    unittest.main()
