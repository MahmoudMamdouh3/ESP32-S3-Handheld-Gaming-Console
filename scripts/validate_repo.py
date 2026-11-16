# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Repository validator covering compile, syntax, safety guardrails, layout, and local ROM archives.

Phases:
  0  Target Firmware Compile (arduino-cli — skipped with explicit notice unless strict)
  1  Python Syntax Check
  2  Firmware Safety Guardrails (hard-stop defines, teardowns, Serial.print ban, partitions, debug macros)
  3  Static Engine Size Gate
  4  Structural Registration (wrappers, sd_card.cpp extensions, display_emu.h stream methods)
  5  Local ROM Archive Health (skipped cleanly when gitignored local ROM directories are absent)
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import time
from collections.abc import Callable, Sequence
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_DIR = Path(__file__).resolve().parent
TESTS_DIR = REPO_ROOT / "tests"
FIRMWARE_DIR = REPO_ROOT / "firmware" / "BmoGameboy"

if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))


def _load_sketch_profile() -> tuple[str, str]:
    sketch_yaml = FIRMWARE_DIR / "sketch.yaml"
    default_profile = "esp32s3"
    fqbn = ""
    if sketch_yaml.is_file():
        for raw_line in sketch_yaml.read_text(encoding="utf-8").splitlines():
            s = raw_line.strip()
            if s.startswith("default_profile:"):
                default_profile = s.split(":", 1)[1].strip()
            elif s.startswith("fqbn:"):
                fqbn = s.split(":", 1)[1].strip()
    return default_profile, fqbn


def _discover_registry_contracts() -> tuple[list[str], list[str]]:
    reg_path = FIRMWARE_DIR / "src" / "cores" / "core_registry.h"
    emulators: list[str] = []
    teardowns: list[str] = []
    if reg_path.is_file():
        text = reg_path.read_text(encoding="utf-8")
        emulators = sorted(set(re.findall(r'#include\s+"(emu_[a-z0-9_]+)\.h"', text)))
        teardowns = sorted(set(re.findall(r"\b([A-Za-z0-9_]+Emu::destroy)\b", text)))
    return emulators, teardowns


