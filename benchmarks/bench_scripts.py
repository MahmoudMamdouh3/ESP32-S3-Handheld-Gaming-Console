# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Host benchmarks for Python asset conversion and SD deployment scripts."""

from __future__ import annotations

import importlib
import json
import sys
import tempfile
import time
from pathlib import Path
from typing import Any

from PIL import Image

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))


def _create_fixture_tree(root: Path, count: int) -> tuple[Path, Path, Path, Path]:
    rom_dir = root / "roms"
    covers_dir = root / "covers"
    boxart_dir = root / "boxart"
    sd_dir = root / "sd"
    rom_dir.mkdir(parents=True)
    covers_dir.mkdir(parents=True)
    boxart_dir.mkdir(parents=True)
    sd_dir.mkdir(parents=True)

    for i in range(count):
        stem = f"Fixture_Game_{i:03d}"
        sub = rom_dir / ("gb" if i % 2 == 0 else "nes")
        sub.mkdir(exist_ok=True)
        ext = ".gb" if i % 2 == 0 else ".nes"
        (sub / f"{stem}{ext}").write_bytes(bytes([(i + j) & 0xFF for j in range(16384)]))

        img = Image.new("RGB", (128, 128), ((i * 37) % 255, (i * 71) % 255, (i * 113) % 255))
        img.save(covers_dir / f"{stem}.png")

    return rom_dir, covers_dir, boxart_dir, sd_dir


def run_script_benchmarks(fixture_count: int = 40) -> dict[str, Any]:
    from scripts import convert_covers

    deploy_mod: Any
    try:
        deploy_mod = importlib.import_module("scripts.deploy_sd")
    except ImportError:
        deploy_mod = importlib.import_module("scripts.deploy_to_sd")

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        rom_dir, covers_dir, boxart_dir, sd_dir = _create_fixture_tree(root, fixture_count)

        t0 = time.perf_counter()
        convert_covers.sync_all_covers(
            rom_dir=rom_dir,
            sd_boxart_dir=boxart_dir,
            covers_dir=covers_dir,
        )
        convert_ms = (time.perf_counter() - t0) * 1000.0

        tasks = deploy_mod.deployment_tasks(rom_dir, boxart_dir, sd_dir)

        t1 = time.perf_counter()
        updates_cold, _, _ = deploy_mod.deployment_plan(tasks, report_progress=False)
        for src, dst in updates_cold:
            deploy_mod.copy_single_file(src, dst)
        deploy_cold_ms = (time.perf_counter() - t1) * 1000.0

        # Modify half of the sources slightly to measure incremental sync
        for idx, (src, _) in enumerate(tasks):
            if idx % 4 == 0 and src.suffix.lower() in {".gb", ".nes"}:
                data = bytearray(src.read_bytes())
                data[0] ^= 0xFF
                src.write_bytes(bytes(data))

        t2 = time.perf_counter()
        updates_inc, _, _ = deploy_mod.deployment_plan(tasks, report_progress=False)
        for src, dst in updates_inc:
            deploy_mod.copy_single_file(src, dst)
        deploy_incremental_ms = (time.perf_counter() - t2) * 1000.0

    return {
        "fixture_count": fixture_count,
        "convert_covers_ms": round(convert_ms, 2),
        "deploy_sd_cold_ms": round(deploy_cold_ms, 2),
        "deploy_sd_incremental_ms": round(deploy_incremental_ms, 2),
        "deploy_tasks": len(tasks),
    }


def main() -> int:
    payload = run_script_benchmarks()
    print(json.dumps(payload, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
