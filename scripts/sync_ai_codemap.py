#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Generate and verify docs/ai/CODEMAP.md (<= 200 lines) and docs/ai/STATE.md (<= 60 lines)."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
CODEMAP_PATH = REPO_ROOT / "docs" / "ai" / "CODEMAP.md"
STATE_PATH = REPO_ROOT / "docs" / "ai" / "STATE.md"

KNOWN_DESCRIPTIONS: dict[str, str] = {
    "firmware/BmoGameboy/BmoGameboy.ino": "Arduino entry sketch delegating setup() and loop() to BmoApp",
    "firmware/bringup/bringup.ino": "Standalone first-power-on bring-up sketch (PSRAM, TFT, SD, 8 buttons)",
    "firmware/BmoGameboy/src/app/bmo_app.cpp": "Top-level coordinator, setup(), and loop() dispatcher",
    "firmware/BmoGameboy/src/app/app_state.cpp": "Application FSM state, boot recovery, and Konami state machine",
    "firmware/BmoGameboy/src/app/app_launcher.cpp": "ROM loader, identity check, and core launch/teardown",
    "firmware/BmoGameboy/src/app/app_menus.cpp": "Console carousel, game list, museum modal, and idle mascot handlers",
    "firmware/BmoGameboy/src/app/app_diagnostics.cpp": "Diagnostics dashboard, SPI stress pattern, and profiler toggle",
    "firmware/BmoGameboy/src/app/app_emulator_loop.cpp": "Emulator step loop, frame pacing, pause menu, and quit policy",
    "firmware/BmoGameboy/src/app/boot_safe_mode.h": "40 MHz SPI Safe Mode decision helper and reset-reason classifier",
    "firmware/BmoGameboy/src/hal/config.h": "Umbrella HAL configuration re-exporting pins, timing, and flags",
    "firmware/BmoGameboy/src/hal/pin_map.h": "Authoritative soldered GPIO pin assignments",
    "firmware/BmoGameboy/src/hal/board_timing.h": "SPI clock rates, display geometry, and LOG_* macros",
    "firmware/BmoGameboy/src/hal/feature_flags.h": "Hardware hard-stop flags and optional core compile switches",
    "firmware/BmoGameboy/src/hal/pins_check.h": "Compile-time static_assert checks for GPIO uniqueness and reserved pins",
    "firmware/BmoGameboy/src/hal/spi_arbiter.cpp": "FreeRTOS recursive mutex serializing shared FSPI (ST7789 + MicroSD)",
    "firmware/BmoGameboy/src/hal/buttons.cpp": "GPIO_IN_REG / GPIO_IN1_REG 12-button active-low debouncer",
    "firmware/BmoGameboy/src/hal/psram_alloc.h": "Octal PSRAM allocation wrappers with overflow and DRAM guards",
    "firmware/BmoGameboy/src/hal/profiler.cpp": "PSRAM-backed frame ring profiler and summary snapshotter",
    "firmware/BmoGameboy/src/hal/battery.cpp": "Dormant ADC battery monitor (gated off by FEATURE_BATTERY_MONITOR=0)",
    "firmware/BmoGameboy/src/hal/audio_i2s.cpp": "Dormant I2S DAC audio driver (gated off by FEATURE_AUDIO=0)",
    "firmware/BmoGameboy/src/storage/sd_mount.cpp": "MicroSD mount, directory scaffolding, and PSRAM catalog pool sizing",
    "firmware/BmoGameboy/src/storage/sd_catalog.cpp": "BMOIDX03 index reader/writer, FAT scan, and ROM loader",
    "firmware/BmoGameboy/src/storage/sd_index_io.cpp": "BMOIDX03 binary serialization, streaming CRC verify, and atomic save",
    "firmware/BmoGameboy/src/storage/sd_scanner.cpp": "Recursive FAT32 ROM scanner and file filter pipeline",
    "firmware/BmoGameboy/src/storage/rom_catalog_index.cpp": "In-memory PSRAM catalog storage and indexed path lookup",
    "firmware/BmoGameboy/src/storage/sd_metadata.cpp": "Atomic favorites.txt and history.txt persistence (PSRAM-backed)",
    "firmware/BmoGameboy/src/storage/popular_games_table.h": "Per-platform curated Top Picks keyword matching table",
    "firmware/BmoGameboy/src/storage/save_manager.cpp": "Multi-slot BSS2 save-state and buffered BMP screenshot manager",
    "firmware/BmoGameboy/src/storage/save_state_format.h": "BSS2 72-byte versioned save-state header and CRC32 validator",
    "firmware/BmoGameboy/src/storage/rom_index.h": "BMOIDX03 44-byte header and 20-byte entry binary catalog schema",
    "firmware/BmoGameboy/src/storage/path_key.h": "Deterministic <stem>_<8hex_crc32> sidecar key generator",
    "firmware/BmoGameboy/src/storage/atomic_fs.h": "Write-temp-verify-rename atomic SD file transaction engine",
    "firmware/BmoGameboy/src/ui/display_emu.cpp": "Carousel, game list, museum, pause HUD, and toast UI renderer",
    "firmware/BmoGameboy/src/ui/display_worker.cpp": "CPU Core 0 FreeRTOS display streaming task and viewport blitters",
    "firmware/BmoGameboy/src/ui/display_driver.cpp": "ST7789 SPI initialization and internal-DRAM DMA bounce strip blitter",
    "firmware/BmoGameboy/src/ui/display_pipeline.cpp": "Core 0/1 atomic display worker queue, palette cache, and scalers",
    "firmware/BmoGameboy/src/ui/bmo_face.cpp": "Procedural SDF BMO mascot face renderer with harmonic spring physics",
    "firmware/BmoGameboy/src/ui/bmo_face_sdf.cpp": "SDF primitive evaluation and expression target tables",
    "firmware/BmoGameboy/src/ui/bmo_face_renderer.cpp": "Scanline SDF rasterizer, AABB culling, and particle compositing",
    "firmware/BmoGameboy/src/ui/bmo_face_anim.cpp": "Mascot spring dynamics, gaze/blink timers, speech, and dance state",
    "firmware/BmoGameboy/src/ui/theme.h": "Canonical BGR565 wire-swapped color tokens and spacing constants",
    "firmware/BmoGameboy/src/ui/console_catalog.h": "15-platform metadata catalog, file extensions, and badge themes",
    "firmware/BmoGameboy/src/cores/core_registry.h": "Static dispatch table mapping emulator indices 0..14 to C++ wrappers",
    "firmware/BmoGameboy/src/cores/gb_common.h": "Shared Game Boy / GBC cart RAM, scaler, and state helpers",
    "firmware/BmoGameboy/src/cores/gb_common.cpp": "Shared Game Boy / GBC core runtime utilities",
}

