"""
scripts/color_calc.py — RGB888 <-> Byte-Swapped BGR565 ST7789 Wire Format Calculator
"""

import argparse
from typing import Tuple


def to_bgr565_swapped(r: int, g: int, b: int) -> int:
    """Converts 8-bit RGB (0..255) into ST7789 byte-swapped BGR565 wire format."""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    bgr = (b5 << 11) | (g6 << 5) | r5
    return ((bgr >> 8) | (bgr << 8)) & 0xFFFF


def from_bgr565_swapped(swapped: int) -> Tuple[int, int, int]:
    """Unpacks ST7789 byte-swapped BGR565 back to 8-bit (R, G, B)."""
    bgr = ((swapped >> 8) | ((swapped & 0xFF) << 8)) & 0xFFFF
    r5 = bgr & 0x1F
    g6 = (bgr >> 5) & 0x3F
    b5 = (bgr >> 11) & 0x1F
    return (r5 << 3, g6 << 2, b5 << 3)


def main() -> int:
    parser = argparse.ArgumentParser(description="Calculate ST7789 byte-swapped BGR565 values.")
    parser.add_argument("--rgb", nargs=3, type=int, metavar=("R", "G", "B"), help="Convert R G B (0..255)")
    args = parser.parse_args()

    if args.rgb:
        r, g, b = args.rgb
        print(f"RGB({r}, {g}, {b}) -> 0x{to_bgr565_swapped(r, g, b):04X}")
        return 0

    presets = [
        ("White", 255, 255, 255),
        ("Black", 0, 0, 0),
        ("BMO Teal", 138, 213, 195),
        ("Classic DMG 0", 152, 188, 8),
        ("Classic DMG 1", 136, 172, 8),
        ("Classic DMG 2", 48, 96, 48),
        ("Classic DMG 3", 8, 56, 8),
    ]
    for name, r, g, b in presets:
        print(f"{name:15s}: 0x{to_bgr565_swapped(r, g, b):04X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