def _discover_catalog_extensions() -> list[str]:
    cat_path = FIRMWARE_DIR / "src" / "ui" / "console_catalog.h"
    if not cat_path.is_file():
        return []
    text = cat_path.read_text(encoding="utf-8")
    m = re.search(r"kExtensionRoutes\[\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        return []
    return list(dict.fromkeys(re.findall(r'"(\.[a-z0-9]+)"', m.group(1))))


def _discover_display_stream_methods() -> list[str]:
    display_h = FIRMWARE_DIR / "src" / "ui" / "display_emu.h"
    if not display_h.is_file():
        return []
    text = display_h.read_text(encoding="utf-8")
    return list(dict.fromkeys(re.findall(r"\bvoid\s+(stream[A-Za-z0-9_]+)\s*\(", text)))


ARDUINO_PROFILE, ARDUINO_FQBN = _load_sketch_profile()
TARGET_COMPILE_STATUS = "NOT_RUN"

DEBUG_LOG_MACROS = {"LOG_LEVEL_DEBUG", "LOG_DEBUG", "LOG_DEBUG_STR"}
DEBUG_ONLY_MACRO_NAME = re.compile(
    r"(?:^|_)(?:DEBUG|UNIT_TESTS?|TEST_MODE|TEST_ONLY)(?:_|$)", re.IGNORECASE
)

EXPECTED_EMULATORS, EXPECTED_TEARDOWNS = _discover_registry_contracts()
EXPECTED_EXTENSIONS = _discover_catalog_extensions()
REQUIRED_STREAM_METHODS = _discover_display_stream_methods()


def _find_arduino_cli() -> str | None:
    for candidate in ("arduino-cli", "arduino-cli.exe"):
        if shutil.which(candidate):
            return candidate
    for candidate_path in (REPO_ROOT / "arduino-cli.exe", REPO_ROOT / "arduino-cli"):
        if candidate_path.exists():
            return str(candidate_path)
    return None


def _parse_partitions_csv(csv_path: Path) -> dict[str, dict[str, int]]:
    result: dict[str, dict[str, int]] = {}
    for line in csv_path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        cols = [c.strip() for c in line.split(",")]
        if len(cols) < 5:
            continue
        name, raw_size = cols[0], cols[4]
        try:
            offset = int(cols[3], 16) if cols[3].startswith("0x") else int(cols[3])
        except ValueError:
            offset = 0
        try:
            if raw_size.upper().endswith("M"):
                size = int(raw_size[:-1]) * 1024 * 1024
            elif raw_size.lower().startswith("0x"):
                size = int(raw_size, 16)
            else:
                size = int(raw_size)
        except ValueError:
            continue
        result[name] = {"offset": offset, "size": size}
    return result


def _validate_compile_metrics(combined: str, build_dir: Path) -> list[str]:
    issues: list[str] = []
    text_m = re.search(r"Sketch uses (\d+) bytes.*?Maximum is (\d+) bytes", combined)
    sram_m = re.search(r"Global variables use (\d+) bytes.*?Maximum is (\d+) bytes", combined)
    if text_m:
        used, mx = int(text_m.group(1)), int(text_m.group(2))
        pct = used / mx * 100
        headroom_pct = (mx - used) / mx * 100
        print(f"  Flash: {used:,} / {mx:,} bytes ({pct:.1f}%) — headroom {headroom_pct:.1f}%")
        if used > mx:
            issues.append(f"FLASH OVERFLOW: {used:,} bytes exceeds {mx:,} bytes max.")
        elif headroom_pct < 20:
            issues.append(f"Flash headroom low: {headroom_pct:.1f}% free (minimum 20%).")
    else:
        issues.append("arduino-cli output did not contain the sketch flash-usage line")

    if sram_m:
        sused, smx = int(sram_m.group(1)), int(sram_m.group(2))
        spct = sused / smx * 100
        print(f"  SRAM:  {sused:,} / {smx:,} bytes ({spct:.1f}%)")
        if spct > 90:
            issues.append(f"SRAM critically high: {spct:.1f}%")

    repo_partitions = FIRMWARE_DIR / "partitions.csv"
    built_partitions = build_dir / "partitions.csv"
    if not built_partitions.is_file():
        issues.append("target build did not produce partitions.csv")
    elif built_partitions.read_bytes() != repo_partitions.read_bytes():
        issues.append("target build partition table differs from firmware/BmoGameboy/partitions.csv")

    app0_size = _parse_partitions_csv(repo_partitions).get("app0", {}).get("size")
    app_images = sorted(build_dir.glob("*.ino.bin"))
    if app0_size is None:
        issues.append("repository partition table has no app0 size")
    elif len(app_images) != 1:
        issues.append(f"target build produced {len(app_images)} candidate app images; expected one")
    else:
        image_size = app_images[0].stat().st_size
        print(f"  App image: {image_size:,} / {app0_size:,} bytes (app0); {app0_size - image_size:,} bytes free")
        if image_size > app0_size:
            issues.append(f"app image exceeds app0 by {image_size - app0_size:,} bytes")
    return issues


def check_arduino_compile(require_target_compile: bool = False) -> list[str]:
    global TARGET_COMPILE_STATUS
    issues: list[str] = []
    cli = _find_arduino_cli()
    if not cli:
        TARGET_COMPILE_STATUS = "SKIPPED"
        print("  arduino-cli is unavailable; target compile was skipped.")
        if require_target_compile:
            issues.append("arduino-cli is unavailable in strict target-compile mode")
        return issues

    build_dir = REPO_ROOT / "build" / "target-compile"
    build_dir.mkdir(parents=True, exist_ok=True)
    print(f"  Compiling profile: {ARDUINO_PROFILE} ({ARDUINO_FQBN})")
    try:
        result = subprocess.run(
            [cli, "compile", "--clean", "--profile", ARDUINO_PROFILE,
             "--build-path", str(build_dir), str(FIRMWARE_DIR)],
            capture_output=True, text=True, timeout=1800,
        )
    except subprocess.TimeoutExpired:
        TARGET_COMPILE_STATUS = "FAILED"
        issues.append("arduino-cli compile timed out after 1800 s")
        return issues
    except Exception as exc:
        TARGET_COMPILE_STATUS = "FAILED"
        issues.append(f"arduino-cli invocation error: {exc}")
        return issues

    combined = result.stdout + result.stderr
    if combined.strip():
        print(combined.rstrip())
    if result.returncode != 0:
        TARGET_COMPILE_STATUS = "FAILED"
        issues.append("arduino-cli compile returned non-zero exit (see output above)")
        return issues

    issues.extend(_validate_compile_metrics(combined, build_dir))
    TARGET_COMPILE_STATUS = "FAILED" if issues else "VERIFIED"
    return issues


def _debug_only_definitions(text: str) -> list[str]:
    found: list[str] = []
    definition = re.compile(
        r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)|(?:^|\s)-D([A-Za-z_][A-Za-z0-9_]*)",
        re.MULTILINE,
    )
    for match in definition.finditer(text):
        name = match.group(1) or match.group(2)
        if name not in DEBUG_LOG_MACROS and DEBUG_ONLY_MACRO_NAME.search(name):
            found.append(name)
    return found


