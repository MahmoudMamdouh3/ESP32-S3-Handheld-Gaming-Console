"""
scripts/deploy_sd_complete.py — Local BMO Handheld SD Card Deployer & Indexer
Structures user-supplied games and converted box art onto a FAT32 SD card root,
bypassing single-directory FAT32 limits by console partitioning, and writes a
.bmo_index binary cache matching firmware/BmoGameboy/src/core/rom_index.h.
"""

import argparse
import os
import sys
import time
import zlib
import struct
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.deploy_to_sd import copy_single_file as atomic_copy_single_file, validate_target_collisions
from scripts.repo_tools import get_data_dir
from scripts.rom_key import make_rom_key

SRC_GAMES = get_data_dir(REPO_ROOT) / "games"
SRC_BOXART = get_data_dir(REPO_ROOT) / "roms" / "boxart"
DEST_ROOT: Path | None = None
DEST_BOXART: Path | None = None
DEST_SAVES: Path | None = None
DEST_SHOTS: Path | None = None

# Matches firmware RomType enum values in firmware/BmoGameboy/src/core/sd_card.h.
EXT_MAP = {
    '.gb': ('gb', 2),       # ROM_GB = 2
    '.gbc': ('gbc', 3),     # ROM_GBC = 3
    '.nes': ('nes', 4),     # ROM_NES = 4
    '.wad': ('wad', 5),     # ROM_WAD = 5
    '.sms': ('sms', 6),     # ROM_SMS = 6
    '.gg': ('gg', 7),       # ROM_GG = 7
    '.pce': ('pce', 8),     # ROM_PCE = 8
    '.a26': ('a26', 9),     # ROM_ATARI = 9
    '.p8': ('p8', 10),      # ROM_PICO8 = 10
    '.gen': ('gen', 11),    # ROM_GENESIS = 11
    '.md': ('gen', 11),
    '.smd': ('gen', 11),
    '.sfc': ('snes', 12),   # ROM_SNES = 12
    '.smc': ('snes', 12),
    '.ws': ('ws', 13),      # ROM_WSWAN = 13
    '.wsc': ('wsc', 13),
    '.ngp': ('ngc', 14),    # ROM_NGP = 14
    '.ngc': ('ngc', 14),
    '.lnx': ('lnx', 15),    # ROM_LYNX = 15
    '.col': ('col', 16),    # ROM_COLEM = 16
    '.sg': ('sg', 16),
}

def copy_single_file(args):
    src, dst = args
    try:
        written, size = atomic_copy_single_file(src, dst)
        return (True, size, written)
    except Exception as e:
        return (False, 0, str(e))


def catalog_fingerprint(entry_count: int, total_bytes: int, root_stamp: int, entries_crc: int) -> int:
    """Matches BmoIndex::catalogFingerprint in firmware/BmoGameboy/src/core/rom_index.h."""
    words = struct.pack(
        "<IIIII",
        entry_count & 0xFFFFFFFF,
        total_bytes & 0xFFFFFFFF,
        (total_bytes >> 32) & 0xFFFFFFFF,
        root_stamp & 0xFFFFFFFF,
        entries_crc & 0xFFFFFFFF,
    )
    return zlib.crc32(words) & 0xFFFFFFFF


