"""
scripts/bulk_cover_manager.py — High-Performance Bulk Box Art Downloader & Converter
Downloads complete official retail box art packs from the Libretro Thumbnails archive,
matches them to all installed ROMs in E:\BMO Gameboy\games, and converts them to
64x64 BGR565 (.raw) and 24-bit BMP (.bmp) compatible with BMO Gameboy firmware.
"""

import os
import re
import sys
import time
import shutil
import struct
import urllib.parse
import subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from PIL import Image

REPO_ROOT = Path(r"E:\BMO Gameboy")
GAMES_DIR = REPO_ROOT / "games"
CACHE_DIR = REPO_ROOT / ".rom_cache" / "thumbnails"
BOXART_DIR = REPO_ROOT / "roms" / "boxart"
SD_BOXART_DIR = REPO_ROOT / "sdcard" / "boxart"
HD_BOXART_DIR = REPO_ROOT / "roms" / "boxart" / "hd"

ARIA2C = r"C:\Users\mahmo\AppData\Local\Microsoft\WinGet\Links\aria2c.EXE"
SEVEN_ZIP = r"C:\Program Files\7-Zip\7z.exe"
BASE_URL = "https://archive.org/download/retroarch-thumbnails/"

COVER_WIDTH = 64
COVER_HEIGHT = 64
RAW_SIZE = COVER_WIDTH * COVER_HEIGHT * 2  # 8192 bytes

SYSTEM_PACKS = [
    {
        "id": "gb",
        "name": "Game Boy",
        "archive": "Nintendo - Game Boy.7z",
        "extensions": [".gb"]
    },
    {
        "id": "gbc",
        "name": "Game Boy Color",
        "archive": "Nintendo - Game Boy Color.7z",
        "extensions": [".gbc"]
    },
    {
        "id": "nes",
        "name": "NES",
        "archive": "Nintendo - Nintendo Entertainment System.7z",
        "extensions": [".nes"]
    },
    {
        "id": "genesis",
        "name": "Sega Genesis",
        "archive": "Sega - Mega Drive - Genesis.7z",
        "extensions": [".gen", ".md"]
    },
    {
        "id": "sms",
        "name": "Sega Master System",
        "archive": "Sega - Master System - Mark III.7z",
        "extensions": [".sms"]
    },
    {
        "id": "gg",
        "name": "Sega Game Gear",
        "archive": "Sega - Game Gear.7z",
        "extensions": [".gg"]
    },
    {
        "id": "sg1000",
        "name": "Sega SG-1000",
        "archive": "Sega - SG-1000.7z",
        "extensions": [".sg"]
    },
    {
        "id": "pce",
        "name": "PC Engine",
        "archive": "NEC - PC Engine - TurboGrafx 16.7z",
        "extensions": [".pce"]
    },
    {
        "id": "sgx",
        "name": "PC Engine SuperGrafx",
        "archive": "NEC - PC Engine SuperGrafx.7z",
        "extensions": [".pce"]
    },
    {
        "id": "a26",
        "name": "Atari 2600",
        "archive": "Atari - 2600.7z",
        "extensions": [".a26"]
    },
    {
        "id": "lynx",
        "name": "Atari Lynx",
        "archive": "Atari - Lynx.7z",
        "extensions": [".lnx"]
    },
    {
        "id": "ws",
        "name": "Bandai WonderSwan",
        "archive": "Bandai - WonderSwan.7z",
        "extensions": [".ws"]
    },
    {
        "id": "wsc",
        "name": "Bandai WonderSwan Color",
        "archive": "Bandai - WonderSwan Color.7z",
        "extensions": [".wsc"]
    },
    {
        "id": "ngp",
        "name": "SNK Neo Geo Pocket",
        "archive": "SNK - Neo Geo Pocket.7z",
        "extensions": [".ngp"]
    },
    {
        "id": "ngc",
        "name": "SNK Neo Geo Pocket Color",
        "archive": "SNK - Neo Geo Pocket Color.7z",
        "extensions": [".ngc"]
    },
    {
        "id": "col",
        "name": "ColecoVision",
        "archive": "Coleco - ColecoVision.7z",
        "extensions": [".col"]
    },
    {
        "id": "snes",
        "name": "Super Nintendo",
        "archive": "Nintendo - Super Nintendo Entertainment System.7z",
        "extensions": [".sfc", ".smc"]
    }
]

