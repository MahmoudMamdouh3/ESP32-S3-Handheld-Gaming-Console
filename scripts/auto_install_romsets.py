#!/usr/bin/env python3
"""
auto_install_romsets.py
Automates downloading, extracting, sanitizing, and installing complete curated
game catalogues for Game Boy, Game Boy Color, NES, Doom, Sega Master System,
Game Gear, Atari 2600, PC Engine, and Pico-8 onto the SD card or local games directory.
"""

import os
import sys
import re
import argparse
import urllib.request
import zipfile
import shutil
from pathlib import Path

DEFAULT_DEST = Path(r"E:\BMO Gameboy\games")
CACHE_DIR = Path(r"E:\BMO Gameboy\.rom_cache")

ROM_PACKS = [
    {
        "console": "Game Boy (Complete USA Collection)",
        "ext": ".gb",
        "url": "https://archive.org/download/gb_20250129/gb.zip",
        "archive_name": "Game Boy Complete USA.zip"
    },
    {
        "console": "Game Boy Color (Complete 1G1R English)",
        "ext": ".gbc",
        "url": "https://archive.org/download/nintendo-game-boy-color-en-1g-1r/Nintendo%20-%20Game%20Boy%20Color%20En%201g1r.zip",
        "archive_name": "gbc_1g1r.zip"
    },
    {
        "console": "Nintendo Entertainment System (577 Perfect Collection)",
        "ext": ".nes",
        "url": "https://archive.org/download/577-perfect-nes-archive/577%20Perfect%20NES%20Archive.zip",
        "archive_name": "577 Perfect NES Archive.zip"
    },
    {
        "console": "Doom (Freedoom 1 & 2)",
        "ext": ".wad",
        "url": "https://archive.org/download/freedoom-0.12.1/freedoom-0.12.1.zip",
        "archive_name": "freedoom-0.12.1.zip"
    },
    {
        "console": "Doom (FreeDM)",
        "ext": ".wad",
        "url": "https://archive.org/download/freedoom-0.12.1/freedm-0.12.1.zip",
        "archive_name": "freedm-0.12.1.zip"
    },
    {
        "console": "Sega Master System (Complete USA/EU Collection)",
        "ext": ".sms",
        "url": "https://archive.org/download/master-system_202502/Master%20System.zip",
        "archive_name": "master_system.zip"
    },
    {
        "console": "Sega Game Gear (Complete 1G1R Collection)",
        "ext": ".gg",
        "url": "https://archive.org/download/game-gear_202507/Game%20Gear.zip",
        "archive_name": "game_gear.zip"
    },
    {
        "console": "PC Engine / TurboGrafx-16 (Complete Collection)",
        "ext": ".pce",
        "url": "https://archive.org/download/pcengine_202306/PCEngine.zip",
        "archive_name": "pc_engine.zip"
    },
    {
        "console": "Atari 2600 (Complete 1G1R Collection)",
        "ext": ".a26",
        "url": "https://archive.org/download/atari-2600_202502/Atari%202600.zip",
        "archive_name": "atari_2600.zip"
    },
    {
        "console": "PICO-8 (Top Fantasy Cartridges)",
        "ext": ".p8",
        "url": "https://archive.org/download/pico8/PICO8.zip",
        "archive_name": "pico8.zip"
    },
    {
        "console": "ColecoVision (Complete Collection)",
        "ext": ".col",
        "url": "https://archive.org/download/colecovision_202110/colecovision.zip",
        "archive_name": "colecovision.zip"
    },
    {
        "console": "Atari Lynx (Complete Collection)",
        "ext": ".lnx",
        "url": "https://archive.org/download/atari-lynx-roms/atari-lynx-champion-collection-updated.zip",
        "archive_name": "atari_lynx.zip"
    },
    {
        "console": "SNK Neo Geo Pocket Color (Complete Collection)",
        "ext": ".ngc",
        "url": "https://archive.org/download/snk-neo-geo-pocket-color_20210327/SNK%20-%20Neo%20Geo%20Pocket%20Color.zip",
        "archive_name": "ngpc.zip"
    },
    {
        "console": "Bandai WonderSwan Color (Complete Collection)",
        "ext": ".wsc",
        "url": "https://archive.org/download/wswanc_202604/wswanc.zip",
        "archive_name": "wswanc.zip"
    },
    {
        "console": "Sega Genesis / Mega Drive (Complete Collection)",
        "ext": ".gen",
        "url": "https://archive.org/download/sega-genesis/Sega%20Genesis.zip",
        "archive_name": "sega_genesis.zip"
    }
]

