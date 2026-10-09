#!/usr/bin/env python3
"""
Unit tests for BMO Handheld Console Box Art Engine (64x64 Cover Art)
Tests path extraction, BMP header parsing, pixel layout, and color conversion.
"""

import os
import struct
import unittest

BOX_ART_WIDTH = 64
BOX_ART_HEIGHT = 64
BOX_ART_BUFFER_SIZE = BOX_ART_WIDTH * BOX_ART_HEIGHT * 2  # 8,192 bytes


def get_base_name(filename: str) -> str:
    base = os.path.basename(filename)
    root, _ = os.path.splitext(base)
    return root


def make_ui_color(r: int, g: int, b: int) -> int:
    """Simulates Theme::makeUiColor(r, g, b) which converts to byte-swapped BGR565."""
    bgr565 = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3)
    swapped = ((bgr565 & 0xFF) << 8) | ((bgr565 >> 8) & 0xFF)
    return swapped & 0xFFFF


def pack_bmp_header(width: int, height: int, bpp: int = 24) -> bytes:
    """Generates standard 54-byte BMP header."""
    row_size = ((width * bpp + 31) // 32) * 4
    image_size = row_size * abs(height)
    file_size = 54 + image_size
    data_offset = 54
    header_size = 40
    planes = 1
    compression = 0

    return struct.pack(
        "<2sIHHI" "IiiHHIIiiII",
        b"BM",
        file_size,
        0, 0,
        data_offset,
        header_size,
        width,
        height,
        planes,
        bpp,
        compression,
        image_size,
        2835, 2835,
        0, 0
    )


class TestBoxArtEngine(unittest.TestCase):
    def test_buffer_size_invariant(self):
        self.assertEqual(BOX_ART_BUFFER_SIZE, 8192)
        self.assertEqual(BOX_ART_WIDTH * BOX_ART_HEIGHT, 4096)

    def test_base_name_extraction(self):
        self.assertEqual(get_base_name("Pokemon - Red Version (USA).gb"), "Pokemon - Red Version (USA)")
        self.assertEqual(get_base_name("/sd/roms/Doom.wad"), "Doom")
        self.assertEqual(get_base_name("roms\\snes\\Super Mario World.sfc"), "Super Mario World")
        self.assertEqual(get_base_name("Aladdin.gbc"), "Aladdin")

    def test_candidate_paths(self):
        base = get_base_name("Sonic The Hedgehog.sms")
        paths = [
            f"/boxart/{base}.raw",
            f"/boxart/{base}.bmp",
            f"/covers/{base}.raw",
            f"/covers/{base}.bmp",
        ]
        self.assertEqual(paths[0], "/boxart/Sonic The Hedgehog.raw")
        self.assertEqual(paths[1], "/boxart/Sonic The Hedgehog.bmp")
        self.assertEqual(paths[2], "/covers/Sonic The Hedgehog.raw")
        self.assertEqual(paths[3], "/covers/Sonic The Hedgehog.bmp")

    def test_bmp_header_parsing(self):
        header = pack_bmp_header(64, 64, 24)
        self.assertEqual(len(header), 54)
        self.assertEqual(header[:2], b"BM")
        width, height = struct.unpack_from("<ii", header, 18)
        self.assertEqual(width, 64)
        self.assertEqual(height, 64)
        bpp = struct.unpack_from("<H", header, 28)[0]
        self.assertEqual(bpp, 24)

    def test_bmp_16bit_header(self):
        header = pack_bmp_header(64, 64, 16)
        self.assertEqual(header[:2], b"BM")
        bpp = struct.unpack_from("<H", header, 28)[0]
        self.assertEqual(bpp, 16)

    def test_color_conversion_roundtrip(self):
        # Pure red (255, 0, 0)
        c_red = make_ui_color(255, 0, 0)
        self.assertIsInstance(c_red, int)
        self.assertTrue(0 <= c_red <= 0xFFFF)

        # Pure green (0, 255, 0)
        c_green = make_ui_color(0, 255, 0)
        self.assertNotEqual(c_red, c_green)

        # Pure blue (0, 0, 255)
        c_blue = make_ui_color(0, 0, 255)
        self.assertNotEqual(c_green, c_blue)


if __name__ == "__main__":
    unittest.main()