def generate_binary_index(deployed_games, boxart_keys, dest_root: Path | None = None):
    """
    Constructs a BMO Binary ROM Index v2 payload ("BMOIDX02").
    Header (36 bytes) + N * Entry (72 bytes). Writes timestamp=0 because the
    host OS cannot precompute ESP32 FATFS rootDirectoryStamp(), so the console
    safely stamps or refreshes on mount.
    """
    print("\n[*] Generating pre-compiled BMO v2 index (/.bmo_index)...")

    def sort_key(item):
        rel_path, rom_type, fsize = item
        base = rel_path.split('/')[-1].lower()
        return (base, rel_path.lower())

    sorted_games = sorted(deployed_games, key=sort_key)

    entries_bytes = bytearray()
    total_file_bytes = 0
    for rel_path, rom_type, fsize in sorted_games:
        fname_bytes = rel_path.encode('utf-8')
        if len(fname_bytes) > 63:
            raise ValueError(f"ROM path exceeds firmware 63-byte limit: {rel_path}")
        fname_padded = fname_bytes.ljust(64, b'\x00')
        has_boxart = 1 if make_rom_key(rel_path).lower() in boxart_keys else 0
        is_favorite = 0
        reserved = 0
        entry = struct.pack('<64sBBBBI', fname_padded, rom_type, is_favorite, has_boxart, reserved, fsize)
        entries_bytes.extend(entry)
        total_file_bytes += fsize

    crc = zlib.crc32(entries_bytes) & 0xFFFFFFFF
    magic = b"BMOIDX02"
    version = 2
    entry_count = len(sorted_games)
    entry_size = 72
    timestamp = 0
    res0 = total_file_bytes & 0xFFFFFFFF
    res1 = catalog_fingerprint(entry_count, total_file_bytes, timestamp, crc)

    header = struct.pack('<8s7I', magic, version, entry_count, entry_size, crc, timestamp, res0, res1)

    target_root = dest_root if dest_root is not None else DEST_ROOT
    if target_root is None:
        raise ValueError("Destination root is required (--destination).")

    index_path = target_root / ".bmo_index"
    temp_path = target_root / ".bmo_index.tmp"
    with open(temp_path, 'wb') as f:
        f.write(header)
        f.write(entries_bytes)
        f.flush()
        os.fsync(f.fileno())
    os.replace(temp_path, index_path)

    print(f"[*] Pre-compiled BMOIDX02 index written to {index_path} ({len(header) + len(entries_bytes):,} bytes, {entry_count:,} ROMs, CRC: {hex(crc)})")

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Deploy user-supplied ROMs and covers to an SD card root and build /.bmo_index.")
    parser.add_argument("--destination", type=Path, required=True, help="Target SD card root path (e.g. F:\\)")
    parser.add_argument("--games-dir", type=Path, default=None, help="Source ROM directory (default: BMO_DATA_DIR/games)")
    parser.add_argument("--boxart-dir", type=Path, default=None, help="Source box-art directory (default: BMO_DATA_DIR/roms/boxart)")
    args = parser.parse_args(argv)

    data_dir = get_data_dir(REPO_ROOT)
    src_games_dir = args.games_dir if args.games_dir is not None else (data_dir / "games")
    src_boxart_dir = args.boxart_dir if args.boxart_dir is not None else (data_dir / "roms" / "boxart")
    dest_root = args.destination
    dest_boxart = dest_root / "boxart"
    dest_saves = dest_root / "saves"
    dest_shots = dest_root / "screenshots"

    print("==================================================================")
    print("[*] COMPLETE BMO HANDHELD SD CARD DEPLOYMENT & VERIFICATION")
    print(f"Source Games:   {src_games_dir}")
    print(f"Source Boxart:  {src_boxart_dir}")
    print(f"Destination:    {dest_root}")
    print("==================================================================")

    if not dest_root.exists():
        print(f"[ERROR] SD Card at {dest_root} not accessible!")
        return 1

    dest_boxart.mkdir(parents=True, exist_ok=True)
    dest_saves.mkdir(parents=True, exist_ok=True)
    dest_shots.mkdir(parents=True, exist_ok=True)

    # 1. Games Selection & Box Art Keying (strictly .raw keyed files + validated lengths)
    print("\n[*] Phase 1: Planning Games & Keyed 64x64 Box Art (.raw)...")
    src_games = [p for p in src_games_dir.iterdir() if p.is_file()] if src_games_dir.exists() else []
    src_raw_by_stem = {
        sf.stem.lower(): sf
        for sf in (src_boxart_dir.glob("*.raw") if src_boxart_dir.exists() else [])
        if sf.is_file()
    }
    print(f"[*] Total source games: {len(src_games):,}; master covers: {len(src_raw_by_stem):,}")

    game_tasks = []
    boxart_tasks = []
    deployed_games_meta = []

    for sg in src_games:
        ext = sg.suffix.lower()
        mapping = EXT_MAP.get(ext)
        if not mapping:
            continue
        folder, rom_type = mapping
        rel_path = f"{folder}/{sg.name}"
        if len(rel_path.encode('utf-8')) > 63:
            raise ValueError(f"ROM path exceeds firmware 63-byte limit: {rel_path}")
        df = dest_root / folder / sg.name
        deployed_games_meta.append((rel_path, rom_type, sg.stat().st_size))
        game_tasks.append((sg, df))
        cover_src = src_raw_by_stem.get(sg.stem.lower())
        if cover_src:
            boxart_tasks.append((cover_src, dest_boxart / f"{make_rom_key(rel_path)}.raw"))

    validate_target_collisions(game_tasks + boxart_tasks, dest_root)

    print(f"[*] Box art tasks queued: {len(boxart_tasks):,}")
    if boxart_tasks:
        with ThreadPoolExecutor(max_workers=4) as pool:
            for idx, (ok, sz, written) in enumerate(pool.map(copy_single_file, boxart_tasks, chunksize=32)):
                if not ok:
                    raise RuntimeError(f"Box art copy failed: {written}")
                if (idx + 1) % 1000 == 0 or idx + 1 == len(boxart_tasks):
                    print(f"  Boxart progress: {idx + 1:,}/{len(boxart_tasks):,}")

    existing_boxarts = {f.stem.lower() for f in dest_boxart.glob("*.raw")}
    print(f"[*] Box Art ready on SD Card: {len(existing_boxarts):,} covers active.")

    print(f"[*] Synchronizing {len(game_tasks):,} verified game files...")
    if game_tasks:
        t0 = time.time()
        copied_bytes = 0
        with ThreadPoolExecutor(max_workers=6) as pool:
            for idx, (ok, sz, written) in enumerate(pool.map(copy_single_file, game_tasks, chunksize=16)):
                if not ok:
                    raise RuntimeError(f"Game copy failed: {written}")
                if written:
                    copied_bytes += sz
                if (idx + 1) % 500 == 0 or idx + 1 == len(game_tasks):
                    elapsed = max(time.time() - t0, 0.1)
                    speed = (copied_bytes / (1024 * 1024)) / elapsed
                    pct = ((idx + 1) / len(game_tasks)) * 100
                    print(f"  [{pct:5.1f}%] {idx + 1:,}/{len(game_tasks):,} games verified | Speed: {speed:.2f} MB/s | Elapsed: {int(elapsed)}s", flush=True)

    # 3. Generate Binary Index
    generate_binary_index(deployed_games_meta, existing_boxarts, dest_root=dest_root)

    # 4. Final Audit
    print("\n==================================================================")
    print("DEPLOYMENT & PLAYABILITY AUDIT:")
    total_sd_games = 0
    for folder, _ in sorted(set(EXT_MAP.values())):
        cdir = dest_root / folder
        cnt = len(list(cdir.iterdir())) if cdir.exists() else 0
        total_sd_games += cnt
        print(f"  - {folder:6s}: {cnt:,} games")
    print(f"Total Games Verified on SD Card: {total_sd_games:,}")
    print(f"Total Box Art Files on SD Card:  {len(list(dest_boxart.glob('*.raw'))):,}")
    print("==================================================================")
    return 0

if __name__ == '__main__':
    raise SystemExit(main())

