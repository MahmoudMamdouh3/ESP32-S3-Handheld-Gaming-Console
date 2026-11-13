"""
scripts/test_runner.py — Fast Baked ROM Header Integrity & Checksum Verifier
"""

import re
from pathlib import Path
from typing import List, Tuple

REPO_ROOT = Path(__file__).resolve().parents[1]
BAKED_ROM_HEADER = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "storage" / "local_baked_rom.h"


def verify_c_rom_header(header_path: Path) -> Tuple[bool, int, int]:
    """Reads the first 4KB of a baked ROM C header and verifies the GB/GBC header checksum (0x0134..0x014D)."""
    with header_path.open("r", encoding="utf-8", errors="ignore") as f:
        head_chunk = f.read(16384)

    hex_tokens = re.findall(r"0x([0-9a-fA-F]{2})", head_chunk)
    if len(hex_tokens) <= 0x0150:
        return False, 0, 0

    rom_bytes = [int(h, 16) for h in hex_tokens[:0x0155]]
    checksum = 0
    for addr in range(0x0134, 0x014C + 1):
        checksum = (checksum - rom_bytes[addr] - 1) & 0xFF

    expected = rom_bytes[0x014D]
    return checksum == expected, checksum, expected


def verify_all_baked_roms() -> List[Tuple[str, bool, int, int]]:
    results: List[Tuple[str, bool, int, int]] = []
    if not BAKED_ROM_HEADER.exists():
        return results
    ok, actual, expected = verify_c_rom_header(BAKED_ROM_HEADER)
    results.append((BAKED_ROM_HEADER.name, ok, actual, expected))
    return results


def main() -> int:
    results = verify_all_baked_roms()
    if not results:
        print("No baked ROM headers present (BMO_ENABLE_BAKED_ROMS=0 default).")
        return 0

    failures = 0
    for name, ok, actual, expected in results:
        status = "PASSED" if ok else "FAILED"
        print(f"{name:20s} | Checksum: 0x{actual:02X} (Expected: 0x{expected:02X}) -> {status}")
        if not ok:
            failures += 1

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
