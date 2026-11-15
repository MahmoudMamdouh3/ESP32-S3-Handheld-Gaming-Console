"""
scripts/fetch_covers.py — Backward-compatible alias for scripts/convert_covers.py.
This tool converts local user-supplied PNG/JPG images (or procedural fallbacks) into
64x64 24-bit BMP (.bmp) and BGR565 (.raw) box art files. It performs NO network downloads.
Prefer invoking `python scripts/convert_covers.py`.
"""

from __future__ import annotations

import sys
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parent
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

from convert_covers import (  # noqa: F401
    COVER_HEIGHT,
    COVER_WIDTH,
    RAW_BYTE_SIZE,
    REPO_ROOT,
    SUPPORTED_ROM_EXTENSIONS,
    encode_bmp_24bit,
    encode_raw_bgr565,
    generate_procedural_cover_rgb,
    image_to_pixels_rgb,
    main,
    rgb888_to_bgr565_swapped,
    slugify_title,
    sync_all_covers,
    write_cover_files,
)

if __name__ == "__main__":
    raise SystemExit(main())
