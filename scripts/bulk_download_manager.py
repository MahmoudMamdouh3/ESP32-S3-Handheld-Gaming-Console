#!/usr/bin/env python3
"""
bulk_download_manager.py
Industrial-strength bulk downloader, extractor, and validator for all consoles in BMO Gameboy.
Downloads full No-Intro archives from Archive.org, extracts, verifies headers/integrity,
sanitizes filenames for FAT32, and installs all working games to E:\\BMO Gameboy\\games.
"""

import os
import sys
import re
import shutil
import subprocess
import urllib.parse
from pathlib import Path
from collections import defaultdict

# Add scripts directory to path for validators
SCRIPTS_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPTS_DIR.parent
GAMES_DIR = REPO_ROOT / "games"
CACHE_DIR = REPO_ROOT / ".rom_cache"
STAGE_DIR = CACHE_DIR / "stage_bulk"

ARIA2C = r"C:\Users\mahmo\AppData\Local\Microsoft\WinGet\Links\aria2c.EXE"
SEVEN_ZIP = r"C:\Program Files\7-Zip\7z.exe"
BASE_URL = "https://archive.org/download/no-intro_romset_collection/"

sys.path.append(str(SCRIPTS_DIR))
from audit_all_games import VALIDATORS

# Target packs definition
PACKS = [
    {
        "id": "lynx",
        "name": "Atari - Lynx.7z",
        "console": "Atari Lynx",
        "target_ext": ".lnx",
        "size_mb": 12.7
    },
    {
        "id": "ngpc",
        "name": "SNK - Neo Geo Pocket Color.7z",
        "console": "SNK Neo Geo Pocket Color",
        "target_ext": ".ngc",
        "size_mb": 61.3
    },
    {
        "id": "sms",
        "name": "Sega - Master System - Mark III.7z",
        "console": "Sega Master System",
        "target_ext": ".sms",
        "size_mb": 73.0
    },
    {
        "id": "pce",
        "name": "NEC - PC Engine - TurboGrafx 16.7z",
        "console": "PC Engine / TurboGrafx-16",
        "target_ext": ".pce",
        "size_mb": 90.5
    },
    {
        "id": "wswan",
        "name": "Bandai - WonderSwan.7z",
        "console": "Bandai WonderSwan",
        "target_ext": ".ws",
        "size_mb": 93.6
    },
    {
        "id": "wscolor",
        "name": "Bandai - WonderSwan Color.7z",
        "console": "Bandai WonderSwan Color",
        "target_ext": ".wsc",
        "size_mb": 155.2
    },
    {
        "id": "gg",
        "name": "Sega - Game Gear.7z",
        "console": "Sega Game Gear",
        "target_ext": ".gg",
        "size_mb": 131.0
    },
    {
        "id": "gb",
        "name": "Nintendo - Game Boy.7z",
        "console": "Nintendo Game Boy",
        "target_ext": ".gb",
        "size_mb": 197.9
    },
    {
        "id": "nes",
        "name": "Nintendo - Nintendo Entertainment System (Headered).7z",
        "console": "Nintendo Entertainment System",
        "target_ext": ".nes",
        "size_mb": 394.5
    },
    {
        "id": "gbc",
        "name": "Nintendo - Game Boy Color.7z",
        "console": "Nintendo Game Boy Color",
        "target_ext": ".gbc",
        "size_mb": 746.9
    },
    {
        "id": "gen",
        "name": "Sega - Mega Drive - Genesis.7z",
        "console": "Sega Genesis / Mega Drive",
        "target_ext": ".gen",
        "size_mb": 1608.0
    },
    {
        "id": "snes",
        "name": "Nintendo - Super Nintendo Entertainment System.7z",
        "console": "Super Nintendo",
        "target_ext": ".sfc",
        "size_mb": 3151.2
    },
    {
        "id": "sg1000",
        "name": "Sega - SG-1000.7z",
        "console": "Sega SG-1000",
        "target_ext": ".sg",
        "size_mb": 2.9
    },
    {
        "id": "supergrafx",
        "name": "NEC - PC Engine SuperGrafx.7z",
        "console": "PC Engine SuperGrafx",
        "target_ext": ".pce",
        "size_mb": 2.4
    }
]

