"""
scripts/fetch_covers.py — High-Resolution Original Box Art Downloader & Multi-Target BGR565 Converter
Downloads the original full-resolution box art PNGs (for 4K Web Simulator & future high-DPI screens)
and converts them via Lanczos resampling into 64x64 24-bit BMP (.bmp) and 8,192-byte pre-swapped
BGR565 (.raw) cover art compatible with BoxArt::load() (src/core/box_art.cpp) on MicroSD (/boxart/).
"""

import argparse
import hashlib
import io
import re
import shutil
import struct
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

from scripts.rom_key import make_rom_key

try:
    from PIL import Image, ImageDraw
except ImportError:
    Image = None  # type: ignore

REPO_ROOT = Path(__file__).resolve().parents[1]
COVER_WIDTH = 64
COVER_HEIGHT = 64
RAW_BYTE_SIZE = COVER_WIDTH * COVER_HEIGHT * 2  # 8,192 bytes

LIBRETRO_SYSTEM_MAP = {
    ".gb": "Nintendo - Game Boy",
    ".gbc": "Nintendo - Game Boy Color",
    ".nes": "Nintendo - Nintendo Entertainment System",
    ".sfc": "Nintendo - Super Nintendo Entertainment System",
    ".smc": "Nintendo - Super Nintendo Entertainment System",
    ".sms": "Sega - Master System - Mark III",
    ".gg": "Sega - Game Gear",
    ".gen": "Sega - Mega Drive - Genesis",
    ".md": "Sega - Mega Drive - Genesis",
    ".pce": "NEC - PC Engine - TurboGrafx 16",
    ".a26": "Atari - 2600",
    ".lnx": "Atari - Lynx",
    ".ws": "Bandai - WonderSwan",
    ".wsc": "Bandai - WonderSwan Color",
    ".ngp": "SNK - Neo Geo Pocket",
    ".ngc": "SNK - Neo Geo Pocket Color",
    ".col": "Coleco - ColecoVision",
}