SUPPORTED_EXTS = (
    ".gb", ".gbc", ".nes", ".wad", ".sms", ".gg", ".pce",
    ".a26", ".a78", ".p8", ".gen", ".md", ".smd", ".sfc",
    ".smc", ".ws", ".wsc", ".ngp", ".ngc", ".lnx", ".col", ".sg"
)

def sanitize_rom_filename(filename: str, max_len: int = 58) -> str:
    """
    Sanitize filename for FAT32 and the firmware's 64-char buffer limit.
    """
    base, ext = os.path.splitext(filename)
    # Remove illegal FAT32 characters
    clean_base = re.sub(r'[\\/*?:"<>|]', '', base)
    # Replace multiple spaces with a single space
    clean_base = re.sub(r'\s+', ' ', clean_base).strip()
    
    if len(clean_base) + len(ext) > max_len:
        clean_base = clean_base[:max_len - len(ext)].strip()
        
    return clean_base + ext.lower()

import subprocess

def download_file(url: str, dest_path: Path):
    """Download a file using fast curl.exe or fallback to urllib with integrity checking."""
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    if dest_path.exists() and dest_path.stat().st_size > 1000:
        if zipfile.is_zipfile(dest_path):
            print(f"  [Cache hit] {dest_path.name} already downloaded and verified.")
            return
        else:
            print(f"  [Warning] {dest_path.name} was corrupted. Re-downloading...")
            dest_path.unlink(missing_ok=True)

    tmp_path = dest_path.with_name(dest_path.name + ".tmp")
    tmp_path.unlink(missing_ok=True)

    print(f"  [Downloading] {url}...")
    try:
        cmd = [
            "curl.exe", "-L", "-k",
            "-A", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)",
            url, "-o", str(tmp_path)
        ]
        res = subprocess.run(cmd, capture_output=False)
        if res.returncode == 0 and tmp_path.exists() and tmp_path.stat().st_size > 1000:
            if zipfile.is_zipfile(tmp_path):
                shutil.move(tmp_path, dest_path)
                print(f"  [Verified] Download complete for {dest_path.name}.")
                return
    except Exception as err:
        print(f"    curl error: {err}, trying urllib fallback...")

    headers = {'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'}
    req = urllib.request.Request(url, headers=headers)
    
    with urllib.request.urlopen(req, timeout=120) as response, open(tmp_path, 'wb') as out_file:
        total_size = int(response.headers.get('content-length', 0))
        block_size = 256 * 1024
        downloaded = 0
        
        while True:
            buffer = response.read(block_size)
            if not buffer:
                break
            downloaded += len(buffer)
            out_file.write(buffer)

    if tmp_path.exists() and zipfile.is_zipfile(tmp_path):
        shutil.move(tmp_path, dest_path)
        print(f"  [Verified] Download complete for {dest_path.name}.")

