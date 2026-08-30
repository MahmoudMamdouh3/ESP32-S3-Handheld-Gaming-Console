#!/usr/bin/env python3
"""
auto_install_romsets.py
Automates downloading, extracting, sanitizing, and installing complete curated
1G1R game catalogues for Game Boy, Game Boy Color, NES, and Doom onto the SD card
or local games directory.
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
    }
]

SUPPORTED_EXTS = (".gb", ".gbc", ".nes", ".wad")

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

def download_file(url: str, dest_path: Path):
    """Download a file with progress reporting and custom User-Agent."""
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    if dest_path.exists() and dest_path.stat().st_size > 0:
        print(f"  [Cache hit] {dest_path.name} already downloaded.")
        return

    print(f"  [Downloading] {url}...")
    headers = {'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'}
    req = urllib.request.Request(url, headers=headers)
    
    with urllib.request.urlopen(req, timeout=120) as response, open(dest_path, 'wb') as out_file:
        total_size = int(response.headers.get('content-length', 0))
        block_size = 64 * 1024
        downloaded = 0
        
        while True:
            buffer = response.read(block_size)
            if not buffer:
                break
            downloaded += len(buffer)
            out_file.write(buffer)
            if total_size > 0:
                percent = downloaded * 100 / total_size
                mb = downloaded / (1024 * 1024)
                total_mb = total_size / (1024 * 1024)
                sys.stdout.write(f"\r    -> {mb:.1f}MB / {total_mb:.1f}MB ({percent:.1f}%)")
                sys.stdout.flush()
        print()

def extract_and_install(zip_path: Path, dest_dir: Path) -> dict:
    """Extract matching ROMs from zip, sanitize filenames, and copy to dest_dir."""
    counts = {".gb": 0, ".gbc": 0, ".nes": 0, ".wad": 0}
    dest_dir.mkdir(parents=True, exist_ok=True)

    print(f"  [Extracting] {zip_path.name}...")
    try:
        with zipfile.ZipFile(zip_path, 'r') as zf:
            for member in zf.infolist():
                if member.is_dir():
                    continue
                
                ext = os.path.splitext(member.filename)[1].lower()
                
                # Check for nested zips (common in some sets)
                if ext == ".zip":
                    try:
                        with zipfile.ZipFile(zf.open(member), 'r') as nested_zf:
                            for nested_member in nested_zf.infolist():
                                n_ext = os.path.splitext(nested_member.filename)[1].lower()
                                if n_ext in SUPPORTED_EXTS:
                                    clean_name = sanitize_rom_filename(os.path.basename(nested_member.filename))
                                    out_file = dest_dir / clean_name
                                    with nested_zf.open(nested_member) as src, open(out_file, 'wb') as dst:
                                        shutil.copyfileobj(src, dst)
                                    counts[n_ext] = counts.get(n_ext, 0) + 1
                    except Exception as e:
                        pass
                elif ext in SUPPORTED_EXTS:
                    clean_name = sanitize_rom_filename(os.path.basename(member.filename))
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
    print("[*] BMO GAMEBOY AUTOMATED FULL-CATALOGUE INSTALLER")
    print(f"Target Destination: {dest_dir}")
    print(f"Local Cache:        {CACHE_DIR}")
    print("==================================================================")

    total_installed = {".gb": 0, ".gbc": 0, ".nes": 0, ".wad": 0}

    for pack in ROM_PACKS:
        print(f"\n>> Processing {pack['console']}...")
        local_archive = CACHE_DIR / pack["archive_name"]
        
        try:
            download_file(pack["url"], local_archive)
            counts = extract_and_install(local_archive, dest_dir)
            for ext, count in counts.items():
                total_installed[ext] += count
                if count > 0:
                    print(f"    Installed {count} {ext.upper()} games.")
        except Exception as e:
            print(f"    [X] Failed to process {pack['console']}: {e}")

    print("\n==================================================================")
    print("INSTALLATION SUMMARY")
    print("==================================================================")
    print(f"  Game Boy (.gb):                 {total_installed['.gb']} games")
    print(f"  Game Boy Color (.gbc):           {total_installed['.gbc']} games")
    print(f"  Nintendo Entertainment (.nes):   {total_installed['.nes']} games")
    print(f"  Doom / Freedoom (.wad):          {total_installed['.wad']} WADs")
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