# Curated exact Libretro Named_Boxarts filenames for baked ROMs & simulator showcase titles
CURATED_BOXART_CATALOG: List[Tuple[str, str, str]] = [
    # (slug / base_name, Libretro system, exact Libretro PNG filename)
    ("Amazing Spider-Man, The (USA, Europe)", "Nintendo - Game Boy", "Amazing Spider-Man, The (USA, Europe).png"),
    ("Kirby's Dream Land 2 (USA, Europe) (SGB Enhanced)", "Nintendo - Game Boy", "Kirby's Dream Land 2 (USA, Europe) (SGB Enhanced).png"),
    ("Legend of Zelda, The - Link's Awakening (USA, Europe) (Rev 2)", "Nintendo - Game Boy", "Legend of Zelda, The - Link's Awakening (USA, Europe) (Rev 2).png"),
    ("Sneaky Snakes (USA, Europe)", "Nintendo - Game Boy", "Sneaky Snakes (USA, Europe).png"),
    ("Super Mario Land (World) (Rev 1)", "Nintendo - Game Boy", "Super Mario Land (World) (Rev 1).png"),
    ("Tetris 2 (USA, Europe) (SGB Enhanced)", "Nintendo - Game Boy", "Tetris 2 (USA, Europe) (SGB Enhanced).png"),
    ("Tetris (World) (Rev A)", "Nintendo - Game Boy", "Tetris (World) (Rev 1).png"),
    ("Pokemon Red Version", "Nintendo - Game Boy", "Pokemon - Red Version (USA, Europe) (SGB Enhanced).png"),
    ("Teenage Mutant Ninja Turtles III - Radical Rescue", "Nintendo - Game Boy", "Teenage Mutant Ninja Turtles III - Radical Rescue (USA).png"),
    ("Alleyway", "Nintendo - Game Boy", "Alleyway (World).png"),
    ("Tamagotchi", "Nintendo - Game Boy", "Tamagotchi (USA, Europe) (SGB Enhanced).png"),
    ("Aladdin (USA)", "Nintendo - Game Boy Color", "Aladdin (USA).png"),
    ("Grand Theft Auto 2 (USA)", "Nintendo - Game Boy Color", "Grand Theft Auto 2 (USA).png"),
    ("Kirby - Tilt 'n' Tumble (USA)", "Nintendo - Game Boy Color", "Kirby - Tilt 'n' Tumble (USA).png"),
    ("Legend of Zelda, The - Oracle of Ages (USA, Australia)", "Nintendo - Game Boy Color", "Legend of Zelda, The - Oracle of Ages (USA, Australia).png"),
    ("Legend of Zelda, The - Oracle of Seasons (USA, Australia)", "Nintendo - Game Boy Color", "Legend of Zelda, The - Oracle of Seasons (USA, Australia).png"),
    ("LEGO Racers (USA) (En,Fr,Es)", "Nintendo - Game Boy Color", "LEGO Racers (USA) (En,Fr,Es).png"),
    ("Super Mario Bros. Deluxe (USA, Europe) (Rev 1)", "Nintendo - Game Boy Color", "Super Mario Bros. Deluxe (USA, Europe) (Rev 1).png"),
    ("Pokemon - Crystal Version", "Nintendo - Game Boy Color", "Pokemon - Crystal Version (USA, Europe) (Rev 1).png"),
    ("Donkey Kong Country", "Nintendo - Game Boy Color", "Donkey Kong Country (USA, Europe) (En,Fr,De,Es,It).png"),
    ("Street Fighter Alpha - Warriors' Dreams", "Nintendo - Game Boy Color", "Street Fighter Alpha - Warriors' Dreams (USA).png"),
    ("Super Mario Bros 3", "Nintendo - Nintendo Entertainment System", "Super Mario Bros. 3 (USA).png"),
    ("The Legend of Zelda", "Nintendo - Nintendo Entertainment System", "Legend of Zelda, The (USA).png"),
    ("Mega Man 2", "Nintendo - Nintendo Entertainment System", "Mega Man 2 (USA).png"),
    ("Castlevania III - Dracula's Curse", "Nintendo - Nintendo Entertainment System", "Castlevania III - Dracula's Curse (USA).png"),
    ("Chrono Trigger", "Nintendo - Super Nintendo Entertainment System", "Chrono Trigger (USA).png"),
    ("Super Metroid", "Nintendo - Super Nintendo Entertainment System", "Super Metroid (Japan, USA) (En,Ja).png"),
    ("Super Mario World", "Nintendo - Super Nintendo Entertainment System", "Super Mario World (USA).png"),
    ("Sonic the Hedgehog 2", "Sega - Mega Drive - Genesis", "Sonic The Hedgehog 2 (World) (Rev A).png"),
    ("Streets of Rage 2", "Sega - Mega Drive - Genesis", "Streets of Rage 2 (USA).png"),
    ("Gunstar Heroes", "Sega - Mega Drive - Genesis", "Gunstar Heroes (USA).png"),
    ("Phantasy Star IV", "Sega - Mega Drive - Genesis", "Phantasy Star IV (USA).png"),
    ("Phantasy Star", "Sega - Master System - Mark III", "Phantasy Star (USA, Europe) (Rev A).png"),
    ("Alex Kidd in Miracle World", "Sega - Master System - Mark III", "Alex Kidd in Miracle World (USA, Europe).png"),
    ("Wonder Boy III: The Dragon's Trap", "Sega - Master System - Mark III", "Wonder Boy III - The Dragon's Trap (USA, Europe).png"),
    ("Sonic the Hedgehog (8-bit)", "Sega - Master System - Mark III", "Sonic The Hedgehog (USA, Europe, Brazil) (En).png"),
    ("Sonic the Hedgehog: Triple Trouble", "Sega - Game Gear", "Sonic The Hedgehog - Triple Trouble (USA, Europe, Brazil) (En).png"),
    ("Shinobi II: The Silent Fury", "Sega - Game Gear", "Shinobi II - The Silent Fury (World).png"),
    ("Defenders of Oasis", "Sega - Game Gear", "Defenders of Oasis (USA, Europe).png"),
    ("Soldier Blade", "NEC - PC Engine - TurboGrafx 16", "Soldier Blade (USA).png"),
    ("Bonk's Adventure", "NEC - PC Engine - TurboGrafx 16", "Bonk's Adventure (USA).png"),
    ("Blazing Lazers (Gunhed)", "NEC - PC Engine - TurboGrafx 16", "Blazing Lazers (USA).png"),
    ("Pitfall!", "Atari - 2600", "Pitfall! - Pitfall Harry's Jungle Adventure (USA).png"),
    ("Space Invaders", "Atari - 2600", "Space Invaders (USA).png"),
    ("River Raid", "Atari - 2600", "River Raid (USA).png"),
    ("Adventure", "Atari - 2600", "Adventure (USA).png"),
    ("Donkey Kong", "Coleco - ColecoVision", "Donkey Kong (USA).png"),
    ("Zaxxon", "Coleco - ColecoVision", "Zaxxon (USA, Europe).png"),
    ("Blue Lightning", "Atari - Lynx", "Blue Lightning (USA, Europe).png"),
    ("California Games", "Atari - Lynx", "California Games (USA, Europe).png"),
    ("Todd's Adventures in Slime World", "Atari - Lynx", "Todd's Adventures in Slime World (USA, Europe).png"),
    ("Chip's Challenge", "Atari - Lynx", "Chip's Challenge (USA, Europe).png"),
    ("SNK vs. Capcom: Card Fighters Clash", "SNK - Neo Geo Pocket Color", "SNK vs. Capcom - Card Fighters' Clash - SNK Version (USA, Europe).png"),
    ("Match of the Millennium (SvC)", "SNK - Neo Geo Pocket Color", "SNK vs. Capcom - The Match of the Millennium (World) (En,Ja).png"),
    ("Metal Slug: 1st & 2nd Mission", "SNK - Neo Geo Pocket Color", "Metal Slug - 1st Mission (World) (En,Ja).png"),
    ("Sonic the Hedgehog Pocket Adventure", "SNK - Neo Geo Pocket Color", "Sonic The Hedgehog - Pocket Adventure (World).png"),
    ("Klonoa: Moonlight Museum", "Bandai - WonderSwan", "Kaze no Klonoa - Moonlight Museum (Japan).png"),
    ("Final Fantasy IV (WSC)", "Bandai - WonderSwan Color", "Final Fantasy IV (Japan).png"),
    ("Judgement Silversword", "Bandai - WonderSwan Color", "Judgement Silversword - Rebirth Edition (Japan) (Rev 4321).png"),
    ("DOOM", "Nintendo - Super Nintendo Entertainment System", "Doom (USA).png"),
]