def _check_python_syntax_file(script: Path) -> str | None:
    try:
        compile(script.read_text(encoding="utf-8"), str(script), "exec")
        return None
    except SyntaxError as exc:
        return f"{script.name}: syntax error: {exc}"


def check_release_debug_macro_guardrails() -> list[str]:
    issues: list[str] = []
    ignored_dirs = {".git", ".tools", "build", "node_modules", "__pycache__", "games", "roms", "sdcard", "Book"}
    third_party_dir = FIRMWARE_DIR / "src" / "third_party"
    for current_root, directory_names, file_names in os.walk(REPO_ROOT):
        current_path = Path(current_root)
        if current_path == third_party_dir or third_party_dir in current_path.parents:
            directory_names[:] = []
            continue
        directory_names[:] = [name for name in directory_names if name not in ignored_dirs]
        for file_name in file_names:
            header = current_path / file_name
            if header.suffix.lower() not in {".h", ".hh", ".hpp", ".hxx"}:
                continue
            relative = header.relative_to(REPO_ROOT)
            try:
                text = header.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            issues.extend(
                f"{relative.as_posix()}: release header defines debug/test-only macro {macro}"
                for macro in _debug_only_definitions(text)
            )

    profile = FIRMWARE_DIR / "sketch.yaml"
    if profile.is_file():
        issues.extend(
            f"{profile.relative_to(REPO_ROOT).as_posix()}: release profile defines debug/test-only macro {macro}"
            for macro in _debug_only_definitions(profile.read_text(encoding="utf-8", errors="ignore"))
        )
    return issues


def check_python_files() -> list[str]:
    errors: list[str] = []
    guardian_dir = REPO_ROOT / "tools" / "guardian"
    for folder in [SCRIPTS_DIR, TESTS_DIR, guardian_dir]:
        if not folder.exists():
            continue
        errors.extend(
            err
            for script in sorted(folder.rglob("*.py"))
            if (err := _check_python_syntax_file(script))
        )
    return errors


def _check_core_and_registry_guardrails() -> list[str]:
    issues: list[str] = []
    app_dir = FIRMWARE_DIR / "src" / "app"
    if app_dir.is_dir():
        app_text = "\n".join(p.read_text(encoding="utf-8") for p in sorted(app_dir.glob("*.cpp")))
        if "CoreRegistry::findByIndex" not in app_text or "core->destroy()" not in app_text:
            issues.append("src/app/: missing CoreRegistry teardown dispatch in destroyActiveEmulator()")
    else:
        issues.append("src/app/ not found")

    registry_path = FIRMWARE_DIR / "src" / "cores" / "core_registry.h"
    if registry_path.exists():
        registry_text = registry_path.read_text(encoding="utf-8")
        emulators, teardowns = _discover_registry_contracts()
        if len(emulators) < 14 or len(teardowns) < 14:
            issues.append("core_registry.h: expected >= 14 registered emulators and destroy() handlers")
        issues.extend(f"core_registry.h: missing teardown registration: {td}" for td in teardowns if td not in registry_text)
    else:
        issues.append("src/cores/core_registry.h not found")

    emu_dir = FIRMWARE_DIR / "src" / "cores"
    if emu_dir.exists():
        issues.extend(
            f"{cpp.name}: naked Serial.print in emulator wrapper — use LOG_ macros"
            for cpp in sorted(emu_dir.glob("*.cpp"))
            if re.search(r"\bSerial\.print", cpp.read_text(encoding="utf-8"))
        )

    walnut_cpp = FIRMWARE_DIR / "src" / "cores" / "emu_walnut.cpp"
    if walnut_cpp.exists():
        wt = walnut_cpp.read_text(encoding="utf-8")
        if "*(uint16_t*)&" in wt or "*(uint32_t*)&" in wt:
            issues.append("emu_walnut.cpp: unaligned raw pointer dereference (Xtensa safety)")
    return issues


