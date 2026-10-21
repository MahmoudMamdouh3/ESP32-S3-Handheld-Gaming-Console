"""
scripts/convert_roms.py — Convert binary ROM files into 4-byte aligned C .rodata headers.
"""

import argparse
from pathlib import Path
from typing import List, Tuple

REPO_ROOT = Path(__file__).resolve().parents[1]
ROM_DIR = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "assets" / "roms"

DEFAULT_ROMS: List[Tuple[str, str, str]] = [
    ("Amazing Spider-Man, The (USA, Europe).gb", "rom_amazingspiderman", "rom_spiderman.h"),
    ("Kirby's Dream Land 2 (USA, Europe) (SGB Enhanced).gb", "rom_kirby2", "rom_kirby2.h"),
    ("Legend of Zelda, The - Link's Awakening (USA, Europe) (Rev 2).gb", "rom_zeldala", "rom_zeldala.h"),
    ("Sneaky Snakes (USA, Europe).gb", "rom_sneakysnakes", "rom_sneakysnakes.h"),
    ("Super Mario Land (World) (Rev 1).gb", "rom_supermarioland", "rom_supermarioland.h"),
    ("Tetris 2 (USA, Europe) (SGB Enhanced).gb", "rom_tetris2", "rom_tetris2.h"),
]


def bin_to_header(in_path: Path, out_path: Path, var_name: str) -> int:
    """Converts a binary ROM file into an aligned .rodata C header."""
    data = in_path.read_bytes()
    with out_path.open("w", encoding="utf-8") as out:
        out.write("#pragma once\n#include <stdint.h>\n#include <stddef.h>\n")
        out.write(
            f'const uint8_t {var_name}[] __attribute__((section(".rodata"))) __attribute__((aligned(4))) = {{\n'
        )
        lines = [
            ", ".join(f"0x{b:02X}" for b in data[i : i + 16])
            for i in range(0, len(data), 16)
        ]
        out.write(",\n".join(lines))
        out.write(f"\n}};\nconst size_t {var_name}_size = {len(data)};\n")
    return len(data)


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert ROM binaries to C .rodata headers.")
    parser.add_argument("--input", "-i", type=Path, help="Single input ROM binary path")
    parser.add_argument("--output", "-o", type=Path, help="Output .h header path")
    parser.add_argument("--var", "-v", type=str, help="C array variable name")
    args = parser.parse_args()

    if args.input and args.output and args.var:
        if not args.input.exists():
            print(f"Input file not found: {args.input}")
            return 1
        sz = bin_to_header(args.input, args.output, args.var)
        print(f"Saved {args.output} ({sz:,} bytes)")
        return 0

    for filename, var_name, out_file in DEFAULT_ROMS:
        in_path = ROM_DIR / filename
        out_path = ROM_DIR / out_file
        if not in_path.exists():
            print(f"Skipping {filename}: file not found at {in_path}")
            continue
        sz = bin_to_header(in_path, out_path, var_name)
        print(f"Saved {out_path} ({sz:,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
