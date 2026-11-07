"""Path-key helpers shared by firmware deployment and cover generation."""

from __future__ import annotations

import zlib


def normalize_rom_path(relative_path: str) -> str:
    if not relative_path:
        raise ValueError("ROM path must not be empty")
    normalized = relative_path.replace("\\", "/").lstrip("/")
    normalized = normalized.translate(str.maketrans("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz"))
    if not normalized or not normalized.rsplit("/", 1)[-1]:
        raise ValueError("ROM path must include a filename")
    return normalized


def make_rom_key(relative_path: str) -> str:
    """Return a portable key: up to 23 sanitized stem chars plus full-path CRC32."""
    normalized = normalize_rom_path(relative_path)
    filename = normalized.rsplit("/", 1)[-1]
    dot = filename.rfind(".")
    stem = filename[:dot] if dot >= 0 else filename
    prefix_chars: list[str] = []
    previous_underscore = False
    for char in stem:
        lowered = char.lower() if "A" <= char <= "Z" else char
        if ("a" <= lowered <= "z") or ("0" <= lowered <= "9") or lowered == "-":
            prefix_chars.append(lowered)
            previous_underscore = False
        elif prefix_chars and not previous_underscore:
            prefix_chars.append("_")
            previous_underscore = True
        if len(prefix_chars) >= 23:
            break
    prefix = "".join(prefix_chars).rstrip("_") or "rom"
    crc = zlib.crc32(normalized.encode("utf-8")) & 0xFFFFFFFF
    return f"{prefix}_{crc:08x}"