def _check_partitions_guardrail() -> list[str]:
    partitions_csv = FIRMWARE_DIR / "partitions.csv"
    if not partitions_csv.exists():
        return ["partitions.csv not found"]
    issues: list[str] = []
    parts = _parse_partitions_csv(partitions_csv)
    if "app0" not in parts:
        issues.append("partitions.csv: app0 partition missing")
    elif parts["app0"]["size"] < 0x800000:
        sz = parts["app0"]["size"]
        issues.append(f"partitions.csv: app0 is {sz:,} bytes — must be >= 8 MB")
    sorted_p = sorted(parts.values(), key=lambda p: p["offset"])
    for i in range(len(sorted_p) - 1):
        a, b = sorted_p[i], sorted_p[i + 1]
        if a["offset"] + a["size"] > b["offset"]:
            issues.append(
                f"partitions.csv: overlap between 0x{a['offset']:06X}+{a['size']:06X} and 0x{b['offset']:06X}"
            )
    return issues


def check_firmware_safety_guardrails() -> list[str]:
    issues: list[str] = []
    hal_dir = FIRMWARE_DIR / "src" / "hal"
    config_path = hal_dir / "config.h"
    flags_path = hal_dir / "feature_flags.h"
    if not config_path.exists():
        return ["src/hal/config.h not found"]
    config_text = config_path.read_text(encoding="utf-8")
    if flags_path.exists():
        config_text = flags_path.read_text(encoding="utf-8") + "\n" + config_text
    issues.extend(
        f"hal/config.h: {flag} must be exactly 0 (hardware hard stop)"
        for flag in ("FEATURE_BATTERY_MONITOR", "FEATURE_AUDIO")
        if not re.search(rf"#define\s+{flag}\s+0\b", config_text)
    )
    issues.extend(_check_core_and_registry_guardrails())
    issues.extend(_check_partitions_guardrail())
    issues.extend(check_release_debug_macro_guardrails())
    return issues


def check_flash_budget_static() -> list[str]:
    issues: list[str] = []
    legacy_engines = FIRMWARE_DIR / "src" / "engines"
    if legacy_engines.exists():
        issues.append("Legacy stub directory src/engines must not exist (use src/third_party)")
    third_party = FIRMWARE_DIR / "src" / "third_party"
    if not third_party.exists():
        return ["No engine source directory (src/third_party) found"]
    total = 0
    for f in third_party.rglob("*"):
        if not f.is_file():
            continue
        sz = f.stat().st_size
        total += sz
        if sz >= 1024 * 1024:
            rel = f.relative_to(FIRMWARE_DIR).as_posix()
            issues.append(f"{rel}: single file {sz:,} bytes >= 1 MiB blob threshold")
    print(f"  Third-party source tree: {total / 1024:.1f} KB")
    if total > 16 * 1024 * 1024:
        issues.append(f"Third-party source tree is {total / 1024:.0f} KB — check for large uncompressed blobs")
    return issues


def check_structural_soundness() -> list[str]:
    issues: list[str] = []
    emu_dir = FIRMWARE_DIR / "src" / "cores"
    emulators, _ = _discover_registry_contracts()
    storage_dir = FIRMWARE_DIR / "src" / "storage"
    issues.extend(
        f"Missing SD storage module: src/storage/{sd_module}"
        for sd_module in ("sd_mount.cpp", "sd_metadata.cpp")
        if not (storage_dir / sd_module).exists()
    )
    if not ((storage_dir / "sd_catalog.cpp").exists() or (storage_dir / "rom_catalog_index.cpp").exists()):
        issues.append("Missing SD catalog module (sd_catalog.cpp or rom_catalog_index.cpp)")

    for stem in emulators:
        issues.extend(
            f"Missing emulator wrapper: src/cores/{stem}{ext}"
            for ext in (".h", ".cpp")
            if not (emu_dir / (stem + ext)).exists()
        )

    catalog_h = FIRMWARE_DIR / "src" / "ui" / "console_catalog.h"
    if catalog_h.exists():
        extensions = _discover_catalog_extensions()
        if len(extensions) < 21:
            issues.append(f"console_catalog.h: expected >= 21 ROM extensions, found {len(extensions)}")
    else:
        issues.append("src/ui/console_catalog.h not found")

    display_h = FIRMWARE_DIR / "src" / "ui" / "display_emu.h"
    if display_h.exists():
        methods = _discover_display_stream_methods()
        if len(methods) < 13:
            issues.append(f"display_emu.h: expected >= 13 stream methods, found {len(methods)}")
    else:
        issues.append("display_emu.h not found")
    return issues


