"""Shared helper package re-exporting canonical repository, key, and color utilities."""

from __future__ import annotations

from pathlib import Path

from scripts.color_calc import from_bgr565_swapped, to_bgr565_swapped
from scripts.repo_tools import (
    find_game_roots,
    get_data_dir,
    sanitize_identifier,
    validate_rom_header,
)
from scripts.rom_key import make_rom_key, normalize_rom_path

REPO_ROOT = Path(__file__).resolve().parents[2]


def require_dir(path: Path, label: str = "Directory") -> Path:
    """Validate that a directory exists before reading user-supplied game assets."""
    candidate = Path(path).expanduser()
    if not candidate.is_dir():
        raise FileNotFoundError(f"{label} does not exist or is not a directory: {candidate}")
    return candidate


__all__ = [
    "REPO_ROOT",
    "find_game_roots",
    "from_bgr565_swapped",
    "get_data_dir",
    "make_rom_key",
    "normalize_rom_path",
    "require_dir",
    "sanitize_identifier",
    "to_bgr565_swapped",
    "validate_rom_header",
]