def sanitize_rom_filename(filename: str, max_len: int = 58) -> str:
    base, ext = os.path.splitext(filename)
    clean_base = re.sub(r'[\\/*?:"<>|]', '', base)
    clean_base = re.sub(r'\s+', ' ', clean_base).strip()
    if len(clean_base) + len(ext) > max_len:
        clean_base = clean_base[:max_len - len(ext)].strip()
    return clean_base + ext.lower()

def download_pack(archive_name: str) -> Path:
    dest_file = CACHE_DIR / archive_name
    if dest_file.exists() and dest_file.stat().st_size > 1024 * 1024:
        # Check archive integrity with 7z
        res = subprocess.run([SEVEN_ZIP, "t", str(dest_file)], capture_output=True)
        if res.returncode == 0:
            print(f"  [Cache hit] {archive_name} already downloaded and verified.")
            return dest_file
        else:
            print(f"  [Notice] {archive_name} corrupted in cache. Re-downloading...")
            dest_file.unlink(missing_ok=True)

    url = BASE_URL + urllib.parse.quote(archive_name)
    print(f"  [aria2c downloading] {archive_name} from {url}...")
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
    return dest_file

def process_pack(pack: dict) -> dict:
    archive_name = pack["name"]
    console = pack["console"]
    target_ext = pack["target_ext"]

    print(f"\n=======================================================")
    print(f"[*] Processing: {console} ({archive_name})")
    print(f"=======================================================")

    archive_path = download_pack(archive_name)
    
    stage_outer = STAGE_DIR / pack["id"]
    if stage_outer.exists():
        shutil.rmtree(stage_outer, ignore_errors=True)
    stage_outer.mkdir(parents=True, exist_ok=True)

    print(f"  [1/3] Unpacking outer archive {archive_name}...")
    subprocess.run([SEVEN_ZIP, "x", "-y", f"-o{stage_outer}", str(archive_path)], check=True, stdout=subprocess.DEVNULL)

    inner_archives = list(stage_outer.rglob("*.7z")) + list(stage_outer.rglob("*.zip"))
    valid_exts = {target_ext}
    if target_ext == ".sfc": valid_exts.update({".smc", ".sfc"})
    elif target_ext == ".gen": valid_exts.update({".bin", ".md", ".smd", ".gen"})
    elif target_ext == ".a26": valid_exts.update({".bin", ".a26"})
    elif target_ext == ".sg": valid_exts.update({".bin", ".sg"})
    elif target_ext == ".pce": valid_exts.update({".sgx", ".pce", ".bin"})
    direct_roms = [f for f in stage_outer.rglob("*") if f.is_file() and f.suffix.lower() in valid_exts]

    print(f"  [2/3] Found {len(inner_archives)} game archives, {len(direct_roms)} direct ROMs. Extracting & validating...")

    tmp_extract = stage_outer / "_tmp_extract"
    tmp_extract.mkdir(parents=True, exist_ok=True)

    installed = 0
    skipped_dup = 0
    broken = 0
    total_found = len(inner_archives) + len(direct_roms)

    validator = VALIDATORS.get(target_ext)

    # Process inner archives
    for idx, arc in enumerate(inner_archives):
        subprocess.run([SEVEN_ZIP, "e", "-y", f"-o{tmp_extract}", str(arc)], check=True, stdout=subprocess.DEVNULL)
        for rf in tmp_extract.iterdir():
            if rf.is_file():
                ext = rf.suffix.lower()
                clean_name = sanitize_rom_filename(rf.name)
                # Map alternate extensions to canonical target_ext
                if target_ext == ".a26" and ext in ('.bin', '.a26'):
                    clean_name = os.path.splitext(clean_name)[0] + '.a26'
                    ext = ".a26"
                elif target_ext == ".gen" and ext in ('.bin', '.md', '.smd', '.gen'):
                    clean_name = os.path.splitext(clean_name)[0] + '.gen'
                    ext = ".gen"
                elif target_ext == ".sfc" and ext in ('.smc', '.sfc'):
                    clean_name = os.path.splitext(clean_name)[0] + '.sfc'
                    ext = ".sfc"
                elif target_ext == ".sg" and ext in ('.bin', '.sg'):
                    clean_name = os.path.splitext(clean_name)[0] + '.sg'
                    ext = ".sg"
                elif target_ext == ".pce" and ext in ('.sgx', '.pce', '.bin'):
                    clean_name = os.path.splitext(clean_name)[0] + '.pce'
                    ext = ".pce"

                dest_file = GAMES_DIR / clean_name
                
                # Check validation
                is_valid = True
                if validator:
                    try:
                        sz = rf.stat().st_size
                        with open(rf, 'rb') as fp:
                            data = fp.read(min(sz, 1024 * 1024))
                        is_valid, reason = validator(data)
                    except Exception:
                        is_valid = False

                if is_valid:
                    if not dest_file.exists():
                        shutil.move(rf, dest_file)
                        installed += 1
                    else:
                        skipped_dup += 1
                else:
                    broken += 1

        # Clear tmp_extract
        for item in tmp_extract.iterdir():
            if item.is_file(): item.unlink()

        if (idx + 1) % 250 == 0 or idx + 1 == len(inner_archives):
            print(f"    Progress: {idx + 1}/{len(inner_archives)} archives processed | +{installed} new, {skipped_dup} dups, {broken} broken")

    # Process direct ROMs
    for rf in direct_roms:
        clean_name = sanitize_rom_filename(rf.name)
        if target_ext in ('.a26', '.gen', '.sfc', '.sg', '.pce'):
            clean_name = os.path.splitext(clean_name)[0] + target_ext
        dest_file = GAMES_DIR / clean_name
        is_valid = True
        if validator:
            try:
                sz = rf.stat().st_size
                with open(rf, 'rb') as fp:
                    data = fp.read(min(sz, 1024 * 1024))
                is_valid, reason = validator(data)
            except Exception:
                is_valid = False
        if is_valid:
            if not dest_file.exists():
                shutil.copy2(rf, dest_file)
                installed += 1
            else:
                skipped_dup += 1
        else:
            broken += 1

    print(f"  [3/3] Cleaning up staging directory...")
    shutil.rmtree(stage_outer, ignore_errors=True)

    res = {
        "console": console,
        "target_ext": target_ext,
        "installed_new": installed,
        "already_had": skipped_dup,
        "broken": broken,
        "total_active": installed + skipped_dup
    }
    print(f"  [Done] {console}: +{installed} newly added, {skipped_dup} existing. Total in library: {installed + skipped_dup} (broken: {broken})")
    return res

