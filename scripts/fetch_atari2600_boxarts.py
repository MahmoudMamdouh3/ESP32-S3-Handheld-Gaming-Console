"""
scripts/fetch_atari2600_boxarts.py — High-Performance Direct HTTPS Atari 2600 Box Art Downloader
Downloads official Atari 2600 retail box arts directly from thumbnails.libretro.com CDN over HTTPS
and converts them into 64x64 BGR565 .raw and 24-bit .bmp files.
"""

import os
import re
import io
import time
import struct
import urllib.parse
import urllib.request
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from PIL import Image

REPO_ROOT = Path(r"E:\BMO Gameboy")
GAMES_DIR = REPO_ROOT / "games"
BOXART_DIR = REPO_ROOT / "roms" / "boxart"
SD_BOXART_DIR = REPO_ROOT / "sdcard" / "boxart"
HD_BOXART_DIR = REPO_ROOT / "roms" / "boxart" / "hd"

CDN_URL = "https://thumbnails.libretro.com/Atari%20-%202600/Named_Boxarts/"
COVER_WIDTH = 64
COVER_HEIGHT = 64
RAW_SIZE = COVER_WIDTH * COVER_HEIGHT * 2

def normalize_title(title: str) -> str:
    s = title.lower()
    s = re.sub(r'\(.*?\)|\[.*?\]', '', s)
    s = re.sub(r'[^a-z0-9]', '', s)
    return s.strip()

def convert_png_bytes_to_bmo(png_bytes: bytes, base_name: str) -> bool:
    try:
        with Image.open(io.BytesIO(png_bytes)) as im:
            rgb = im.convert("RGB")
            # Save original HD copy
            hd_target = HD_BOXART_DIR / f"{base_name}.png"
            if not hd_target.exists():
                rgb.save(hd_target, format="PNG", optimize=True)

            resized = rgb.resize((COVER_WIDTH, COVER_HEIGHT), Image.Resampling.LANCZOS)
            raw_rgb = resized.tobytes()

            # Encode BGR565 pre-swapped
            raw_bytes = bytearray(RAW_SIZE)
            for i in range(COVER_WIDTH * COVER_HEIGHT):
                r = raw_rgb[i * 3]
                g = raw_rgb[i * 3 + 1]
                b = raw_rgb[i * 3 + 2]
                c565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3)
                swapped = ((c565 & 0xFF) << 8) | ((c565 >> 8) & 0xFF)
                raw_bytes[i * 2] = swapped & 0xFF
                raw_bytes[i * 2 + 1] = (swapped >> 8) & 0xFF

            # Encode 24-bit BMP
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
                    body[dst_off] = raw_rgb[px_off + 2]
                    body[dst_off + 1] = raw_rgb[px_off + 1]
                    body[dst_off + 2] = raw_rgb[px_off]

            bmp_bytes = bytes(header) + bytes(body)

            (BOXART_DIR / f"{base_name}.raw").write_bytes(bytes(raw_bytes))
            (BOXART_DIR / f"{base_name}.bmp").write_bytes(bmp_bytes)
            (SD_BOXART_DIR / f"{base_name}.raw").write_bytes(bytes(raw_bytes))
            (SD_BOXART_DIR / f"{base_name}.bmp").write_bytes(bmp_bytes)
            return True
    except Exception:
        return False

def fetch_and_convert(task: tuple) -> bool:
    url, base_name = task
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    try:
        with urllib.request.urlopen(req, timeout=15) as r:
            data = r.read()
        return convert_png_bytes_to_bmo(data, base_name)
    except Exception:
        return False

def main():
    BOXART_DIR.mkdir(parents=True, exist_ok=True)
    SD_BOXART_DIR.mkdir(parents=True, exist_ok=True)
    HD_BOXART_DIR.mkdir(parents=True, exist_ok=True)

    print("[*] Fetching Atari 2600 official box art catalog from Libretro CDN...")
    req = urllib.request.Request(CDN_URL, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        html = r.read().decode('utf-8', errors='ignore')

    cdn_files = {}
    normalized_cdn = {}
    for line in html.splitlines():
        if '.png' in line and 'href=' in line:
            m = re.search(r'href="([^"]+\.png)"', line)
            if m:
                raw_filename = urllib.parse.unquote(m.group(1))
                stem = Path(raw_filename).stem
                cdn_files[stem] = CDN_URL + m.group(1)
                norm = normalize_title(stem)
                if norm and norm not in normalized_cdn:
                    normalized_cdn[norm] = CDN_URL + m.group(1)

    print(f"[*] Indexed {len(cdn_files)} Atari 2600 box arts on CDN.")

    installed_a26 = [f for f in GAMES_DIR.iterdir() if f.is_file() and f.suffix.lower() == '.a26']
    print(f"[*] Found {len(installed_a26)} installed Atari 2600 games.")

    tasks = []
    skipped = 0
    missing = 0

    for rf in installed_a26:
        stem = rf.stem
        if (BOXART_DIR / f"{stem}.raw").exists():
            skipped += 1
            continue
        url = cdn_files.get(stem)
        if not url:
            norm = normalize_title(stem)
            url = normalized_cdn.get(norm)
        if url:
            tasks.append((url, stem))
        else:
            missing += 1

    print(f"[*] Downloading and converting {len(tasks)} Atari 2600 covers (already up to date: {skipped}, missing: {missing})...")
    with ThreadPoolExecutor(max_workers=12) as pool:
        results = list(pool.map(fetch_and_convert, tasks))

    success = sum(1 for r in results if r)
    print(f"[Done] Atari 2600: +{success} covers converted and installed!")

if __name__ == '__main__':
    main()