CORE_META: dict[str, tuple[str, str]] = {
    "emu_peanut.cpp": ("`peanut_gb` (GB DMG)", "Always enabled (`mit` profile)"),
    "emu_walnut.cpp": ("`walnut_cgb` (GBC)", "Always enabled (`mit` profile)"),
    "emu_nes.cpp": ("`agnes` (NES)", "Always enabled (`mit` profile)"),
    "emu_doom.cpp": ("`doom` (`doomgeneric`)", "`BMO_ENABLE_DOOM`"),
    "emu_sms.cpp": ("`smsplus` (SMS / GG)", "`BMO_EXPERIMENTAL_CORES`"),
    "emu_colem.cpp": ("`smsplus` / `colem` (ColecoVision)", "`BMO_EXPERIMENTAL_CORES`"),
    "emu_pce.cpp": ("`huexpress` (PC Engine)", "`BMO_EXPERIMENTAL_CORES`"),
    "emu_atari.cpp": ("`stella2014` (Atari 2600)", "`BMO_EXPERIMENTAL_CORES`"),
    "emu_pico.cpp": ("`fake08` + `z8lua` (PICO-8)", "`BMO_EXPERIMENTAL_CORES`"),
    "emu_genesis.cpp": ("`gwenesis` (Sega Genesis)", "`BMO_EXPERIMENTAL_CORES`"),
    "emu_snes.cpp": ("`snes9x` (SNES)", "`BMO_EXPERIMENTAL_CORES`"),
    "emu_wswan.cpp": ("`oswan` (WonderSwan)", "`BMO_EXPERIMENTAL_CORES`"),
    "emu_ngp.cpp": ("`race` (Neo Geo Pocket)", "`BMO_EXPERIMENTAL_CORES`"),
    "emu_lynx.cpp": ("`handy` (Atari Lynx)", "`BMO_EXPERIMENTAL_CORES`"),
}


def count_lines(path: Path) -> int:
    try:
        return len(path.read_text(encoding="utf-8", errors="ignore").splitlines())
    except OSError:
        return 0


