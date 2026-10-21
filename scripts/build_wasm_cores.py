#!/usr/bin/env python3
"""
scripts/build_wasm_cores.py — Deterministic W3C WebAssembly (.wasm) Playtest Core Compiler
Assembles `tools/bmo_simulator/bmo_playtest_cores.wasm` directly from WebAssembly binary
opcodes without external Emscripten/LLVM dependencies.

Linear Memory Layout (16 pages = 1,048,576 bytes):
  0x00000..0x0003F : Core State Registers
    [0x00] console_id   (i32)
    [0x04] width        (i32)
    [0x08] height       (i32)
    [0x0C] frame_count  (i32)
    [0x10] rom_len      (i32)
    [0x14] rom_checksum (i32, FNV-1a 32-bit)
    [0x18] player_x     (i32)
    [0x1C] player_y     (i32)
    [0x20] buttons_mask (i32)
  0x10000..0x5FFFF : ROM Buffer (320 KB, ROM_BASE = 65536)
  0x60000..0x85800 : RGB565 Framebuffer (320x240x2 = 153,600 B, FB_BASE = 393216)
"""

from __future__ import annotations

import base64
import sys
from pathlib import Path
from typing import List

REPO_ROOT = Path(__file__).resolve().parents[1]
WASM_OUT_PATH = REPO_ROOT / "tools" / "bmo_simulator" / "bmo_playtest_cores.wasm"

ROM_BASE = 0x10000
FB_BASE = 0x60000

# Wasm value types
I32 = 0x7F
FUNC_TYPE = 0x60


def uleb128(val: int) -> bytes:
    """Encode unsigned integer as LEB128."""
    out = bytearray()
    while True:
        byte = val & 0x7F
        val >>= 7
        if val != 0:
            byte |= 0x80
        out.append(byte)
        if val == 0:
            break
    return bytes(out)


def sleb128(val: int) -> bytes:
    """Encode signed integer as LEB128."""
    out = bytearray()
    more = True
    while more:
        byte = val & 0x7F
        val >>= 7
        if (val == 0 and (byte & 0x40) == 0) or (val == -1 and (byte & 0x40) != 0):
            more = False
        else:
            byte |= 0x80
        out.append(byte)
    return bytes(out)


def i32_const(val: int) -> bytes:
    return b"\x41" + sleb128(val)


def local_get(idx: int) -> bytes:
    return b"\x20" + uleb128(idx)


def local_set(idx: int) -> bytes:
    return b"\x21" + uleb128(idx)


def local_tee(idx: int) -> bytes:
    return b"\x22" + uleb128(idx)


def i32_load(align: int = 2, offset: int = 0) -> bytes:
    return b"\x28" + uleb128(align) + uleb128(offset)


def i32_load8_u(align: int = 0, offset: int = 0) -> bytes:
    return b"\x2D" + uleb128(align) + uleb128(offset)


def i32_store(align: int = 2, offset: int = 0) -> bytes:
    return b"\x36" + uleb128(align) + uleb128(offset)


def i32_store16(align: int = 1, offset: int = 0) -> bytes:
    return b"\x3B" + uleb128(align) + uleb128(offset)


def encode_section(sec_id: int, payload: bytes) -> bytes:
    return bytes([sec_id]) + uleb128(len(payload)) + payload


def encode_vec(items: List[bytes]) -> bytes:
    return uleb128(len(items)) + b"".join(items)


def encode_func_body(locals_i32_count: int, code: bytes) -> bytes:
    if locals_i32_count == 0:
        locals_vec = uleb128(0)
    else:
        locals_vec = uleb128(1) + uleb128(locals_i32_count) + bytes([I32])
    body = locals_vec + code + b"\x0B"  # end
    return uleb128(len(body)) + body


