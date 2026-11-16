# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Canonical BMO Handheld SD card deployer & binary ROM index builder (BMOIDX03)."""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import os
import shutil
import struct
import tempfile
import time
import warnings
import zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from scripts.repo_tools import get_data_dir
    from scripts.rom_key import make_rom_key
else:
    try:
        from scripts.repo_tools import get_data_dir
        from scripts.rom_key import make_rom_key
    except ImportError:
        from repo_tools import get_data_dir
        from rom_key import make_rom_key

REPO_ROOT = Path(__file__).resolve().parents[1]
DEST_ROOT: Path | None = None

ROM_EXTENSIONS = frozenset({
    ".gb", ".gbc", ".nes", ".wad", ".sms", ".gg", ".pce", ".a26",
    ".p8", ".gen", ".md", ".smd", ".sfc", ".smc", ".ws",
    ".wsc", ".ngp", ".ngc", ".lnx", ".col", ".sg",
})
BOXART_EXTENSIONS = frozenset({".raw", ".bmp"})
BUFFER_SIZE = 1024 * 1024
EXPECTED_VOLUME_LABEL = "GAMELIB"

# Matches firmware RomType enum values in firmware/BmoGameboy/src/storage/sd_card.h.
EXT_MAP: dict[str, tuple[str, int]] = {
    ".gb": ("gb", 2),       # ROM_GB = 2
    ".gbc": ("gbc", 3),     # ROM_GBC = 3
    ".nes": ("nes", 4),     # ROM_NES = 4
    ".wad": ("wad", 5),     # ROM_WAD = 5
    ".sms": ("sms", 6),     # ROM_SMS = 6
    ".gg": ("gg", 7),       # ROM_GG = 7
    ".pce": ("pce", 8),     # ROM_PCE = 8
    ".a26": ("a26", 9),     # ROM_ATARI = 9
    ".p8": ("p8", 10),      # ROM_PICO8 = 10
    ".gen": ("gen", 11),    # ROM_GENESIS = 11
    ".md": ("gen", 11),
    ".smd": ("gen", 11),
    ".sfc": ("snes", 12),   # ROM_SNES = 12
    ".smc": ("snes", 12),
    ".ws": ("ws", 13),      # ROM_WSWAN = 13
    ".wsc": ("wsc", 13),
    ".ngp": ("ngc", 14),    # ROM_NGP = 14
    ".ngc": ("ngc", 14),
    ".lnx": ("lnx", 15),    # ROM_LYNX = 15
    ".col": ("col", 16),    # ROM_COLEM = 16
    ".sg": ("sg", 16),
}


def sha256_file(path: Path) -> bytes:
    """Return raw SHA-256 digest bytes for a file."""
    digest = hashlib.sha256()
    buf = bytearray(min(BUFFER_SIZE, max(65536, path.stat().st_size or 65536)))
    mv = memoryview(buf)
    with path.open("rb") as source:
        while n := source.readinto(buf):
            digest.update(mv[:n])
    return digest.digest()


def compute_prefix_crc32(path: Path, max_bytes: int = 4096) -> int:
    """IEEE CRC32 over up to the first 4 KiB of a ROM file, matching firmware verifyRomIdentity."""
    with path.open("rb") as f:
        chunk = f.read(max_bytes)
    return (zlib.crc32(chunk) & 0xFFFFFFFF) if chunk else 0


def catalog_fingerprint(
    entry_count: int, string_table_bytes: int, total_bytes: int, root_stamp: int, payload_crc: int
) -> int:
    """Matches BmoIndex::catalogFingerprint in firmware/BmoGameboy/src/storage/rom_index.h."""
    words = struct.pack(
        "<IIIIII",
        entry_count & 0xFFFFFFFF,
        string_table_bytes & 0xFFFFFFFF,
        total_bytes & 0xFFFFFFFF,
        (total_bytes >> 32) & 0xFFFFFFFF,
        root_stamp & 0xFFFFFFFF,
        payload_crc & 0xFFFFFFFF,
    )
    return zlib.crc32(words) & 0xFFFFFFFF