def extract_and_install(zip_path: Path, dest_dir: Path, target_ext: str = None) -> dict:
    """Extract matching ROMs from zip, sanitize filenames, and copy to dest_dir."""
    counts = {ext: 0 for ext in SUPPORTED_EXTS}
    dest_dir.mkdir(parents=True, exist_ok=True)

    print(f"  [Extracting] {zip_path.name}...")
    try:
        with zipfile.ZipFile(zip_path, 'r') as zf:
            for member in zf.infolist():
                if member.is_dir():
                    continue
                
                raw_name = os.path.basename(member.filename)
                ext = os.path.splitext(raw_name)[1].lower()
                
                # Check for nested zips (common in No-Intro sets)
                if ext == ".zip":
                    try:
                        with zipfile.ZipFile(zf.open(member), 'r') as nested_zf:
                            for nested_member in nested_zf.infolist():
                                n_raw = os.path.basename(nested_member.filename)
                                n_ext = os.path.splitext(n_raw)[1].lower()
                                if target_ext == ".a26" and n_ext in (".bin", ".a26"):
                                    n_raw = os.path.splitext(n_raw)[0] + ".a26"
                                    n_ext = ".a26"
                                if n_ext in SUPPORTED_EXTS:
                                    clean_name = sanitize_rom_filename(n_raw)
                                    out_file = dest_dir / clean_name
                                    with nested_zf.open(nested_member) as src, open(out_file, 'wb') as dst:
                                        shutil.copyfileobj(src, dst)
                                    counts[n_ext] = counts.get(n_ext, 0) + 1
                    except Exception:
                        pass
                else:
                    if target_ext == ".a26" and ext in (".bin", ".a26"):
                        raw_name = os.path.splitext(raw_name)[0] + ".a26"
                        ext = ".a26"
                    if target_ext == ".p8" and ext in (".p8", ".png") and "p8" in raw_name.lower():
                        raw_name = os.path.splitext(raw_name)[0] + ".p8"
                        ext = ".p8"
                        
                    if ext in SUPPORTED_EXTS:
                        clean_name = sanitize_rom_filename(raw_name)
                        out_file = dest_dir / clean_name
                        with zf.open(member) as src, open(out_file, 'wb') as dst:
                            shutil.copyfileobj(src, dst)
                        counts[ext] = counts.get(ext, 0) + 1
    except Exception as exc:
        print(f"    Error reading {zip_path.name}: {exc}")

    return counts

def main():
    parser = argparse.ArgumentParser(description="Automated full-catalogue game installer for BMO Gameboy.")
    parser.add_argument("--dest", type=Path, default=DEFAULT_DEST, help="Destination directory for ROMs (SD card root or local folder)")
    args = parser.parse_args()

    dest_dir = args.dest.resolve()
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    dest_dir.mkdir(parents=True, exist_ok=True)

    if sys.stdout.encoding.lower() != 'utf-8':
        try:
            sys.stdout.reconfigure(encoding='utf-8')
        except Exception:
            pass

    print("==================================================================")
    print("[*] BMO GAMEBOY AUTOMATED FULL-CATALOGUE INSTALLER (ALL CONSOLES)")
    print(f"Target Destination: {dest_dir}")
    print(f"Local Cache:        {CACHE_DIR}")
    print("==================================================================")

    total_installed = {ext: 0 for ext in SUPPORTED_EXTS}

    for pack in ROM_PACKS:
        print(f"\n>> Processing {pack['console']}...")
        local_archive = CACHE_DIR / pack["archive_name"]
        
        try:
            download_file(pack["url"], local_archive)
            counts = extract_and_install(local_archive, dest_dir, pack.get("ext"))
            for ext, count in counts.items():
                total_installed[ext] += count
                if count > 0:
                    print(f"    Installed {count} {ext.upper()} games.")
        except Exception as e:
            print(f"    [X] Failed to process {pack['console']}: {e}")

    print("\n==================================================================")
    print("INSTALLATION SUMMARY")
    print("==================================================================")
    for ext, count in sorted(total_installed.items()):
        if count > 0:
            print(f"  {ext.upper():<10}: {count} games")
    total_games = sum(total_installed.values())
    print(f"  Total Catalogue Size:            {total_games} games")
    
    # Calculate disk usage
    total_bytes = sum(f.stat().st_size for f in dest_dir.glob("*") if f.is_file())
    print(f"  Total Disk Usage:                {total_bytes / (1024 * 1024):.2f} MB")
    print(f"  All games located in:            {dest_dir}")
    print("==================================================================")
    print("Ready to copy to your MicroSD card root directory!")

if __name__ == "__main__":
    main()
