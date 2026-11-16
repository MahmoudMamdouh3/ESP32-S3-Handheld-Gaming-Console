#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Backward-compatible wrapper delegating to scripts/sync_ai_codemap.py."""

from __future__ import annotations

import sys
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parent
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

from sync_ai_codemap import build_codemap, main  # noqa: E402

__all__ = ["build_codemap", "main"]

if __name__ == "__main__":
    raise SystemExit(main(["--write", *sys.argv[1:]]))
