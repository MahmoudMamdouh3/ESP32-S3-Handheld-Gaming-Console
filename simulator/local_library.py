#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Read-only local ROM and box-art catalog discovery for the browser simulator."""

from __future__ import annotations

import ctypes
import hashlib
import json
import os
import re
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path

EXTENSION_TO_SYSTEM: dict[str, tuple[str, str]] = {
    ".gb": ("ROM_GB", "GB"),
    ".gbc": ("ROM_GBC", "GBC"),
    ".nes": ("ROM_NES", "NES"),
    ".wad": ("ROM_WAD", "DOOM"),
    ".sms": ("ROM_SMS", "SMS"),
    ".gg": ("ROM_GG", "GG"),
    ".pce": ("ROM_PCE", "PCE"),
    ".a26": ("ROM_ATARI", "A26"),
    ".p8": ("ROM_PICO8", "P8"),
    ".gen": ("ROM_GENESIS", "MD"),
    ".md": ("ROM_GENESIS", "MD"),
    ".smd": ("ROM_GENESIS", "MD"),
    ".sfc": ("ROM_SNES", "SNES"),
    ".smc": ("ROM_SNES", "SNES"),
    ".ws": ("ROM_WSWAN", "WS"),
    ".wsc": ("ROM_WSWAN", "WS"),
    ".ngp": ("ROM_NGP", "NGP"),
    ".ngc": ("ROM_NGP", "NGP"),
    ".lnx": ("ROM_LYNX", "LNX"),
    ".col": ("ROM_COLEM", "COL"),
    ".sg": ("ROM_COLEM", "COL"),
}

IMAGE_MIME_TYPES: dict[str, str] = {
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".bmp": "image/bmp",
}

IGNORED_DIR_NAMES = frozenset({
    "boxart",
    "saves",
    "screenshots",
    "hd",
    "_local_archived_assets",
    "$recycle.bin",
    "system volume information",
})

BMO_INDEX_MAGIC = b"BMOIDX03"
RAW_COVER_BYTES = 64 * 64 * 2
MAX_LOCAL_ROM_BYTES = 32 * 1024 * 1024
_PAREN_OR_BRACKET_RE = re.compile(r"\([^)]*\)|\[[^\]]*\]")
_NON_ALNUM_RE = re.compile(r"[^a-z0-9]+")
_RAW_BGR565_LUT: list[bytes] | None = None


@dataclass(frozen=True)
class LocalLibraryCatalog:
    """Immutable snapshot of read-only local ROMs, matched covers, and JSON payload."""

    manifest_bytes: bytes
    rom_paths: dict[str, Path]
    cover_paths: dict[str, Path]
    sources: tuple[str, ...]
    total_roms: int
    matched_covers: int


def slugify_title(title: str) -> str:
    """Match simulator cover slug convention (`constants.js` / `convert_covers.py`)."""
    stem = Path(title).stem.lower()
    stripped = _PAREN_OR_BRACKET_RE.sub("", stem)
    slug = _NON_ALNUM_RE.sub("_", stripped).strip("_")
    return slug or "cover"


def make_rom_key(relative_path: str) -> str:
    """Return deterministic `<stem23>_<crc32>` matching `scripts/rom_key.py`."""
    normalized = relative_path.replace("\\", "/").lstrip("/").lower()
    filename = normalized.rsplit("/", 1)[-1]
    dot = filename.rfind(".")
    stem = filename[:dot] if dot >= 0 else filename
    prefix_chars: list[str] = []
    prev_under = False
    for ch in stem:
        if ("a" <= ch <= "z") or ("0" <= ch <= "9") or ch == "-":
            prefix_chars.append(ch)
            prev_under = False
        elif prefix_chars and not prev_under:
            prefix_chars.append("_")
            prev_under = True
        if len(prefix_chars) >= 23:
            break
    prefix = "".join(prefix_chars).rstrip("_") or "rom"
    crc = zlib.crc32(normalized.encode("utf-8")) & 0xFFFFFFFF
    return f"{prefix}_{crc:08x}"


def _token_for(prefix: str, key: str) -> str:
    digest = hashlib.sha256(key.encode("utf-8", errors="replace")).hexdigest()[:15]
    return f"{prefix}{digest}"


def _build_bgr565_lut() -> list[bytes]:
    lut: list[bytes] = []
    for val in range(65536):
        r = (((val >> 11) & 0x1F) * 255) // 31
        g = (((val >> 5) & 0x3F) * 255) // 63
        b = ((val & 0x1F) * 255) // 31
        lut.append(bytes((b, g, r)))
    return lut


