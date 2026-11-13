"""
scripts/convert_roms.py — Convert user-supplied binary ROM files into 4-byte aligned C .rodata headers.
"""

import argparse
import sys
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPTS_DIR.parent
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

from repo_tools import get_data_dir, sanitize_identifier

ROM_DIR = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "assets" / "roms"


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
    parser = argparse.ArgumentParser(description="Convert user-supplied ROM binaries to C .rodata headers.")
    parser.add_argument("--input", "-i", type=Path, help="Single input ROM binary path")
    parser.add_argument("--output", "-o", type=Path, help="Output .h header path (default: local_baked_rom.h)")
    parser.add_argument("--var", "-v", type=str, default=None, help="C array variable name")
    parser.add_argument("--rom-dir", type=Path, default=None, help="Directory of user-supplied .gb/.gbc ROMs to convert")
    args = parser.parse_args()

    if args.input:
        if not args.input.exists():
            print(f"Input file not found: {args.input}")
            return 1
        out_path = args.output or (ROM_DIR / "local_baked_rom.h")
        out_path.parent.mkdir(parents=True, exist_ok=True)
        var_name = args.var or f"rom_{sanitize_identifier(args.input.stem)}"
        sz = bin_to_header(args.input, out_path, var_name)
        print(f"Saved {out_path} ({sz:,} bytes)")
        return 0

    src_dir = args.rom_dir or (get_data_dir(REPO_ROOT) / "roms")
    if not src_dir.is_dir():
        print(f"No ROM directory at {src_dir}; pass --input <path> or --rom-dir <dir>.")
        return 1

    converted = 0
    for rom_path in sorted(src_dir.iterdir()):
        if not rom_path.is_file() or rom_path.suffix.lower() not in {".gb", ".gbc"}:
            continue
        ident = sanitize_identifier(rom_path.stem)
        out_path = ROM_DIR / f"rom_{ident}.h"
        out_path.parent.mkdir(parents=True, exist_ok=True)
        sz = bin_to_header(rom_path, out_path, f"rom_{ident}")
        print(f"Saved {out_path} ({sz:,} bytes)")
        converted += 1

    print(f"Converted {converted} ROM header(s) from {src_dir}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
