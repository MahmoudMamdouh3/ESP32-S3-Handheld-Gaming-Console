from __future__ import annotations

import os
import re
from pathlib import Path


def get_data_dir(repo_root: Path | None = None) -> Path:
    """Return user-supplied ROM/cover data directory (`BMO_DATA_DIR` or repo root)."""
    raw = os.environ.get("BMO_DATA_DIR", "").strip()
    if raw:
        return Path(raw).expanduser()
    return repo_root if repo_root is not None else Path(__file__).resolve().parents[1]


def sanitize_identifier(value: str) -> str:
    normalized = re.sub(r'[^a-zA-Z0-9]+', '', value).lower()
    return normalized or 'game'


def find_game_roots(repo_root: Path) -> dict[str, Path]:
    candidates = {
        'roms_gb': repo_root / 'roms' / 'gb',
        'roms_gbc': repo_root / 'roms' / 'gbc',
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