def rgb888_to_bgr565_swapped(r: int, g: int, b: int) -> int:
    """Packs 8-bit RGB into byte-swapped 16-bit BGR565 matching Theme::makeUiColor / BoxArt."""
    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3)
    return ((rgb565 & 0xFF) << 8) | ((rgb565 >> 8) & 0xFF)


def generate_procedural_cover_rgb(
    title: str, width: int = COVER_WIDTH, height: int = COVER_HEIGHT
) -> List[Tuple[int, int, int]]:
    """Generates a deterministic retro cartridge cover art grid (RGB888) when offline."""
    digest = hashlib.sha256(title.encode("utf-8", errors="ignore")).digest()
    accent_r = 60 + (digest[0] % 180)
    accent_g = 60 + (digest[1] % 180)
    accent_b = 60 + (digest[2] % 180)
    bg_teal = (31, 78, 72)
    border_gold = (255, 224, 51)
    mint_screen = (206, 245, 228)

    pixels: List[Tuple[int, int, int]] = []
    for y in range(height):
        for x in range(width):
            if x < 2 or x >= width - 2 or y < 2 or y >= height - 2:
                pixels.append(border_gold)
            elif (width // 8) <= x < (width - width // 8) and (height // 8) <= y < (height * 3 // 5):
                pattern = ((x * 3 + digest[3]) ^ (y * 5 + digest[4])) & 0x1F
                if pattern < 6:
                    pixels.append(mint_screen)
                else:
                    pixels.append((accent_r, accent_g, accent_b))
            else:
                pixels.append(bg_teal)
    return pixels


def encode_raw_bgr565(
    pixels_rgb: Iterable[Tuple[int, int, int]],
    expected_bytes: Optional[int] = RAW_BYTE_SIZE,
) -> bytes:
    """Encodes RGB888 pixels into little-endian uint16_t swapped BGR565."""
    out = bytearray()
    for r, g, b in pixels_rgb:
        val = rgb888_to_bgr565_swapped(r, g, b)
        out.extend(struct.pack("<H", val))
    if expected_bytes is not None and len(out) != expected_bytes:
        raise ValueError(f"Expected {expected_bytes} bytes, got {len(out)}")
    return bytes(out)


def encode_bmp_24bit(
    pixels_rgb: List[Tuple[int, int, int]],
    width: int = COVER_WIDTH,
    height: int = COVER_HEIGHT,
) -> bytes:
    """Encodes RGB888 pixels into a standard 24-bit bottom-up BMP image."""
    row_stride = (width * 3 + 3) & ~3
    data_size = row_stride * height
    file_size = 54 + data_size

    header = bytearray(54)
    header[0:2] = b"BM"
    struct.pack_into("<I", header, 2, file_size)
    struct.pack_into("<I", header, 10, 54)
    struct.pack_into("<I", header, 14, 40)
    struct.pack_into("<i", header, 18, width)
    struct.pack_into("<i", header, 22, height)  # bottom-up
    struct.pack_into("<H", header, 26, 1)
    struct.pack_into("<H", header, 28, 24)
    struct.pack_into("<I", header, 34, data_size)

    body = bytearray(data_size)
    for y in range(height):
        src_row = height - 1 - y
        row_offset = y * row_stride
        for x in range(width):
            r, g, b = pixels_rgb[src_row * width + x]
            px_off = row_offset + x * 3
            body[px_off] = b & 0xFF
            body[px_off + 1] = g & 0xFF
            body[px_off + 2] = r & 0xFF

    return bytes(header) + bytes(body)


def image_to_pixels_rgb(
    img_path: Path, width: int = COVER_WIDTH, height: int = COVER_HEIGHT
) -> Optional[List[Tuple[int, int, int]]]:
    """Converts a high-resolution PNG/JPG image into Lanczos-resampled RGB888 pixels."""
    if Image is None or not img_path.exists():
        return None
    with Image.open(img_path) as im:
        rgb = im.convert("RGB")
        resized = rgb.resize((width, height), Image.Resampling.LANCZOS)
        raw_bytes = resized.tobytes()
        return [
            (raw_bytes[i], raw_bytes[i + 1], raw_bytes[i + 2])
            for i in range(0, len(raw_bytes), 3)
        ]


def write_cover_files(
    base_name: str,
    out_dir: Path,
    pixels_rgb: Optional[List[Tuple[int, int, int]]] = None,
    width: int = COVER_WIDTH,
    height: int = COVER_HEIGHT,
    rom_relative_path: Optional[str] = None,
) -> Tuple[Path, Path]:
    """Writes both .raw and .bmp cover files using a ROM path key when supplied."""
    out_dir.mkdir(parents=True, exist_ok=True)
    if pixels_rgb is None:
        pixels_rgb = generate_procedural_cover_rgb(base_name, width, height)
    output_name = make_rom_key(rom_relative_path) if rom_relative_path is not None else base_name
    raw_path = out_dir / f"{output_name}.raw"
    bmp_path = out_dir / f"{output_name}.bmp"
    raw_path.write_bytes(encode_raw_bgr565(pixels_rgb, expected_bytes=width * height * 2))
    bmp_path.write_bytes(encode_bmp_24bit(pixels_rgb, width=width, height=height))
    return raw_path, bmp_path


def build_libretro_thumbnail_url(rom_filename: str) -> Optional[str]:
    """Builds a Libretro Thumbnails CDN URL for a given ROM filename."""
    path = Path(rom_filename)
    ext = path.suffix.lower()
    system = LIBRETRO_SYSTEM_MAP.get(ext)
    if not system:
        return None
    clean_stem = re.sub(r'[&*/:`<>?\\|"]', "_", path.stem)
    encoded_sys = urllib.parse.quote(system)
    encoded_name = urllib.parse.quote(f"{clean_stem}.png")
    return f"https://thumbnails.libretro.com/{encoded_sys}/Named_Boxarts/{encoded_name}"


def download_libretro_hd_png(system_name: str, png_filename: str, dest_path: Path, _depth: int = 0) -> bool:
    """Downloads the original full-resolution PNG box art from Libretro CDN (following symlink pointers)."""
    if dest_path.exists() and dest_path.stat().st_size > 4096:
        return True
    clean_name = re.sub(r'[&*/:`<>?\\|"]', "_", png_filename)
    url = (
        f"https://thumbnails.libretro.com/"
        f"{urllib.parse.quote(system_name)}/Named_Boxarts/{urllib.parse.quote(clean_name)}"
    )
    req = urllib.request.Request(url, headers={"User-Agent": "BMO-CoverFetcher/2.0"})
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            data = resp.read()
            if len(data) > 1024 and data[:8] == b"\x89PNG\r\n\x1a\n":
                dest_path.parent.mkdir(parents=True, exist_ok=True)
                dest_path.write_bytes(data)
                return True
            if _depth < 2 and 4 <= len(data) < 256:
                target = data.decode("utf-8", errors="ignore").strip()
                if target.lower().endswith(".png"):
                    return download_libretro_hd_png(system_name, target, dest_path, _depth + 1)
    except Exception:
        return False
    return False


def build_virtual_bmo_hd_cover(dest_path: Path) -> bool:
    """Builds a 640x640 high-resolution box art PNG for Virtual BMO from official GBS assets."""
    if Image is None:
        return False
    if dest_path.exists() and dest_path.stat().st_size > 4096:
        return True
    zpath = REPO_ROOT / "resources" / "Virtual BMO GBS Source Code.zip"
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    canvas = Image.new("RGB", (640, 640), (26, 75, 66))
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle((12, 12, 627, 627), radius=28, fill=(95, 180, 156), outline=(255, 224, 51), width=8)
    draw.rounded_rectangle((36, 36, 603, 603), radius=20, fill=(15, 38, 32), outline=(26, 75, 66), width=4)

    if zpath.exists():
        try:
            with zipfile.ZipFile(zpath) as zf:
                title_bytes = zf.read("BMO Simulator v3.0/assets/backgrounds/Guardians_of_suncshine_title.png")
                desk_bytes = zf.read("BMO Simulator v3.0/assets/backgrounds/BMO Desktop.png")
                title_im = Image.open(io.BytesIO(title_bytes)).convert("RGB").resize((520, 260), Image.Resampling.NEAREST)
                desk_im = Image.open(io.BytesIO(desk_bytes)).convert("RGB").resize((520, 260), Image.Resampling.NEAREST)
                canvas.paste(title_im, (60, 56))
                canvas.paste(desk_im, (60, 324))
        except Exception:
            pass

    canvas.save(dest_path, format="PNG", optimize=True)
    return True


def slugify_title(title: str) -> str:
    """Produces a clean filesystem/URL slug for simulator cover lookup."""
    s = Path(title).stem.lower()
    s = re.sub(r"\([^)]*\)|\[[^\]]*\]", "", s)
    s = re.sub(r"[^a-z0-9]+", "_", s).strip("_")
    return s or "cover"


def sync_all_covers(
    rom_dir: Path = REPO_ROOT / "roms",
    sd_boxart_dir: Path = REPO_ROOT / "roms" / "boxart",
    sim_covers_dir: Path = REPO_ROOT / "tools" / "bmo_simulator" / "covers",
    target_width: int = COVER_WIDTH,
    target_height: int = COVER_HEIGHT,
    offline: bool = False,
) -> Dict[str, str]:
    """Downloads highest-resolution original PNG covers and generates SD .raw/.bmp + simulator PNGs."""
    hd_dir = sd_boxart_dir / "hd"
    hd_dir.mkdir(parents=True, exist_ok=True)
    sd_boxart_dir.mkdir(parents=True, exist_ok=True)
    sim_covers_dir.mkdir(parents=True, exist_ok=True)

    manifest: Dict[str, str] = {}

    # 1. Virtual BMO Official Game high-resolution cover
    vbmo_hd = hd_dir / "Virtual BMO (Official Game).png"
    if build_virtual_bmo_hd_cover(vbmo_hd):
        vbmo_sim = sim_covers_dir / "virtual_bmo.png"
        shutil.copy2(vbmo_hd, vbmo_sim)
        px = image_to_pixels_rgb(vbmo_hd, target_width, target_height)
        write_cover_files("Virtual BMO (Official Game)", sd_boxart_dir, px, target_width, target_height)
        write_cover_files("virtual_bmo", sd_boxart_dir, px, target_width, target_height)
        manifest["Virtual BMO (Official Game)"] = "covers/virtual_bmo.png"

    # 2. Download curated high-resolution original box arts
    for base_name, sys_name, libretro_png in CURATED_BOXART_CATALOG:
        hd_png = hd_dir / f"{base_name}.png"
        if not offline:
            download_libretro_hd_png(sys_name, libretro_png, hd_png)
        if hd_png.exists():
            slug = slugify_title(base_name)
            sim_png = sim_covers_dir / f"{slug}.png"
            shutil.copy2(hd_png, sim_png)
            px = image_to_pixels_rgb(hd_png, target_width, target_height)
            matching_roms = []
            if rom_dir.exists():
                matching_roms = [
                    path for path in rom_dir.rglob("*")
                    if path.is_file() and "boxart" not in path.parts
                    and path.stem.casefold() == base_name.casefold()
                ]
            if matching_roms:
                for rom_path in matching_roms:
                    relative_path = rom_path.relative_to(rom_dir).as_posix()
                    if len(relative_path.encode("utf-8")) >= 64:
                        raise ValueError(f"ROM path is too long for firmware index: {relative_path}")
                    write_cover_files(
                        base_name, sd_boxart_dir, px, target_width, target_height,
                        rom_relative_path=relative_path,
                    )
            else:
                write_cover_files(base_name, sd_boxart_dir, px, target_width, target_height)
            manifest[base_name] = f"covers/{slug}.png"

    # 3. Also scan any additional ROM files in rom_dir
    if rom_dir.exists():
        for rom_path in sorted(rom_dir.rglob("*")):
            if not rom_path.is_file() or "boxart" in rom_path.parts:
                continue
            ext = rom_path.suffix.lower()
            sys_name = LIBRETRO_SYSTEM_MAP.get(ext)
            if not sys_name:
                continue
            stem = rom_path.stem
            hd_png = hd_dir / f"{stem}.png"
            if not hd_png.exists() and not offline:
                download_libretro_hd_png(sys_name, f"{stem}.png", hd_png)
            px = image_to_pixels_rgb(hd_png, target_width, target_height) if hd_png.exists() else None
            relative_path = rom_path.relative_to(rom_dir).as_posix()
            if len(relative_path.encode("utf-8")) >= 64:
                raise ValueError(f"ROM path is too long for firmware index: {relative_path}")
            write_cover_files(
                stem, sd_boxart_dir, px, target_width, target_height,
                rom_relative_path=relative_path,
            )
            if hd_png.exists():
                slug = slugify_title(stem)
                sim_png = sim_covers_dir / f"{slug}.png"
                if not sim_png.exists():
                    shutil.copy2(hd_png, sim_png)
                manifest[stem] = f"covers/{slug}.png"

    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Download original high-resolution box art PNGs and convert to BMO .raw/.bmp covers."
    )
    parser.add_argument("--rom-dir", type=Path, default=REPO_ROOT / "roms", help="Directory containing ROM files")
    parser.add_argument("--out-dir", type=Path, default=REPO_ROOT / "roms" / "boxart", help="Output directory for covers")
    parser.add_argument("--sd-root", type=Path, default=None, help="Optional SD card root path")
    parser.add_argument("--title", type=str, default=None, help="Generate cover for a single ROM title/basename")
    parser.add_argument("--target-size", type=str, default="64x64", help="Target WxH for .raw/.bmp conversion (default: 64x64)")
    parser.add_argument("--offline", action="store_true", help="Do not query network; use cached HD covers or procedural fallback")
    args = parser.parse_args()

    out_dir = (args.sd_root / "boxart") if args.sd_root else args.out_dir
    m = re.match(r"^(\d+)x(\d+)$", args.target_size.lower().strip())
    tw, th = (int(m.group(1)), int(m.group(2))) if m else (COVER_WIDTH, COVER_HEIGHT)

    if args.title:
        base = Path(args.title).stem
        raw_p, bmp_p = write_cover_files(base, out_dir, width=tw, height=th)
        print(f"Generated {raw_p} and {bmp_p}")
        return 0

    manifest = sync_all_covers(
        rom_dir=args.rom_dir,
        sd_boxart_dir=out_dir,
        target_width=tw,
        target_height=th,
        offline=args.offline,
    )
    print(f"Synchronized {len(manifest)} high-resolution original cover arts and {tw}x{th} .raw/.bmp pairs in {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