def validate_target_collisions(tasks: list[tuple[Path, Path]], dest_root: Path) -> None:
    """Raise ValueError on case-insensitive destination path collisions."""
    targets: set[str] = set()
    for _, destination in tasks:
        key = str(destination.relative_to(dest_root)).casefold()
        if key in targets:
            raise ValueError(f"Case-insensitive destination filename collision: {destination.name}")
        targets.add(key)


def _plan_boxart_tasks(
    boxart: list[Path],
    games: list[Path],
    relative_paths: dict[Path, str],
    dest_root: Path,
) -> list[tuple[Path, Path]]:
    games_by_stem: dict[str, list[Path]] = {}
    for game in games:
        games_by_stem.setdefault(game.stem.casefold(), []).append(game)

    tasks: list[tuple[Path, Path]] = []
    keyed_targets: set[str] = set()
    ambiguous_art: list[str] = []
    for art_path in boxart:
        matching_games = games_by_stem.get(art_path.stem.casefold(), [])
        if len(matching_games) > 1:
            ambiguous_art.append(art_path.name)
            continue
        if len(matching_games) == 1:
            target_name = f"{make_rom_key(relative_paths[matching_games[0]])}{art_path.suffix.lower()}"
            destination = dest_root / "boxart" / target_name
        else:
            destination = dest_root / "boxart" / art_path.name
        target_key = str(destination.relative_to(dest_root)).casefold()
        if target_key not in keyed_targets:
            keyed_targets.add(target_key)
            tasks.append((art_path, destination))

    if ambiguous_art:
        warnings.warn(
            f"Skipped {len(ambiguous_art)} box-art files with ambiguous ROM basenames.",
            RuntimeWarning,
            stacklevel=3,
        )
    return tasks


def deployment_tasks(src_games: Path, src_boxart: Path, dest_root: Path) -> list[tuple[Path, Path]]:
    """Build verified mirror deployment task pairs across nested games and box art."""
    for directory, label in ((src_games, "Game source directory"), (src_boxart, "Box-art source directory"), (dest_root, "Destination")):
        if not directory.is_dir():
            raise FileNotFoundError(f"{label} not found: {directory}")

    games = sorted(path for path in src_games.rglob("*") if path.is_file() and path.suffix.lower() in ROM_EXTENSIONS)
    boxart = sorted(path for path in src_boxart.iterdir() if path.is_file() and path.suffix.lower() in BOXART_EXTENSIONS)
    relative_paths = {path: path.relative_to(src_games).as_posix() for path in games}
    for relative_path in relative_paths.values():
        if len(relative_path.encode("utf-8")) > 255:
            raise ValueError(f"ROM path is too long for firmware index: {relative_path}")

    tasks = [(path, dest_root / Path(relative_paths[path])) for path in games]
    tasks.extend(_plan_boxart_tasks(boxart, games, relative_paths, dest_root))
    validate_target_collisions(tasks, dest_root)
    return tasks


def _inspect_plan_task(pair: tuple[Path, Path]) -> tuple[bool, int]:
    src, dst = pair
    if dst.is_symlink() or (dst.exists() and not dst.is_file()):
        raise OSError(f"Destination exists and is not a regular file: {dst}")
    src_size = src.stat().st_size
    if dst.is_file() and dst.stat().st_size == src_size and sha256_file(dst) == sha256_file(src):
        return (False, src_size)
    return (True, src_size)


def deployment_plan(
    tasks: list[tuple[Path, Path]], report_progress: bool = False
) -> tuple[list[tuple[Path, Path]], int, int]:
    """Compare planned file digests against destination to compute incremental copy list."""
    updates: list[tuple[Path, Path]] = []
    unchanged = 0
    update_bytes = 0
    if len(tasks) >= 16:
        workers = min(8, os.cpu_count() or 4)
        with ThreadPoolExecutor(max_workers=workers) as pool:
            inspected = list(pool.map(_inspect_plan_task, tasks, chunksize=8))
    else:
        inspected = [_inspect_plan_task(pair) for pair in tasks]

    for index, ((src, dst), (needs_update, src_size)) in enumerate(zip(tasks, inspected, strict=True), start=1):
        if needs_update:
            updates.append((src, dst))
            update_bytes += src_size
        else:
            unchanged += 1
        if report_progress and (index % 500 == 0 or index == len(tasks)):
            print(f"Compared {index:,}/{len(tasks):,} files.", flush=True)
    return updates, unchanged, update_bytes