def build_wasm_module() -> bytes:
    # Types:
    # type 0: (i32, i32, i32) -> (i32)  [wasm_core_init]
    # type 1: (i32) -> (i32)            [wasm_core_load_rom, wasm_core_step_frame]
    # type 2: () -> (i32)               [getters]
    t0 = bytes([FUNC_TYPE, 3, I32, I32, I32, 1, I32])
    t1 = bytes([FUNC_TYPE, 1, I32, 1, I32])
    t2 = bytes([FUNC_TYPE, 0, 1, I32])
    type_sec = encode_section(1, encode_vec([t0, t1, t2]))

    # Functions:
    # 0: wasm_core_init (type 0)
    # 1: wasm_core_load_rom (type 1)
    # 2: wasm_core_step_frame (type 1)
    # 3: wasm_core_get_rom_ptr (type 2)
    # 4: wasm_core_get_fb_ptr (type 2)
    # 5: wasm_core_get_checksum (type 2)
    # 6: wasm_core_get_frame_count (type 2)
    func_sec = encode_section(3, encode_vec([
        uleb128(0),
        uleb128(1),
        uleb128(1),
        uleb128(2),
        uleb128(2),
        uleb128(2),
        uleb128(2),
    ]))

    # Memory section: 1 memory, min 16 pages (1 MiB), max 16 pages
    mem_sec = encode_section(5, encode_vec([bytes([0x01]) + uleb128(16) + uleb128(16)]))

    # Export section:
    def exp_entry(name: str, kind: int, idx: int) -> bytes:
        nb = name.encode("utf-8")
        return uleb128(len(nb)) + nb + bytes([kind]) + uleb128(idx)

    export_sec = encode_section(7, encode_vec([
        exp_entry("memory", 2, 0),
        exp_entry("wasm_core_init", 0, 0),
        exp_entry("wasm_core_load_rom", 0, 1),
        exp_entry("wasm_core_step_frame", 0, 2),
        exp_entry("wasm_core_get_rom_ptr", 0, 3),
        exp_entry("wasm_core_get_fb_ptr", 0, 4),
        exp_entry("wasm_core_get_checksum", 0, 5),
        exp_entry("wasm_core_get_frame_count", 0, 6),
    ]))

    # Function 0: wasm_core_init(console_id: 0, width: 1, height: 2) -> i32
    f0_code = b"".join([
        i32_const(0), local_get(0), i32_store(2, 0x00),   # console_id
        i32_const(0), local_get(1), i32_store(2, 0x04),   # width
        i32_const(0), local_get(2), i32_store(2, 0x08),   # height
        i32_const(0), i32_const(0), i32_store(2, 0x0C),   # frame_count = 0
        i32_const(0), local_get(1), i32_const(1), b"\x76", i32_store(2, 0x18),  # player_x = width >> 1
        i32_const(0), local_get(2), i32_const(1), b"\x76", i32_store(2, 0x1C),  # player_y = height >> 1
        i32_const(1),
    ])

    # Function 1: wasm_core_load_rom(rom_len: 0) -> i32 (computes FNV-1a 32-bit over ROM_BASE..ROM_BASE+rom_len)
    # locals: 1=hash, 2=idx
    f1_code = b"".join([
        i32_const(0), local_get(0), i32_store(2, 0x10),   # store rom_len
        i32_const(-2128831035), local_set(1),             # 0x811C9DC5 signed i32
        i32_const(0), local_set(2),
        b"\x02\x40",                                      # block $break
        b"\x03\x40",                                      # loop $top
        local_get(2), local_get(0), b"\x4F",              # idx >= rom_len (i32.ge_u)
        b"\x0D\x01",                                      # br_if $break
        local_get(1),
        local_get(2), i32_load8_u(0, ROM_BASE),
        b"\x73",                                          # i32.xor
        i32_const(16777619),
        b"\x6C",                                          # i32.mul
        local_set(1),
        local_get(2), i32_const(1), b"\x6A", local_set(2),
        b"\x0C\x00",                                      # br $top
        b"\x0B",                                          # end loop
        b"\x0B",                                          # end block
        i32_const(0), local_get(1), i32_store(2, 0x14),   # store rom_checksum
        local_get(1),
    ])

    # Function 2: wasm_core_step_frame(buttons_mask: 0) -> i32
    # Updates frame_count, moves player_x/player_y based on buttons_mask, and renders
    # an interactive ROM-seeded RGB565 frame into FB_BASE (0x60000).
    # locals:
    #   1 = width, 2 = height, 3 = frame, 4 = px, 5 = py, 6 = x, 7 = y,
    #   8 = fb_ptr, 9 = rom_len, 10 = color, 11 = hash
    f2_code = b"".join([
        i32_const(0), local_get(0), i32_store(2, 0x20),
        i32_const(0), i32_load(2, 0x04), local_set(1),    # width
        i32_const(0), i32_load(2, 0x08), local_set(2),    # height
        i32_const(0), i32_load(2, 0x0C), i32_const(1), b"\x6A", local_tee(3),
        i32_const(0), local_get(3), i32_store(2, 0x0C),   # ++frame_count -> mem[0x0C]
        b"\x1A",                                          # drop tee'd value
        i32_const(0), i32_load(2, 0x18), local_set(4),    # px
        i32_const(0), i32_load(2, 0x1C), local_set(5),    # py
        i32_const(0), i32_load(2, 0x10), local_set(9),    # rom_len
        i32_const(0), i32_load(2, 0x14), local_set(11),   # hash

        # UP (bit 0 = 1): if (buttons & 1) py = max(8, py - 2)
        local_get(0), i32_const(1), b"\x71",
        b"\x04\x40",
        local_get(5), i32_const(2), b"\x6B", local_set(5),
        local_get(5), i32_const(8), b"\x48",
        b"\x04\x40", i32_const(8), local_set(5), b"\x0B",
        b"\x0B",

        # DOWN (bit 1 = 2): if (buttons & 2) py = min(height - 8, py + 2)
        local_get(0), i32_const(2), b"\x71",
        b"\x04\x40",
        local_get(5), i32_const(2), b"\x6A", local_set(5),
        local_get(5), local_get(2), i32_const(8), b"\x6B", b"\x4A",
        b"\x04\x40", local_get(2), i32_const(8), b"\x6B", local_set(5), b"\x0B",
        b"\x0B",

        # LEFT (bit 2 = 4): if (buttons & 4) px = max(8, px - 2)
        local_get(0), i32_const(4), b"\x71",
        b"\x04\x40",
        local_get(4), i32_const(2), b"\x6B", local_set(4),
        local_get(4), i32_const(8), b"\x48",
        b"\x04\x40", i32_const(8), local_set(4), b"\x0B",
        b"\x0B",

        # RIGHT (bit 3 = 8): if (buttons & 8) px = min(width - 8, px + 2)
        local_get(0), i32_const(8), b"\x71",
        b"\x04\x40",
        local_get(4), i32_const(2), b"\x6A", local_set(4),
        local_get(4), local_get(1), i32_const(8), b"\x6B", b"\x4A",
        b"\x04\x40", local_get(1), i32_const(8), b"\x6B", local_set(4), b"\x0B",
        b"\x0B",

        i32_const(0), local_get(4), i32_store(2, 0x18),
        i32_const(0), local_get(5), i32_store(2, 0x1C),

        # Render loop over y = 0..height-1, x = 0..width-1
        i32_const(FB_BASE), local_set(8),
        i32_const(0), local_set(7),                       # y = 0
        b"\x02\x40",                                      # block $y_break
        b"\x03\x40",                                      # loop $y_loop
        local_get(7), local_get(2), b"\x4F",              # y >= height (i32.ge_u)
        b"\x0D\x01",                                      # br_if $y_break

        i32_const(0), local_set(6),                       # x = 0
        b"\x02\x40",                                      # block $x_break
        b"\x03\x40",                                      # loop $x_loop
        local_get(6), local_get(1), b"\x4F",              # x >= width (i32.ge_u)
        b"\x0D\x01",                                      # br_if $x_break

        # Compute base tile pattern from (x + frame) ^ (y + (hash & 0xFF))
        local_get(6), local_get(3), b"\x6A",
        local_get(7), local_get(11), i32_const(0xFF), b"\x71", b"\x6A",
        b"\x73",                                          # xor
        i32_const(0x1F), b"\x71",
        i32_const(0x0841), b"\x6C",                       # RGB565 gradient
        local_set(10),

        # Mix ROM byte at ((x >> 3) + (y >> 3) * 32) & 0x3FF if rom_len >= 1024
        local_get(9), i32_const(1024), b"\x4F",           # rom_len >= 1024
        b"\x04\x40",
        local_get(6), i32_const(3), b"\x76",
        local_get(7), i32_const(3), b"\x76", i32_const(5), b"\x74", b"\x6A",
        local_get(3), i32_const(2), b"\x76", b"\x6A",
        i32_const(0x3FF), b"\x71",
        i32_load8_u(0, ROM_BASE),
        i32_const(3), b"\x74",
        local_get(10), b"\x73", local_set(10),
        b"\x0B",

        # Draw 12x12 player sprite around (px, py)
        local_get(6), local_get(4), b"\x6B", i32_const(6), b"\x6A", i32_const(12), b"\x49",  # (x - px + 6) <_u 12
        local_get(7), local_get(5), b"\x6B", i32_const(6), b"\x6A", i32_const(12), b"\x49",  # (y - py + 6) <_u 12
        b"\x71",
        b"\x04\x40",
        # If button A (16) or B (32) pressed, highlight in bright coral/gold, else BMO yellow/mint
        local_get(0), i32_const(48), b"\x71",
        b"\x04\x7F",
        i32_const(0xF81F),
        b"\x05",
        i32_const(0xFFE0),
        b"\x0B",
        local_set(10),
        b"\x0B",

        # Store 16-bit RGB565 pixel at fb_ptr
        local_get(8), local_get(10), i32_store16(1, 0),
        local_get(8), i32_const(2), b"\x6A", local_set(8),

        local_get(6), i32_const(1), b"\x6A", local_set(6),
        b"\x0C\x00",                                      # br $x_loop
        b"\x0B",                                          # end x_loop
        b"\x0B",                                          # end x_block

        local_get(7), i32_const(1), b"\x6A", local_set(7),
        b"\x0C\x00",                                      # br $y_loop
        b"\x0B",                                          # end y_loop
        b"\x0B",                                          # end y_block

        local_get(3),
    ])

    # Function 3: wasm_core_get_rom_ptr() -> ROM_BASE
    f3_code = i32_const(ROM_BASE)
    # Function 4: wasm_core_get_fb_ptr() -> FB_BASE
    f4_code = i32_const(FB_BASE)
    # Function 5: wasm_core_get_checksum() -> mem[0x14]
    f5_code = i32_const(0) + i32_load(2, 0x14)
    # Function 6: wasm_core_get_frame_count() -> mem[0x0C]
    f6_code = i32_const(0) + i32_load(2, 0x0C)

    code_sec = encode_section(10, encode_vec([
        encode_func_body(0, f0_code),
        encode_func_body(2, f1_code),
        encode_func_body(11, f2_code),
        encode_func_body(0, f3_code),
        encode_func_body(0, f4_code),
        encode_func_body(0, f5_code),
        encode_func_body(0, f6_code),
    ]))

    wasm_bytes = b"\x00asm\x01\x00\x00\x00" + type_sec + func_sec + mem_sec + export_sec + code_sec
    return wasm_bytes


def main() -> int:
    wasm_bytes = build_wasm_module()
    WASM_OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    WASM_OUT_PATH.write_bytes(wasm_bytes)
    b64 = base64.b64encode(wasm_bytes).decode("ascii")
    print(f"[WASM] Built {WASM_OUT_PATH} ({len(wasm_bytes)} bytes, base64 len={len(b64)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
