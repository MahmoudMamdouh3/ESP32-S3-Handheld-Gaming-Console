import subprocess
import shutil
from pathlib import Path
import os
import re

CACHE_DIR = Path(r"E:\BMO Gameboy\.rom_cache")
GAMES_DIR = Path(r"E:\BMO Gameboy\games")
SEVEN_ZIP = r"C:\Program Files\7-Zip\7z.exe"

def sanitize_rom_filename(filename: str, max_len: int = 58) -> str:
    base, ext = os.path.splitext(filename)
    clean_base = re.sub(r'[\\/*?:"<>|]', '', base)
    clean_base = re.sub(r'\s+', ' ', clean_base).strip()
    if len(clean_base) + len(ext) > max_len:
        clean_base = clean_base[:max_len - len(ext)].strip()
    return clean_base + ext.lower()

def extract_atari():
    pack = CACHE_DIR / "Atari - 2600.7z"
    stage_dir = CACHE_DIR / "stage_atari"
    stage_dir.mkdir(parents=True, exist_ok=True)
    
    print("[1/3] Unpacking outer archive Atari - 2600.7z...")
    cmd1 = [SEVEN_ZIP, "x", "-y", f"-o{stage_dir}", str(pack)]
    subprocess.run(cmd1, check=True, stdout=subprocess.DEVNULL)
    
    # Now find all inner .7z files
    inner_zips = list(stage_dir.rglob("*.7z"))
    print(f"[2/3] Found {len(inner_zips)} individual game archives. Extracting to games folder...")
    
    tmp_extract = stage_dir / "_game_tmp"
    tmp_extract.mkdir(parents=True, exist_ok=True)
    
    installed = 0
    for idx, gz in enumerate(inner_zips):
        cmd2 = [SEVEN_ZIP, "e", "-y", f"-o{tmp_extract}", str(gz)]
        subprocess.run(cmd2, check=True, stdout=subprocess.DEVNULL)
        for rf in tmp_extract.iterdir():
            if rf.is_file():
                ext = rf.suffix.lower()
                clean_name = sanitize_rom_filename(rf.name)
                # Ensure .a26 extension for Atari 2600
                if ext in ('.bin', '.a26'):
                    clean_name = os.path.splitext(clean_name)[0] + '.a26'
                dest_file = GAMES_DIR / clean_name
                shutil.move(rf, dest_file)
                installed += 1
        # clean tmp_extract
        for item in tmp_extract.iterdir():
            if item.is_file(): item.unlink()
            
        if (idx + 1) % 100 == 0 or idx + 1 == len(inner_zips):
            print(f"  Processed {idx + 1}/{len(inner_zips)} games...")

    print(f"[3/3] Cleaning up staging directory...")
    shutil.rmtree(stage_dir, ignore_errors=True)
    print(f"Success! {installed} Atari 2600 games installed into {GAMES_DIR}.")

if __name__ == '__main__':
    extract_atari()