def normalize_title(title: str) -> str:
    """Normalize string for fuzzy title matching."""
    s = title.lower()
    s = re.sub(r'\(.*?\)|\[.*?\]', '', s)  # strip tags
    s = re.sub(r'[^a-z0-9]', '', s)
    return s.strip()

def convert_png_to_bmo_assets(png_path: Path, base_name: str) -> bool:
    """Resamples high-res PNG into 64x64 BGR565 .raw and 24-bit .bmp."""
    try:
        with Image.open(png_path) as im:
            rgb = im.convert("RGB")
            # Preserve original full HD copy in hd/
            hd_target = HD_BOXART_DIR / f"{base_name}.png"
            if not hd_target.exists():
                rgb.save(hd_target, format="PNG", optimize=True)

            resized = rgb.resize((COVER_WIDTH, COVER_HEIGHT), Image.Resampling.LANCZOS)
            raw_rgb = resized.tobytes()

            # Encode BGR565 pre-swapped (Theme::makeUiColor format)
            raw_bytes = bytearray(RAW_SIZE)
            for i in range(COVER_WIDTH * COVER_HEIGHT):
                r = raw_rgb[i * 3]
                g = raw_rgb[i * 3 + 1]
                b = raw_rgb[i * 3 + 2]
                c565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3)
                swapped = ((c565 & 0xFF) << 8) | ((c565 >> 8) & 0xFF)
                raw_bytes[i * 2] = swapped & 0xFF
                raw_bytes[i * 2 + 1] = (swapped >> 8) & 0xFF

            # Encode 24-bit standard BMP
            row_stride = (COVER_WIDTH * 3 + 3) & ~3
            data_size = row_stride * COVER_HEIGHT
            file_size = 54 + data_size
            header = bytearray(54)
            header[0:2] = b"BM"
            struct.pack_into("<I", header, 2, file_size)
            struct.pack_into("<I", header, 10, 54)
            struct.pack_into("<I", header, 14, 40)
            struct.pack_into("<i", header, 18, COVER_WIDTH)
            struct.pack_into("<i", header, 22, COVER_HEIGHT)
            struct.pack_into("<H", header, 26, 1)
            struct.pack_into("<H", header, 28, 24)
            struct.pack_into("<I", header, 34, data_size)

            body = bytearray(data_size)
            for y in range(COVER_HEIGHT):
                src_row = COVER_HEIGHT - 1 - y
                row_offset = y * row_stride
                for x in range(COVER_WIDTH):
                    px_off = (src_row * COVER_WIDTH + x) * 3
                    dst_off = row_offset + x * 3
                    body[dst_off] = raw_rgb[px_off + 2]      # B
                    body[dst_off + 1] = raw_rgb[px_off + 1]  # G
                    body[dst_off + 2] = raw_rgb[px_off]      # R

            bmp_bytes = bytes(header) + bytes(body)

            # Save to roms/boxart
            (BOXART_DIR / f"{base_name}.raw").write_bytes(bytes(raw_bytes))
            (BOXART_DIR / f"{base_name}.bmp").write_bytes(bmp_bytes)

            # Mirror to sdcard/boxart
            (SD_BOXART_DIR / f"{base_name}.raw").write_bytes(bytes(raw_bytes))
            (SD_BOXART_DIR / f"{base_name}.bmp").write_bytes(bmp_bytes)
            return True
    except Exception as e:
        return False

def download_archive(archive_name: str) -> Path:
    dest_path = CACHE_DIR / archive_name
    if dest_path.exists() and dest_path.stat().st_size > 500 * 1024:
        # Check archive integrity
        res = subprocess.run([SEVEN_ZIP, "t", str(dest_path)], capture_output=True)
        if res.returncode == 0:
            print(f"  [Cache hit] {archive_name} verified.")
            return dest_path
        else:
            dest_path.unlink(missing_ok=True)

    url = BASE_URL + urllib.parse.quote(archive_name)
    print(f"  [Downloading] {archive_name} via aria2c...")
    cmd = [
        ARIA2C,
        "-x", "16",
        "-s", "16",
        "-j", "4",
        "--min-split-size=1M",
        "--file-allocation=none",
        "-d", str(CACHE_DIR),
        "-o", archive_name,
        "--auto-file-renaming=false",
        "--allow-overwrite=true",
        url
    ]
    subprocess.run(cmd, check=True)
    return dest_path