def verify_volume_label(root: Path, expected_label: str) -> None:
    """Verify Windows SD card root anchor and FAT volume label before writes."""
    if os.name != "nt":
        raise OSError("SD deployment is supported only on Windows.")
    resolved_root = root.resolve()
    drive_root = resolved_root.anchor
    if resolved_root != Path(drive_root):
        raise ValueError(f"Destination must be the SD-card root, not a subdirectory: {root}")
    volume_name = ctypes.create_unicode_buffer(261)
    if not ctypes.windll.kernel32.GetVolumeInformationW(drive_root, volume_name, len(volume_name), None, None, None, None, 0):
        raise ctypes.WinError()
    if volume_name.value.casefold() != expected_label.casefold():
        raise ValueError(f"Refusing to write to {drive_root}: expected volume label {expected_label!r}, found {volume_name.value!r}.")


def verify_destination_root(root: Path) -> None:
    """Reject writing into subdirectories when targeting Windows drives."""
    if os.name == "nt" and root.resolve() != Path(root.resolve().anchor):
        raise ValueError(f"Destination must be the SD-card root, not a subdirectory: {root}")


def copy_single_file(src: Path, dst: Path) -> tuple[bool, int]:
    """Atomically stage, SHA-256 verify, and replace a single target file in one source read pass."""
    size = src.stat().st_size
    if dst.is_symlink() or (dst.exists() and not dst.is_file()):
        raise OSError(f"Destination exists and is not a regular file: {dst}")
    if dst.is_file() and dst.stat().st_size == size and sha256_file(dst) == sha256_file(src):
        return False, size

    dst.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    digest = hashlib.sha256()
    buf = bytearray(min(BUFFER_SIZE, max(65536, size or 65536)))
    mv = memoryview(buf)
    try:
        with tempfile.NamedTemporaryFile(mode="wb", prefix=".bmo-copy-", suffix=".tmp", dir=dst.parent, delete=False) as staged:
            temporary_path = Path(staged.name)
            with src.open("rb") as source:
                while n := source.readinto(buf):
                    digest.update(mv[:n])
                    staged.write(mv[:n])
            staged.flush()
            os.fsync(staged.fileno())
        source_digest = digest.digest()
        if temporary_path.stat().st_size != size or sha256_file(temporary_path) != source_digest:
            raise OSError(f"Staged file verification failed: {src}")
        os.replace(temporary_path, dst)
        temporary_path = None
        return True, size
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def _execute_mirror_updates(updates: list[tuple[Path, Path]]) -> int:
    copied = skipped = errors = copied_bytes = 0
    results: list[tuple[bool, int, bool | str]]
    if len(updates) >= 4:
        workers = min(6, os.cpu_count() or 4)
        with ThreadPoolExecutor(max_workers=workers) as pool:
            results = list(pool.map(_copy_worker, updates, chunksize=8))
    else:
        results = [_copy_worker(pair) for pair in updates]

    for index, ((src, _), (ok, size, written)) in enumerate(zip(updates, results, strict=True), start=1):
        if not ok:
            errors += 1
            print(f"[ERROR] {src.name}: {written}")
        elif written:
            copied += 1
            copied_bytes += size
        else:
            skipped += 1
        if index % 500 == 0 or index == len(updates):
            print(f"Processed {index:,}/{len(updates):,}: {copied:,} copied, {skipped:,} verified unchanged, {errors:,} errors.", flush=True)
    print(f"Finished: copied {copied:,} files ({copied_bytes:,} bytes), verified unchanged {skipped:,}, errors {errors:,}.")
    return 1 if errors else 0


