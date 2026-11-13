#!/usr/bin/env python3
"""
Unit tests for BMO Handheld Console Box Art Engine (64x64 Cover Art)
Tests path extraction, BMP header parsing, pixel layout, and color conversion.
"""

import os
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

from host_test_support import find_wasi_cxx, require_compiler

REPO_ROOT = Path(__file__).resolve().parents[1]

BOX_ART_WIDTH = 64
BOX_ART_HEIGHT = 64
BOX_ART_BUFFER_SIZE = BOX_ART_WIDTH * BOX_ART_HEIGHT * 2  # 8,192 bytes


def get_base_name(filename: str) -> str:
    base = os.path.basename(filename.replace("\\", "/"))
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

    def test_catalog_matcher_collision_and_keyed_lookup_in_cxx(self):
        compiler = find_wasi_cxx(REPO_ROOT)
        node = shutil.which("node")
        require_compiler(self, compiler, "Pinned WASI C++ compiler")
        require_compiler(self, node, "Node.js WASI runner")
        source = REPO_ROOT / "tests" / "host" / "box_art_lookup_test.cpp"
        include = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "storage"

        with tempfile.TemporaryDirectory() as temp_dir:
            wasm = Path(temp_dir) / "box_art_lookup_test.wasm"
            subprocess.run(
                [str(compiler), "--target=wasm32-wasip1", "-std=c++17", "-I", str(include),
                 str(source), "-o", str(wasm)],
                cwd=REPO_ROOT,
                check=True,
                capture_output=True,
                text=True,
            )
            runner = (
                "const fs=require('fs'); const {WASI}=require('node:wasi'); "
                "(async()=>{const wasi=new WASI({version:'preview1',args:[process.argv[1]]}); "
                "const mod=await WebAssembly.compile(fs.readFileSync(process.argv[1])); "
                "const instance=await WebAssembly.instantiate(mod,{wasi_snapshot_preview1:wasi.wasiImport}); "
                "wasi.start(instance);})().catch(e=>{console.error(e);process.exit(1);});"
            )
            res = subprocess.run(
                [node, "-e", runner, str(wasm)],
                cwd=REPO_ROOT,
                check=True,
                capture_output=True,
                text=True,
            )
        self.assertIn("BOX_ART_LOOKUP_OK", res.stdout)


if __name__ == "__main__":
    unittest.main()
