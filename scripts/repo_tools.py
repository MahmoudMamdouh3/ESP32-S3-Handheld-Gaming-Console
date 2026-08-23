from __future__ import annotations

import re
from pathlib import Path


def sanitize_identifier(value: str) -> str:
    normalized = re.sub(r'[^a-zA-Z0-9]+', '', value).lower()
    return normalized or 'game'


def find_game_roots(repo_root: Path) -> dict[str, Path]:
    candidates = {
        'gameboy_games': repo_root / 'Gameboy games',
        'gameboy_color_games': repo_root / 'Gameboyt color games',
    }
    return {name: path for name, path in candidates.items() if path.exists()}


def validate_rom_header(rom_bytes: bytes) -> tuple[bool, int, int]:
    if len(rom_bytes) <= 0x150:
        return False, 0, 0

    checksum = 0
    for index in range(0x0134, 0x014C + 1):
        checksum = (checksum - rom_bytes[index] - 1) & 0xFF

    expected = rom_bytes[0x014D]
    return checksum == expected, checksum, expected