def deploy(
    dest_root: Path,
    apply: bool = False,
    expected_label: str = EXPECTED_VOLUME_LABEL,
    src_games: Path | None = None,
    src_boxart: Path | None = None,
) -> int:
    """Safely mirror user games and keyed covers onto a verified SD card root."""
    data_dir = get_data_dir(REPO_ROOT)
    games_dir = src_games if src_games is not None else (data_dir / "games")
    boxart_dir = src_boxart if src_boxart is not None else (data_dir / "roms" / "boxart")
    verify_destination_root(dest_root)
    tasks = deployment_tasks(games_dir, boxart_dir, dest_root)
    updates, unchanged, update_bytes = deployment_plan(tasks, report_progress=True)
    missing_count = sum(not dst.exists() for _, dst in updates)
    print(f"Would copy {missing_count:,} missing, replace {len(updates) - missing_count:,}; {unchanged:,} unchanged ({update_bytes:,} B needed).")
    if not apply:
        print("Dry run only. Re-run with --apply to write to the specified SD card.")
        return 0

    verify_volume_label(dest_root, expected_label)
    if shutil.disk_usage(dest_root).free < update_bytes:
        raise OSError(f"Insufficient free space for {update_bytes:,} bytes on {dest_root}")
    if not updates:
        print("No files need updating; the SD card was not changed.")
        return 0
    for directory in (dest_root / "boxart", dest_root / "saves", dest_root / "screenshots"):
        directory.mkdir(parents=True, exist_ok=True)
    index_path = dest_root / ".bmo_index"
    if index_path.exists():
        index_path.unlink()
    return _execute_mirror_updates(updates)


def _normalize_index_games(
    deployed_games: list[tuple[str, int, int] | tuple[str, int, int, int]],
) -> list[tuple[str, int, int, int]]:
    normalized: list[tuple[str, int, int, int]] = []
    for item in deployed_games:
        if len(item) == 3:
            normalized.append((item[0], item[1], item[2], 0))
        elif len(item) == 4:
            normalized.append((item[0], item[1], item[2], item[3]))
        else:
            raise ValueError(f"Unexpected deployed game tuple shape: {item!r}")
    return sorted(normalized, key=lambda row: (row[0].split("/")[-1].lower(), row[0].lower()))


def generate_binary_index(
    deployed_games: list[tuple[str, int, int] | tuple[str, int, int, int]],
    boxart_keys: set[str],
    dest_root: Path | None = None,
) -> None:
    """Write atomic BMOIDX03 v3 binary catalog index (/.bmo_index)."""
    sorted_games = _normalize_index_games(deployed_games)
    entries_bytes = bytearray()
    string_table = bytearray()
    total_file_bytes = 0
    for rel_path, rom_type, fsize, crc_prefix in sorted_games:
        fname_bytes = rel_path.encode("utf-8")
        if len(fname_bytes) == 0 or len(fname_bytes) > 255:
            raise ValueError(f"ROM path must be 1..255 UTF-8 bytes: {rel_path}")
        path_offset = len(string_table)
        string_table.extend(fname_bytes)
        string_table.append(0)
        flags = 0x02 if make_rom_key(rel_path).lower() in boxart_keys else 0x00
        entries_bytes.extend(struct.pack("<IHBBIII", path_offset, len(fname_bytes), rom_type, flags, fsize, crc_prefix & 0xFFFFFFFF, 0))
        total_file_bytes += fsize

    payload = bytes(entries_bytes) + bytes(string_table)
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    fp = catalog_fingerprint(len(sorted_games), len(string_table), total_file_bytes, 0, crc)
    header = struct.pack(
        "<8s9I", b"BMOIDX03", 3, len(sorted_games), 20, len(string_table), crc, 0,
        total_file_bytes & 0xFFFFFFFF, (total_file_bytes >> 32) & 0xFFFFFFFF, fp,
    )
    target_root = dest_root if dest_root is not None else DEST_ROOT
    if target_root is None:
        raise ValueError("Destination root is required (--destination).")
    temp_path = target_root / ".bmo_index.tmp"
    with temp_path.open("wb") as f:
        f.write(header)
        f.write(payload)
        f.flush()
        os.fsync(f.fileno())
    os.replace(temp_path, target_root / ".bmo_index")


def _copy_worker(pair: tuple[Path, Path]) -> tuple[bool, int, bool | str]:
    src, dst = pair
    try:
        written, size = copy_single_file(src, dst)
        return (True, size, written)
    except Exception as exc:
        return (False, 0, str(exc))