def decode_raw_bgr565_to_bmp(raw_bytes: bytes) -> bytes:
    """Convert a 64x64 big-endian RGB565 `.raw` cover scan (8,192 B) to 24-bit BMP."""
    global _RAW_BGR565_LUT
    if len(raw_bytes) != RAW_COVER_BYTES:
        raise ValueError(f"Expected {RAW_COVER_BYTES} bytes for 64x64 .raw cover, got {len(raw_bytes)}")
    if _RAW_BGR565_LUT is None:
        _RAW_BGR565_LUT = _build_bgr565_lut()
    lut = _RAW_BGR565_LUT

    width = 64
    height = 64
    data_size = width * height * 3
    header = bytearray(54)
    header[0:2] = b"BM"
    struct.pack_into("<I", header, 2, 54 + data_size)
    struct.pack_into("<I", header, 10, 54)
    struct.pack_into("<I", header, 14, 40)
    struct.pack_into("<i", header, 18, width)
    struct.pack_into("<i", header, 22, height)
    struct.pack_into("<H", header, 26, 1)
    struct.pack_into("<H", header, 28, 24)
    struct.pack_into("<I", header, 34, data_size)

    rows: list[bytes] = [bytes(header)]
    for y in range(height - 1, -1, -1):
        row_off = y * width * 2
        for x in range(width):
            idx = row_off + x * 2
            rows.append(lut[(raw_bytes[idx] << 8) | raw_bytes[idx + 1]])
    return b"".join(rows)


def _discover_windows_gamelib_drives() -> list[tuple[str, Path]]:
    roots: list[tuple[str, Path]] = []
    if os.name != "nt":
        return roots
    windll = getattr(ctypes, "windll", None)
    if windll is None:
        return roots
    kernel32 = windll.kernel32
    drive_mask = int(kernel32.GetLogicalDrives())
    vol_buf = ctypes.create_unicode_buffer(261)
    for idx in range(26):
        if not (drive_mask & (1 << idx)):
            continue
        letter = chr(ord("A") + idx)
        drive_str = f"{letter}:\\"
        drive_type = int(kernel32.GetDriveTypeW(drive_str))
        if drive_type not in (2, 3):
            continue
        vol_buf.value = ""
        has_info = bool(kernel32.GetVolumeInformationW(drive_str, vol_buf, len(vol_buf), None, None, None, None, 0))
        label = vol_buf.value if has_info else ""
        root_path = Path(drive_str)
        if label.casefold() == "gamelib" or (root_path / ".bmo_index").is_file():
            tag = f"{label} ({letter}:)" if label else f"SD ({letter}:)"
            roots.append((tag, root_path))
    return roots


def discover_local_roots(repo_root: Path) -> list[tuple[str, Path]]:
    """Return deduplicated read-only local ROM roots (repo data dirs + mounted GAMELIB SD)."""
    candidates: list[tuple[str, Path]] = []
    env_dir = os.environ.get("BMO_DATA_DIR", "").strip()
    if env_dir:
        p = Path(env_dir).expanduser()
        if p.is_dir():
            candidates.append(("BMO_DATA_DIR", p))
    for sub in ("games", "roms", "sdcard"):
        p = repo_root / sub
        if p.is_dir():
            candidates.append((sub, p))
    candidates.extend(_discover_windows_gamelib_drives())

    seen: set[Path] = set()
    unique: list[tuple[str, Path]] = []
    for label, path in candidates:
        try:
            resolved = path.resolve()
        except OSError:
            continue
        if resolved not in seen and resolved.is_dir():
            seen.add(resolved)
            unique.append((label, resolved))
    return unique


@dataclass
class _CoverIndices:
    hd_by_stem: dict[str, Path]
    hd_by_slug: dict[str, Path]
    sd_by_key: dict[str, Path]
    curated_slugs: set[str]


def _scan_hd_cover_dir(directory: Path, indices: _CoverIndices) -> None:
    if not directory.is_dir():
        return
    try:
        with os.scandir(directory) as it:
            for entry in it:
                if not entry.is_file(follow_symlinks=False):
                    continue
                name = entry.name
                dot = name.rfind(".")
                if dot <= 0:
                    continue
                ext = name[dot:].lower()
                if ext not in IMAGE_MIME_TYPES:
                    continue
                stem = name[:dot]
                p = Path(entry.path)
                indices.hd_by_stem.setdefault(stem.casefold(), p)
                indices.hd_by_slug.setdefault(slugify_title(stem), p)
    except OSError:
        return


