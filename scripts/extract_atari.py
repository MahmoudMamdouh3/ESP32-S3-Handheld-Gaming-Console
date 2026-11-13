import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPTS_DIR.parent
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

from repo_tools import get_data_dir

SEVEN_ZIP = shutil.which("7z") or os.environ.get("BMO_SEVEN_ZIP", r"C:\Program Files\7-Zip\7z.exe")

def sanitize_rom_filename(filename: str, max_len: int = 58) -> str:
    base, ext = os.path.splitext(filename)
    clean_base = re.sub(r'[\\/*?:"<>|]', '', base)
    clean_base = re.sub(r'\s+', ' ', clean_base).strip()
    if len(clean_base) + len(ext) > max_len:
        clean_base = clean_base[:max_len - len(ext)].strip()
    return clean_base + ext.lower()

def extract_atari(cache_dir: Path | None = None, games_dir: Path | None = None, seven_zip: str = SEVEN_ZIP):
    if cache_dir is None or games_dir is None:
        data_dir = get_data_dir(REPO_ROOT)
        cache_dir = cache_dir or (data_dir / ".rom_cache")
        games_dir = games_dir or (data_dir / "games")
    pack = cache_dir / "Atari - 2600.7z"
    stage_dir = cache_dir / "stage_atari"
    stage_dir.mkdir(parents=True, exist_ok=True)
    games_dir.mkdir(parents=True, exist_ok=True)
    
    print("[1/3] Unpacking outer archive Atari - 2600.7z...")
    cmd1 = [seven_zip, "x", "-y", f"-o{stage_dir}", str(pack)]
    subprocess.run(cmd1, check=True, stdout=subprocess.DEVNULL)
    
    # Now find all inner .7z files
    inner_zips = list(stage_dir.rglob("*.7z"))
    print(f"[2/3] Found {len(inner_zips)} individual game archives. Extracting to games folder...")
    
    tmp_extract = stage_dir / "_game_tmp"
    tmp_extract.mkdir(parents=True, exist_ok=True)
    
    installed = 0
    for idx, gz in enumerate(inner_zips):
        cmd2 = [seven_zip, "e", "-y", f"-o{tmp_extract}", str(gz)]
        subprocess.run(cmd2, check=True, stdout=subprocess.DEVNULL)
        for rf in tmp_extract.iterdir():
            if rf.is_file():
                ext = rf.suffix.lower()
                clean_name = sanitize_rom_filename(rf.name)
                # Ensure .a26 extension for Atari 2600
                if ext in ('.bin', '.a26'):
                    clean_name = os.path.splitext(clean_name)[0] + '.a26'
                dest_file = games_dir / clean_name
                shutil.move(rf, dest_file)
                installed += 1
        # clean tmp_extract
        for item in tmp_extract.iterdir():
            if item.is_file(): item.unlink()
            
        if (idx + 1) % 100 == 0 or idx + 1 == len(inner_zips):
            print(f"  Processed {idx + 1}/{len(inner_zips)} games...")

    print("[3/3] Cleaning up staging directory...")
    shutil.rmtree(stage_dir, ignore_errors=True)
    print(f"Success! {installed} Atari 2600 games installed into {games_dir}.")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Extract user-supplied Atari 2600 archive into games directory.")
    parser.add_argument("--cache-dir", type=Path, default=None)
    parser.add_argument("--games-dir", type=Path, default=None)
    parser.add_argument("--seven-zip", default=SEVEN_ZIP)
    args = parser.parse_args()
    data_dir = get_data_dir(REPO_ROOT)
    extract_atari(
        cache_dir=args.cache_dir or (data_dir / ".rom_cache"),
        games_dir=args.games_dir or (data_dir / "games"),
        seven_zip=args.seven_zip,
    )