def check_game_data() -> list[str]:
    issues: list[str] = []
    try:
        from scripts.repo_tools import find_game_roots, get_data_dir, validate_rom_header
    except ImportError:
        try:
            from repo_tools import find_game_roots, get_data_dir, validate_rom_header  # type: ignore[no-redef]
        except ImportError:
            return ["repo_tools.py not importable — cannot validate ROM archives"]

    try:
        data_dir = get_data_dir(REPO_ROOT)
    except FileNotFoundError as exc:
        return [str(exc)]
    roots = find_game_roots(data_dir)
    if not roots:
        return issues

    import zipfile
    for label, game_dir in roots.items():
        zip_files = sorted(game_dir.glob("*.zip"))
        if not zip_files:
            issues.append(f"{label}: no zip files found.")
            continue
        for zip_file in zip_files:
            try:
                with zipfile.ZipFile(zip_file) as archive:
                    rom_name = next((n for n in archive.namelist() if n.lower().endswith((".gb", ".gbc"))), None)
                    if rom_name is None:
                        issues.append(f"{zip_file.name}: no ROM file found inside archive.")
                        continue
                    rom_bytes = archive.read(rom_name)
                    ok, actual, expected = validate_rom_header(rom_bytes)
                    if not ok and len(rom_bytes) > 0x150:
                        issues.append(
                            f"{zip_file.name}: rom header checksum mismatch (actual 0x{actual:02X}, expected 0x{expected:02X})"
                        )
            except zipfile.BadZipFile:
                issues.append(f"{zip_file.name}: invalid zip archive.")
    return issues


PHASES: list[tuple[str, Callable[[], list[str]]]] = [
    ("Phase 0: Target Firmware Compile", check_arduino_compile),
    ("Phase 1: Python Syntax", check_python_files),
    ("Phase 2: Firmware Safety Guardrails", check_firmware_safety_guardrails),
    ("Phase 3: Static Engine Source Gate", check_flash_budget_static),
    ("Phase 4: Structural Registration", check_structural_soundness),
    ("Phase 5: Local ROM Archive Health", check_game_data),
]


def main(argv: Sequence[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description="Validate BMO Handheld source, build, and safety contracts.")
    parser.add_argument(
        "--require-target-compile",
        action="store_true",
        help="fail if arduino-cli target compile cannot run (also honors BMO_REQUIRE_TARGET_COMPILE=1)",
    )
    args = parser.parse_args(argv)
    env_strict = os.environ.get("BMO_REQUIRE_TARGET_COMPILE", "").strip().lower() in {"1", "true", "yes", "on"}
    require_target_compile = args.require_target_compile or env_strict

    start = time.perf_counter()
    print("=" * 62)
    print("BMO Handheld — Repository Validator")
    print("=" * 62)

    all_issues: list[str] = []
    for phase_name, phase_fn in PHASES:
        if phase_fn is check_arduino_compile:
            def run_phase() -> list[str]:
                return check_arduino_compile(require_target_compile)
            active_fn = run_phase
        else:
            active_fn = phase_fn
        print(f"\n[{phase_name}]")
        errors = active_fn()
        if errors:
            for e in errors:
                print(f"  FAIL: {e}")
            all_issues.extend(errors)
        else:
            print("  PASS")

    duration_ms = (time.perf_counter() - start) * 1000
    print("\n" + "=" * 62)
    if TARGET_COMPILE_STATUS == "SKIPPED":
        print("TARGET COMPILE: SKIPPED - NOT VERIFIED")
    elif TARGET_COMPILE_STATUS == "VERIFIED":
        print("TARGET COMPILE: VERIFIED")
    elif TARGET_COMPILE_STATUS == "FAILED":
        print("TARGET COMPILE: FAILED - NOT VERIFIED")
    if all_issues:
        print(f"VALIDATION FAILED in {duration_ms:.0f} ms — {len(all_issues)} issue(s):")
        for issue in all_issues:
            print(f"  - {issue}")
        raise SystemExit(1)
    print(f"VALIDATION PASSED in {duration_ms:.0f} ms — all phases clean.")


if __name__ == "__main__":
    main()