def _collect_first_party_firmware() -> list[Path]:
    src_root = REPO_ROOT / "firmware" / "BmoGameboy" / "src"
    entries: list[Path] = [
        REPO_ROOT / "firmware" / "BmoGameboy" / "BmoGameboy.ino",
        REPO_ROOT / "firmware" / "bringup" / "bringup.ino",
    ]
    for sub in ("app", "hal", "storage", "ui"):
        d = src_root / sub
        if not d.is_dir():
            continue
        for p in sorted(d.iterdir()):
            if not p.is_file():
                continue
            if p.suffix.lower() not in {".cpp", ".c", ".h", ".hpp"}:
                continue
            rel = p.relative_to(REPO_ROOT).as_posix()
            # Prefer .cpp/.ino + standalone key headers so CODEMAP stays <= 200 lines.
            if p.suffix.lower() in {".cpp", ".c"} or rel in KNOWN_DESCRIPTIONS:
                entries.append(p)
    core_reg = src_root / "cores" / "core_registry.h"
    if core_reg.is_file():
        entries.append(core_reg)
    return [p for p in entries if p.is_file()]


def _infer_description(path: Path) -> str:
    rel = path.relative_to(REPO_ROOT).as_posix()
    if rel in KNOWN_DESCRIPTIONS:
        return KNOWN_DESCRIPTIONS[rel]
    for raw in path.read_text(encoding="utf-8", errors="ignore").splitlines()[:8]:
        s = raw.strip().lstrip("/").lstrip("*").strip()
        if s and not s.startswith(("SPDX-", "pragma", "include", "if", "---", "===")):
            return s[:78]
    return f"First-party module `{path.name}`"


def build_codemap() -> str:
    lines = [
        "<!-- GENERATED BY scripts/sync_ai_codemap.py - DO NOT EDIT BY HAND -->",
        "# Repository Code Map (`docs/ai/CODEMAP.md`)",
        "",
        "Compact structural map for AI agents and contributors. Regenerate via `python scripts/sync_ai_codemap.py --write`.",
        "",
        "## 1. Top-Level Directory Summary",
        "",
        "| Directory / File | Role |",
        "| :--- | :--- |",
        "| [`tasks.py`](../../tasks.py) | Cross-platform CLI task runner (`setup`, `doctor`, `check`, `test`, `build`, `format`, `size`, `bench`, `sim`) |",
        "| `firmware/BmoGameboy/` | Primary ESP32-S3 handheld firmware (`src/app`, `src/hal`, `src/storage`, `src/ui`, `src/cores`, `src/third_party`) |",
        "| `firmware/bringup/` | Minimal standalone hardware bring-up sketch (`PSRAM`, `40 MHz` ST7789 color bars, `SD`, `8` buttons) |",
        "| `hardware/` | [`hardware/layout.json`](../../hardware/layout.json) (`8.80x12.40x3.20 cm` layout), Wokwi `diagram.json`, and manifold CAD generator |",
        "| `simulator/` | Browser UI & Three.js 3D enclosure viewer + WebAssembly emulator playtest harness |",
        "| `benchmarks/` | Host C++, Node/WASM simulator, and Python pipeline benchmarks + regression budgets |",
        "| `tests/` | Python (`unittest`) and native C++17 (`CTest` in `tests/host/`) verification suites |",
        "| `tools/guardian/` | AST static analyzer, SPI bus wire-time calculator (`bus-calc`), and Xtensa ELF profiler (`profile-elf`) |",
        "| `scripts/` | Repo validator, markdown link checker, color token verifier, SD deployer, and layout/codemap generators |",
        "| `docs/` | Architecture, ADRs (`docs/decisions/`), owner guides (`docs/guides/`), wiring (`docs/wiring/`), and audits (`docs/audits/`) |",
        "",
        "## 2. Key First-Party Firmware Modules",
        "",
        "| File | Lines | Responsibility |",
        "| :--- | ---: | :--- |",
    ]

    for p in _collect_first_party_firmware():
        rel = p.relative_to(REPO_ROOT).as_posix()
        lines.append(f"| [`{rel}`](../../{rel}) | `{count_lines(p)}` | {_infer_description(p)} |")

    lines.extend(
        [
            "",
            "## 3. Emulator Wrapper Files (`firmware/BmoGameboy/src/cores/`)",
            "",
            "| Wrapper File | Lines | Upstream Engine (`src/third_party/`) | Build Gate |",
            "| :--- | ---: | :--- | :--- |",
        ]
    )

    core_dir = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "cores"
    for p in sorted(core_dir.glob("*.cpp")):
        rel = p.relative_to(REPO_ROOT).as_posix()
        engine, gate = CORE_META.get(p.name, ("Shared core helper", "Always enabled"))
        lines.append(f"| [`{p.name}`](../../{rel}) | `{count_lines(p)}` | {engine} | {gate} |")

    lines.extend(
        [
            "",
            "## 4. Verification & Generation Scripts (`scripts/`)",
            "",
            "| Script | Lines | Purpose |",
            "| :--- | ---: | :--- |",
        ]
    )
    for sp in sorted((REPO_ROOT / "scripts").glob("*.py")):
        rel = sp.relative_to(REPO_ROOT).as_posix()
        doc_line = ""
        for raw in sp.read_text(encoding="utf-8", errors="ignore").splitlines():
            s = raw.strip().strip('"').strip("'")
            if s and not s.startswith("#"):
                doc_line = s
                break
        lines.append(f"| [`{rel}`](../../{rel}) | `{count_lines(sp)}` | {doc_line[:88]} |")

    lines.append("")
    return "\n".join(lines)