def _scan_keyed_cover_dir(directory: Path, indices: _CoverIndices) -> None:
    if not directory.is_dir():
        return
    try:
        with os.scandir(directory) as it:
            for entry in it:
                if not entry.is_file(follow_symlinks=False):
                    continue
                name = entry.name
                dot = name.rfind(".")
                if dot <= 0:
                    continue
                ext = name[dot:].lower()
                if ext not in {".bmp", ".raw", ".png", ".jpg", ".jpeg"}:
                    continue
                key = name[:dot].casefold()
                existing = indices.sd_by_key.get(key)
                if existing is None or (existing.suffix.lower() == ".raw" and ext != ".raw"):
                    indices.sd_by_key[key] = Path(entry.path)
    except OSError:
        return


def _build_cover_indices(repo_root: Path, roots: list[tuple[str, Path]]) -> _CoverIndices:
    indices = _CoverIndices(hd_by_stem={}, hd_by_slug={}, sd_by_key={}, curated_slugs=set())
    curated_dir = repo_root / "simulator" / "covers"
    if curated_dir.is_dir():
        try:
            with os.scandir(curated_dir) as it:
                for entry in it:
                    if entry.is_file(follow_symlinks=False) and entry.name.lower().endswith(".png"):
                        stem = entry.name[:-4].lower()
                        if stem != "virtual_bmo":
                            indices.curated_slugs.add(stem)
        except OSError:
            pass

    hd_dirs: list[Path] = []
    key_dirs: list[Path] = []
    for _, root in roots:
        hd_dirs.extend([root / "boxart" / "hd", root / "roms" / "boxart" / "hd"])
        key_dirs.extend([root / "boxart", root / "roms" / "boxart"])
    hd_dirs.extend([repo_root / "roms" / "boxart" / "hd", repo_root / "sdcard" / "boxart" / "hd"])
    key_dirs.extend([repo_root / "roms" / "boxart", repo_root / "sdcard" / "boxart"])

    seen_dirs: set[Path] = set()
    for hd_dir in hd_dirs:
        if hd_dir in seen_dirs:
            continue
        seen_dirs.add(hd_dir)
        _scan_hd_cover_dir(hd_dir, indices)
    for key_dir in key_dirs:
        if key_dir in seen_dirs:
            continue
        seen_dirs.add(key_dir)
        _scan_keyed_cover_dir(key_dir, indices)
    return indices


def _load_favorites(roots: list[tuple[str, Path]]) -> set[str]:
    favs: set[str] = set()
    for _, root in roots:
        fav_path = root / "favorites.txt"
        if not fav_path.is_file():
            continue
        try:
            for line in fav_path.read_text(encoding="utf-8", errors="ignore").splitlines():
                cleaned = line.strip().replace("\\", "/").lstrip("/")
                if cleaned:
                    favs.add(cleaned.casefold())
                    favs.add(Path(cleaned).name.casefold())
        except OSError:
            continue
    return favs


def _scan_rom_tree(root: Path) -> list[tuple[str, Path]]:
    found: list[tuple[str, Path]] = []
    stack: list[Path] = [root]
    while stack:
        current = stack.pop()
        try:
            with os.scandir(current) as it:
                for entry in it:
                    name = entry.name
                    if not name or name.startswith("."):
                        continue
                    if entry.is_dir(follow_symlinks=False):
                        if name.casefold() not in IGNORED_DIR_NAMES:
                            stack.append(Path(entry.path))
                        continue
                    if not entry.is_file(follow_symlinks=False):
                        continue
                    dot = name.rfind(".")
                    if dot <= 0:
                        continue
                    ext = name[dot:].lower()
                    if ext in EXTENSION_TO_SYSTEM:
                        rel = Path(entry.path).relative_to(root).as_posix()
                        found.append((rel, Path(entry.path)))
        except OSError:
            continue
    return found


def _read_bmo_index_entries(root: Path) -> list[tuple[str, Path, int]] | None:
    index_path = root / ".bmo_index"
    if not index_path.is_file():
        return None
    try:
        data = index_path.read_bytes()
    except OSError:
        return None
    if len(data) < 44 or data[:8] != BMO_INDEX_MAGIC:
        return None
    _, ver, count, entry_size, st_bytes = struct.unpack_from("<8sIIII", data, 0)
    if ver != 3 or entry_size != 20 or len(data) < 44 + count * 20 + st_bytes:
        return None
    st_off = 44 + count * 20
    st = data[st_off : st_off + st_bytes]
    out: list[tuple[str, Path, int]] = []
    for idx in range(count):
        p_off, p_len, _, flags, _, _, _ = struct.unpack_from("<IHBBIII", data, 44 + idx * 20)
        if p_len == 0 or p_off + p_len > len(st):
            continue
        rel = st[p_off : p_off + p_len].decode("utf-8", errors="replace").replace("\\", "/").lstrip("/")
        dot = rel.rfind(".")
        if dot <= 0 or rel[dot:].lower() not in EXTENSION_TO_SYSTEM:
            continue
        out.append((rel, root / rel, flags))
    return out