def _copy_batch_parallel(tasks: list[tuple[Path, Path]], max_workers: int, label: str) -> None:
    if not tasks:
        return
    t0 = time.time()
    copied_bytes = 0
    with ThreadPoolExecutor(max_workers=max_workers) as pool:
        for idx, (ok, sz, written) in enumerate(pool.map(_copy_worker, tasks, chunksize=16)):
            if not ok:
                raise RuntimeError(f"{label} copy failed: {written}")
            if written:
                copied_bytes += sz
            if (idx + 1) % 500 == 0 or idx + 1 == len(tasks):
                elapsed = max(time.time() - t0, 0.1)
                print(f"  [{((idx + 1) / len(tasks)) * 100:5.1f}%] {idx + 1:,}/{len(tasks):,} {label} | {(copied_bytes / (1024 * 1024)) / elapsed:.2f} MB/s", flush=True)


def _plan_complete_tasks(
    games_dir: Path, boxart_dir: Path, dest_root: Path
) -> tuple[list[tuple[Path, Path]], list[tuple[Path, Path]], list[tuple[str, int, int] | tuple[str, int, int, int]]]:
    src_games_list = [p for p in games_dir.iterdir() if p.is_file()] if games_dir.exists() else []
    src_raw_by_stem = {sf.stem.lower(): sf for sf in (boxart_dir.glob("*.raw") if boxart_dir.exists() else []) if sf.is_file()}
    game_tasks: list[tuple[Path, Path]] = []
    boxart_tasks: list[tuple[Path, Path]] = []
    deployed_meta: list[tuple[str, int, int] | tuple[str, int, int, int]] = []
    dest_boxart = dest_root / "boxart"

    for sg in src_games_list:
        mapping = EXT_MAP.get(sg.suffix.lower())
        if not mapping:
            continue
        folder, rom_type = mapping
        rel_path = f"{folder}/{sg.name}"
        if len(rel_path.encode("utf-8")) > 255:
            raise ValueError(f"ROM path exceeds firmware 255-byte limit: {rel_path}")
        deployed_meta.append((rel_path, rom_type, sg.stat().st_size, compute_prefix_crc32(sg)))
        game_tasks.append((sg, dest_root / folder / sg.name))
        cover_src = src_raw_by_stem.get(sg.stem.lower())
        if cover_src:
            boxart_tasks.append((cover_src, dest_boxart / f"{make_rom_key(rel_path)}.raw"))
    return game_tasks, boxart_tasks, deployed_meta


def deploy_complete(dest_root: Path, src_games: Path | None = None, src_boxart: Path | None = None) -> int:
    """Deploy flat source ROMs partitioned into console subfolders plus /.bmo_index."""
    if not dest_root.exists():
        print(f"[ERROR] SD Card at {dest_root} not accessible!")
        return 1
    data_dir = get_data_dir(REPO_ROOT)
    games_dir = src_games if src_games is not None else (data_dir / "games")
    boxart_dir = src_boxart if src_boxart is not None else (data_dir / "roms" / "boxart")
    dest_boxart = dest_root / "boxart"
    for d in (dest_boxart, dest_root / "saves", dest_root / "screenshots"):
        d.mkdir(parents=True, exist_ok=True)

    game_tasks, boxart_tasks, deployed_meta = _plan_complete_tasks(games_dir, boxart_dir, dest_root)
    validate_target_collisions(game_tasks + boxart_tasks, dest_root)
    _copy_batch_parallel(boxart_tasks, 4, "boxart")
    existing_boxarts = {f.stem.lower() for f in dest_boxart.glob("*.raw")}
    _copy_batch_parallel(game_tasks, 6, "games")
    generate_binary_index(deployed_meta, existing_boxarts, dest_root=dest_root)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, required=True, help="Existing SD-card root path (for example, F:\\).")
    parser.add_argument("--mode", choices=("mirror", "partitioned"), default="mirror")
    parser.add_argument("--apply", action="store_true", help="Write verified files in mirror mode (dry run otherwise).")
    parser.add_argument("--expected-label", default=EXPECTED_VOLUME_LABEL)
    parser.add_argument("--games-dir", type=Path, default=None)
    parser.add_argument("--boxart-dir", type=Path, default=None)
    args = parser.parse_args(argv)
    if args.mode == "partitioned":
        return deploy_complete(args.destination, src_games=args.games_dir, src_boxart=args.boxart_dir)
    return deploy(args.destination, apply=args.apply, expected_label=args.expected_label, src_games=args.games_dir, src_boxart=args.boxart_dir)


if __name__ == "__main__":
    raise SystemExit(main())