def main():
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    GAMES_DIR.mkdir(parents=True, exist_ok=True)
    STAGE_DIR.mkdir(parents=True, exist_ok=True)

    print("==================================================================")
    print("[*] BMO GAMEBOY TOTAL CONSOLE LIBRARY BULK INSTALLER")
    print(f"Destination:  {GAMES_DIR}")
    print(f"Cache:        {CACHE_DIR}")
    print(f"Target Packs: {len(PACKS)} systems")
    print("==================================================================")

    # Optional filter via sys.argv
    selected_packs = PACKS
    if len(sys.argv) > 1:
        req_ids = [a.lower() for a in sys.argv[1:]]
        selected_packs = [p for p in PACKS if p["id"] in req_ids or any(r in p["name"].lower() for r in req_ids)]

    results = []
    for pack in selected_packs:
        try:
            r = process_pack(pack)
            results.append(r)
        except Exception as e:
            print(f"[ERROR] Failed processing {pack['console']}: {e}")

    print("\n==================================================================")
    print("BULK INSTALLATION GRAND SUMMARY")
    print("==================================================================")
    for r in results:
        print(f"  {r['console']:<32} [{r['target_ext']}]: {r['total_active']:>5} games (+{r['installed_new']} new, {r['broken']} rejected)")

if __name__ == '__main__':
    main()