def _match_cover_path(rel_path: str, stem: str, indices: _CoverIndices) -> Path | None:
    stem_cf = stem.casefold()
    exact = indices.hd_by_stem.get(stem_cf)
    if exact is not None:
        return exact
    ext_folder = Path(rel_path).suffix.lower().lstrip(".")
    candidates = [make_rom_key(rel_path).casefold(), stem_cf]
    if "/" not in rel_path and ext_folder:
        candidates.append(make_rom_key(f"{ext_folder}/{Path(rel_path).name}").casefold())
    for key in candidates:
        matched = indices.sd_by_key.get(key)
        if matched is not None:
            return matched
    return indices.hd_by_slug.get(slugify_title(stem))


@dataclass
class _CatalogAccumulator:
    indices: _CoverIndices
    fav_names: set[str]
    rom_paths: dict[str, Path]
    cover_paths: dict[str, Path]
    cover_path_to_token: dict[Path, str]
    seen_roms: set[tuple[str, str]]
    entries: list[list[object]]
    matched_covers: int = 0

    def register_cover(self, rel_path: str, stem: str) -> str:
        cover_path = _match_cover_path(rel_path, stem, self.indices)
        if cover_path is None:
            return ""
        cover_token = self.cover_path_to_token.get(cover_path, "")
        if not cover_token:
            cover_token = _token_for("c", str(cover_path))
            self.cover_path_to_token[cover_path] = cover_token
            self.cover_paths[cover_token] = cover_path
        self.matched_covers += 1
        return cover_token

    def add_item(self, rel_path: str, full_path: Path, idx_flags: int) -> bool:
        name = Path(rel_path).name
        dot = name.rfind(".")
        if dot <= 0:
            return False
        ext = name[dot:].lower()
        route = EXTENSION_TO_SYSTEM.get(ext)
        if route is None:
            return False
        system_id, _ = route
        dedup_key = (system_id, name.casefold())
        if dedup_key in self.seen_roms:
            return False
        self.seen_roms.add(dedup_key)

        stem = name[:dot]
        rom_token = _token_for("r", f"{system_id}:{rel_path}:{name}")
        self.rom_paths[rom_token] = full_path
        cover_token = self.register_cover(rel_path, stem)
        is_fav = bool(idx_flags & 0x01) or rel_path.casefold() in self.fav_names or name.casefold() in self.fav_names
        is_pop = bool(idx_flags & 0x04) or (slugify_title(stem) in self.indices.curated_slugs)
        flags = (1 if is_fav else 0) | (2 if cover_token else 0) | (4 if is_pop else 0)
        self.entries.append([system_id, stem, ext, rom_token, cover_token, flags])
        return True


def build_local_library_catalog(repo_root: Path) -> LocalLibraryCatalog:
    """Scan repo-local data folders and attached GAMELIB SD cards cleanly (read-only)."""
    roots = discover_local_roots(repo_root)
    acc = _CatalogAccumulator(
        indices=_build_cover_indices(repo_root, roots),
        fav_names=_load_favorites(roots),
        rom_paths={},
        cover_paths={},
        cover_path_to_token={},
        seen_roms=set(),
        entries=[],
    )
    active_sources: list[str] = []
    for label, root in roots:
        index_items = _read_bmo_index_entries(root)
        raw_items = index_items if index_items is not None else [(r, p, 0) for r, p in _scan_rom_tree(root)]
        added_here = sum(1 for rel_path, full_path, flags in raw_items if acc.add_item(rel_path, full_path, flags))
        if added_here > 0:
            active_sources.append(label)

    acc.entries.sort(key=lambda item: (str(item[0]), str(item[1]).casefold()))
    payload = {
        "version": 1,
        "sources": active_sources,
        "totalRoms": len(acc.entries),
        "matchedCovers": acc.matched_covers,
        "entries": acc.entries,
    }
    manifest_bytes = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    return LocalLibraryCatalog(
        manifest_bytes=manifest_bytes,
        rom_paths=acc.rom_paths,
        cover_paths=acc.cover_paths,
        sources=tuple(active_sources),
        total_roms=len(acc.entries),
        matched_covers=acc.matched_covers,
    )