def _current_branch_name() -> str:
    try:
        proc = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            check=True,
        )
        return proc.stdout.strip() or "unknown"
    except Exception:
        return "unknown"


def _compute_state_sync_line() -> str:
    fw_files: list[tuple[Path, int]] = []
    for folder in ("app", "hal", "storage", "ui", "cores", "tests"):
        d = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / folder
        if d.is_dir():
            fw_files.extend(
                (p, count_lines(p))
                for p in d.iterdir()
                if p.is_file() and p.suffix.lower() in {".c", ".cpp", ".h", ".hpp"}
            )
    max_file, max_lines = max(fw_files, key=lambda item: item[1]) if fw_files else (Path("none"), 0)
    total_fw_lines = sum(n for _, n in fw_files)
    script_count = len(list((REPO_ROOT / "scripts").glob("*.py")))
    ctest_count = len(list((REPO_ROOT / "tests" / "host").glob("*_test.cpp")))
    return (
        f"- **Synced Metrics (`sync_ai_codemap.py`):** branch `{_current_branch_name()}`, "
        f"`{len(fw_files)}` first-party firmware C/C++ files (`{total_fw_lines:,}` lines, max `{max_file.name}`=`{max_lines}`), "
        f"`{script_count}` scripts, `{ctest_count}` C++ host test suites."
    )


def build_state_text() -> str:
    if not STATE_PATH.is_file():
        raise FileNotFoundError(f"STATE.md not found: {STATE_PATH}")
    original = STATE_PATH.read_text(encoding="utf-8")
    sync_line = _compute_state_sync_line()
    block = f"<!-- SYNC:BEGIN -->\n{sync_line}\n<!-- SYNC:END -->"
    if "<!-- SYNC:BEGIN -->" in original and "<!-- SYNC:END -->" in original:
        updated = re.sub(
            r"<!-- SYNC:BEGIN -->.*?<!-- SYNC:END -->",
            block,
            original,
            flags=re.DOTALL,
        )
    else:
        updated = original.rstrip() + "\n\n" + block + "\n"
    return updated


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--write", action="store_true", help="Write updated CODEMAP.md and STATE.md")
    mode.add_argument("--check", action="store_true", help="Fail if CODEMAP.md or STATE.md is out of date")
    args = parser.parse_args(argv)
    do_check = args.check and not args.write

    codemap_content = build_codemap()
    codemap_lines = len(codemap_content.splitlines())
    if codemap_lines > 200:
        raise RuntimeError(f"CODEMAP.md exceeds 200-line budget ({codemap_lines} lines)")

    state_content = build_state_text()
    state_lines = len(state_content.splitlines())
    if state_lines > 60:
        raise RuntimeError(f"STATE.md exceeds 60-line budget ({state_lines} lines)")

    if do_check:
        mismatches: list[str] = []
        cur_codemap = CODEMAP_PATH.read_text(encoding="utf-8") if CODEMAP_PATH.is_file() else ""
        if cur_codemap != codemap_content:
            mismatches.append(str(CODEMAP_PATH.relative_to(REPO_ROOT)))
        cur_state = STATE_PATH.read_text(encoding="utf-8") if STATE_PATH.is_file() else ""
        if cur_state != state_content:
            mismatches.append(str(STATE_PATH.relative_to(REPO_ROOT)))
        if mismatches:
            print(
                "AI context docs out of sync: "
                + ", ".join(mismatches)
                + ". Run `python scripts/sync_ai_codemap.py --write`.",
                file=sys.stderr,
            )
            return 1
        print(f"AI context sync clean (CODEMAP={codemap_lines} lines, STATE={state_lines} lines).")
        return 0

    CODEMAP_PATH.parent.mkdir(parents=True, exist_ok=True)
    CODEMAP_PATH.write_text(codemap_content, encoding="utf-8", newline="\n")
    STATE_PATH.write_text(state_content, encoding="utf-8", newline="\n")
    print(f"Synced CODEMAP.md ({codemap_lines} lines) and STATE.md ({state_lines} lines).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