def process_system(sys_def: dict):
    name = sys_def["name"]
    archive_name = sys_def["archive"]
    exts = sys_def["extensions"]

    print(f"\n=======================================================")
    print(f"[*] Processing Box Art for: {name} ({archive_name})")
    print(f"=======================================================")

    archive_path = download_archive(archive_name)

    extract_dir = CACHE_DIR / "extracted" / sys_def["id"]
    if extract_dir.exists():
        shutil.rmtree(extract_dir, ignore_errors=True)
    extract_dir.mkdir(parents=True, exist_ok=True)

    print(f"  [Extracting] Named_Boxarts from {archive_name}...")
    cmd = [
        SEVEN_ZIP,
        "e", "-y",
        f"-o{extract_dir}",
        str(archive_path),
        "Named_Boxarts/*",
        "-r"
    ]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)

    # Index available PNGs
    boxarts = {}
    normalized_boxarts = {}
    for p in extract_dir.glob("*.png"):
        boxarts[p.stem] = p
        norm = normalize_title(p.stem)
        if norm and norm not in normalized_boxarts:
            normalized_boxarts[norm] = p

    print(f"  [Indexed] {len(boxarts)} official retail box art covers available.")

    # Match installed ROMs
    installed_roms = [
        f for f in GAMES_DIR.iterdir()
        if f.is_file() and f.suffix.lower() in exts
    ]
    print(f"  [Matching] {len(installed_roms)} installed {name} games...")

    matched = 0
    skipped_existing = 0
    missing = 0

    to_convert = []
    for rf in installed_roms:
        stem = rf.stem
        # Check if already generated
        if (BOXART_DIR / f"{stem}.raw").exists() and (BOXART_DIR / f"{stem}.bmp").exists():
            skipped_existing += 1
            continue

        target_png = boxarts.get(stem)
        if not target_png:
            # Try normalized fuzzy lookup
            norm = normalize_title(stem)
            target_png = normalized_boxarts.get(norm)

        if target_png:
            to_convert.append((target_png, stem))
            matched += 1
        else:
            missing += 1

    print(f"  [Converting] {len(to_convert)} new covers via parallel Lanczos...")
    with ThreadPoolExecutor(max_workers=8) as pool:
        results = list(pool.map(lambda pair: convert_png_to_bmo_assets(pair[0], pair[1]), to_convert))

    success = sum(1 for r in results if r)
    print(f"  [Finished {name}] +{success} converted, {skipped_existing} already up to date, {missing} un-indexed.")
    
    # Cleanup extracted PNGs to preserve disk space
    shutil.rmtree(extract_dir, ignore_errors=True)

def main():
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    BOXART_DIR.mkdir(parents=True, exist_ok=True)
    SD_BOXART_DIR.mkdir(parents=True, exist_ok=True)
    HD_BOXART_DIR.mkdir(parents=True, exist_ok=True)

    print("==================================================================")
    print("[*] BMO GAMEBOY BULK COVER ART INSTALLATION & RESAMPLING PIPELINE")
    print(f"Destination:  {BOXART_DIR} & {SD_BOXART_DIR}")
    print(f"Cache:        {CACHE_DIR}")
    print(f"Packs:        {len(SYSTEM_PACKS)} systems")
    print("==================================================================")

    # Optional filter via args
    selected = SYSTEM_PACKS
    if len(sys.argv) > 1:
        req = [a.lower() for a in sys.argv[1:]]
        selected = [p for p in SYSTEM_PACKS if p["id"] in req or any(r in p["name"].lower() for r in req)]

    for p in selected:
        try:
            process_system(p)
        except Exception as e:
            print(f"[ERROR] Failed {p['name']}: {e}")

    print("\n==================================================================")
    print("BULK COVER ART INSTALLATION COMPLETE")
    print("==================================================================")

if __name__ == "__main__":
    main()

