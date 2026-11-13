"""
scripts/fetch_covers.py — Local Box Art Resampler & Multi-Target BGR565 Converter
Converts user-supplied PNG/JPG cover artwork via Lanczos resampling into 64x64 24-bit BMP (.bmp)
and 8,192-byte pre-swapped BGR565 (.raw) files compatible with BoxArt::load() (src/core/box_art.cpp).
"""

import argparse
import hashlib
import re
import shutil
import struct
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

SCRIPTS_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPTS_DIR.parent
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

from repo_tools import get_data_dir
from scripts.rom_key import make_rom_key

try:
    from PIL import Image
except ImportError:
    Image = None  # type: ignore

COVER_WIDTH = 64
COVER_HEIGHT = 64
RAW_BYTE_SIZE = COVER_WIDTH * COVER_HEIGHT * 2  # 8,192 bytes

SUPPORTED_ROM_EXTENSIONS = frozenset({
    ".gb", ".gbc", ".nes", ".wad", ".sms", ".gg", ".pce", ".a26",
    ".p8", ".gen", ".md", ".smd", ".sfc", ".smc", ".ws", ".wsc",
    ".ngp", ".ngc", ".lnx", ".col", ".sg",
})


def rgb888_to_bgr565_swapped(r: int, g: int, b: int) -> int:
    """Packs 8-bit RGB into byte-swapped 16-bit BGR565 matching Theme::makeUiColor / BoxArt."""
    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3)
    return ((rgb565 & 0xFF) << 8) | ((rgb565 >> 8) & 0xFF)


def generate_procedural_cover_rgb(
    title: str, width: int = COVER_WIDTH, height: int = COVER_HEIGHT
) -> List[Tuple[int, int, int]]:
    """Generates a deterministic retro cartridge cover art grid (RGB888) when no local artwork is supplied."""
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
    """Converts a local PNG/JPG image into Lanczos-resampled RGB888 pixels."""
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


def slugify_title(title: str) -> str:
    """Produces a clean filesystem/URL slug for simulator cover lookup."""
    s = Path(title).stem.lower()
    s = re.sub(r"\([^)]*\)|\[[^\]]*\]", "", s)
    s = re.sub(r"[^a-z0-9]+", "_", s).strip("_")
    return s or "cover"


def _find_local_source_image(stem: str, search_dirs: Iterable[Path]) -> Optional[Path]:
    for directory in search_dirs:
        if not directory.is_dir():
            continue
        for ext in (".png", ".jpg", ".jpeg", ".bmp"):
            candidate = directory / f"{stem}{ext}"
            if candidate.is_file():
                return candidate
    return None


def sync_all_covers(
    rom_dir: Optional[Path] = None,
    sd_boxart_dir: Optional[Path] = None,
    covers_dir: Optional[Path] = None,
    sim_covers_dir: Optional[Path] = None,
    target_width: int = COVER_WIDTH,
    target_height: int = COVER_HEIGHT,
) -> Dict[str, str]:
    """Converts local user-supplied cover images or generates procedural covers into SD .raw/.bmp pairs."""
    data_dir = get_data_dir(REPO_ROOT)
    resolved_rom_dir = rom_dir if rom_dir is not None else (data_dir / "roms")
    resolved_boxart_dir = sd_boxart_dir if sd_boxart_dir is not None else (data_dir / "roms" / "boxart")
    hd_dir = resolved_boxart_dir / "hd"
    search_dirs = [p for p in (covers_dir, hd_dir) if p is not None]

    resolved_boxart_dir.mkdir(parents=True, exist_ok=True)
    if sim_covers_dir is not None:
        sim_covers_dir.mkdir(parents=True, exist_ok=True)

    manifest: Dict[str, str] = {}
    if not resolved_rom_dir.exists():
        return manifest

    for rom_path in sorted(resolved_rom_dir.rglob("*")):
        if not rom_path.is_file() or "boxart" in rom_path.parts:
            continue
        if rom_path.suffix.lower() not in SUPPORTED_ROM_EXTENSIONS:
            continue

        stem = rom_path.stem
        local_img = _find_local_source_image(stem, search_dirs)
        px = image_to_pixels_rgb(local_img, target_width, target_height) if local_img else None
        relative_path = rom_path.relative_to(resolved_rom_dir).as_posix()
        if len(relative_path.encode("utf-8")) >= 64:
            raise ValueError(f"ROM path is too long for firmware index: {relative_path}")

        write_cover_files(
            stem,
            resolved_boxart_dir,
            px,
            target_width,
            target_height,
            rom_relative_path=relative_path,
        )
        if local_img and sim_covers_dir is not None:
            slug = slugify_title(stem)
            sim_dest = sim_covers_dir / f"{slug}{local_img.suffix.lower()}"
            if not sim_dest.exists():
                shutil.copy2(local_img, sim_dest)
            manifest[stem] = f"covers/{sim_dest.name}"

    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert local user-supplied cover artwork into BMO .raw/.bmp box art pairs."
    )
    parser.add_argument("--rom-dir", type=Path, default=None, help="Directory containing ROM files")
    parser.add_argument("--out-dir", type=Path, default=None, help="Output directory for converted covers")
    parser.add_argument("--covers-dir", type=Path, default=None, help="Directory of user-supplied PNG/JPG cover art")
    parser.add_argument("--sd-root", type=Path, default=None, help="Optional SD card root path")
    parser.add_argument("--title", type=str, default=None, help="Generate cover for a single ROM title/basename")
    parser.add_argument("--target-size", type=str, default="64x64", help="Target WxH for .raw/.bmp conversion (default: 64x64)")
    args = parser.parse_args()

    data_dir = get_data_dir(REPO_ROOT)
    default_out = data_dir / "roms" / "boxart"
    out_dir = (args.sd_root / "boxart") if args.sd_root else (args.out_dir or default_out)
    m = re.match(r"^(\d+)x(\d+)$", args.target_size.lower().strip())
    tw, th = (int(m.group(1)), int(m.group(2))) if m else (COVER_WIDTH, COVER_HEIGHT)

    if args.title:
        base = Path(args.title).stem
        local_img = _find_local_source_image(base, [p for p in (args.covers_dir, out_dir / "hd") if p])
        px = image_to_pixels_rgb(local_img, tw, th) if local_img else None
        raw_p, bmp_p = write_cover_files(base, out_dir, pixels_rgb=px, width=tw, height=th)
        print(f"Generated {raw_p} and {bmp_p}")
        return 0

    manifest = sync_all_covers(
        rom_dir=args.rom_dir or (data_dir / "roms"),
        sd_boxart_dir=out_dir,
        covers_dir=args.covers_dir,
        target_width=tw,
        target_height=th,
    )
    print(f"Converted {len(manifest)} local cover image(s) and wrote {tw}x{th} .raw/.bmp pairs in {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
