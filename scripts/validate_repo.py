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
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_DIR = Path(__file__).resolve().parent
TESTS_DIR = REPO_ROOT / "tests"
FIRMWARE_DIR = REPO_ROOT / "firmware" / "BmoGameboy"

if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

ARDUINO_FQBN = (
    "esp32:esp32:esp32s3:"
    "CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,"
    "PartitionScheme=custom,PSRAM=opi,USBMode=default"
)
ARDUINO_PROFILE = "esp32s3"
TARGET_COMPILE_STATUS = "NOT_RUN"

DEBUG_LOG_MACROS = {"LOG_LEVEL_DEBUG", "LOG_DEBUG", "LOG_DEBUG_STR"}
DEBUG_ONLY_MACRO_NAME = re.compile(
    r"(?:^|_)(?:DEBUG|UNIT_TESTS?|TEST_MODE|TEST_ONLY)(?:_|$)", re.IGNORECASE
)

EXPECTED_EMULATORS = [
    "emu_walnut", "emu_peanut", "emu_nes", "emu_doom",
    "emu_sms", "emu_pce", "emu_atari", "emu_pico",
    "emu_genesis", "emu_snes", "emu_wswan", "emu_ngp",
    "emu_lynx", "emu_colem",
]

EXPECTED_TEARDOWNS = [
    "WalnutEmu::destroy()", "PeanutEmu::destroy()",
    "NesEmu::destroy()", "DoomEmu::destroy()",
    "SmsEmu::destroy()", "PceEmu::destroy()",
    "AtariEmu::destroy()", "PicoEmu::destroy()",
    "GenesisEmu::destroy()", "SNESEmu::destroy()",
    "WSwanEmu::destroy()", "NGPEmu::destroy()",
    "LynxEmu::destroy()", "ColemEmu::destroy()",
]

EXPECTED_EXTENSIONS = [
    ".gb", ".gbc", ".nes", ".wad",
    ".sms", ".gg", ".pce", ".a26", ".p8",
    ".gen", ".md", ".smd", ".sfc", ".smc",
    ".ws", ".wsc", ".ngp", ".ngc",
    ".lnx", ".col", ".sg",
]

REQUIRED_STREAM_METHODS = [
    "streamPixelRow", "streamNESFrame", "streamDoomFrame",
    "streamSMSFrame", "streamPCEFrame", "streamAtariFrame",
    "streamPicoFrame", "streamGenesisFrame", "streamSNESFrame",
    "streamWSwanFrame", "streamNGPFrame", "streamLynxFrame",
    "streamColemFrame",
]


def _find_arduino_cli():
    for candidate in ["arduino-cli", "arduino-cli.exe"]:
        if shutil.which(candidate):
            return candidate
    for candidate in [REPO_ROOT / "arduino-cli.exe", REPO_ROOT / "arduino-cli"]:
        if candidate.exists():
            return str(candidate)
    return None


def _parse_partitions_csv(csv_path):
    result = {}
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


def check_arduino_compile(require_target_compile=False):
    global TARGET_COMPILE_STATUS
    issues = []
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

    TARGET_COMPILE_STATUS = "FAILED" if issues else "VERIFIED"
    return issues


def _debug_only_definitions(text):
    found = []
    definition = re.compile(
        r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)|(?:^|\s)-D([A-Za-z_][A-Za-z0-9_]*)",
        re.MULTILINE,
    )
    for match in definition.finditer(text):
        name = match.group(1) or match.group(2)
        if name not in DEBUG_LOG_MACROS and DEBUG_ONLY_MACRO_NAME.search(name):
            found.append(name)
    return found


def check_release_debug_macro_guardrails():
    issues = []
    ignored_dirs = {".git", ".tools", "build", "node_modules", "__pycache__", "games", "roms", "sdcard", "Book"}
    for current_root, directory_names, file_names in os.walk(REPO_ROOT):
        directory_names[:] = [name for name in directory_names if name not in ignored_dirs]
        for file_name in file_names:
            header = Path(current_root) / file_name
            if header.suffix.lower() not in {".h", ".hh", ".hpp", ".hxx"}:
                continue
            relative = header.relative_to(REPO_ROOT)
            try:
                text = header.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            for macro in _debug_only_definitions(text):
                issues.append(f"{relative.as_posix()}: release header defines debug/test-only macro {macro}")

    profile = FIRMWARE_DIR / "sketch.yaml"
    if profile.is_file():
        for macro in _debug_only_definitions(profile.read_text(encoding="utf-8", errors="ignore")):
            issues.append(f"{profile.relative_to(REPO_ROOT).as_posix()}: release profile defines debug/test-only macro {macro}")
    return issues


def check_python_files():
    errors = []
    guardian_dir = REPO_ROOT / "tools" / "guardian"
    for folder in [SCRIPTS_DIR, TESTS_DIR, guardian_dir]:
        if not folder.exists():
            continue
        for script in sorted(folder.rglob("*.py")):
            try:
                compile(script.read_text(encoding="utf-8"), str(script), "exec")
            except SyntaxError as exc:
                errors.append(f"{script.name}: syntax error: {exc}")
    return errors


def check_firmware_safety_guardrails():
    issues = []
    config_path = FIRMWARE_DIR / "src" / "core" / "config.h"
    if not config_path.exists():
        return ["config.h not found"]
    config_text = config_path.read_text(encoding="utf-8")
    for flag in ("FEATURE_BATTERY_MONITOR", "FEATURE_AUDIO"):
        if not re.search(rf"#define\s+{flag}\s+0\b", config_text):
            issues.append(f"config.h: {flag} must be exactly 0 (hardware hard stop)")

    ino_path = FIRMWARE_DIR / "BmoGameboy.ino"
    if ino_path.exists():
        ino_text = ino_path.read_text(encoding="utf-8")
        for td in EXPECTED_TEARDOWNS:
            if td not in ino_text:
                issues.append(f"BmoGameboy.ino: missing teardown call: {td}")
    else:
        issues.append("BmoGameboy.ino not found")

    emu_dir = FIRMWARE_DIR / "src" / "emulators"
    if emu_dir.exists():
        for cpp in sorted(emu_dir.glob("*.cpp")):
            if re.search(r"\bSerial\.print", cpp.read_text(encoding="utf-8")):
                issues.append(f"{cpp.name}: naked Serial.print in emulator wrapper — use LOG_ macros")

    walnut_cpp = FIRMWARE_DIR / "src" / "emulators" / "emu_walnut.cpp"
    if walnut_cpp.exists():
        wt = walnut_cpp.read_text(encoding="utf-8")
        if "*(uint16_t*)&" in wt or "*(uint32_t*)&" in wt:
            issues.append("emu_walnut.cpp: unaligned raw pointer dereference (Xtensa safety)")

    partitions_csv = FIRMWARE_DIR / "partitions.csv"
    if partitions_csv.exists():
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
    else:
        issues.append("partitions.csv not found")

    issues.extend(check_release_debug_macro_guardrails())
    return issues


def check_flash_budget_static():
    issues = []
    vendor_dir = FIRMWARE_DIR / "src" / "vendor"
    if not vendor_dir.exists():
        return issues
    total = sum(f.stat().st_size for f in vendor_dir.rglob("*") if f.is_file())
    print(f"  Engine source tree: {total / 1024:.1f} KB")
    if total > 8 * 1024 * 1024:
        issues.append(f"Engine source tree is {total / 1024:.0f} KB — check for large uncompressed blobs")
    return issues


def check_structural_soundness():
    issues = []
    emu_dir = FIRMWARE_DIR / "src" / "emulators"
    for stem in EXPECTED_EMULATORS:
        for ext in (".h", ".cpp"):
            if not (emu_dir / (stem + ext)).exists():
                issues.append(f"Missing emulator wrapper: src/emulators/{stem}{ext}")

    sd_cpp = FIRMWARE_DIR / "src" / "core" / "sd_card.cpp"
    if sd_cpp.exists():
        sd_text = sd_cpp.read_text(encoding="utf-8")
        for ext in EXPECTED_EXTENSIONS:
            if ext not in sd_text:
                issues.append(f'sd_card.cpp: extension "{ext}" not registered')
    else:
        issues.append("sd_card.cpp not found")

    display_h = FIRMWARE_DIR / "src" / "core" / "display_emu.h"
    if display_h.exists():
        header = display_h.read_text(encoding="utf-8")
        for m in REQUIRED_STREAM_METHODS:
            if m not in header:
                issues.append(f"display_emu.h: missing stream method: {m}")
    else:
        issues.append("display_emu.h not found")
    return issues


def check_game_data():
    issues = []
    try:
        from repo_tools import find_game_roots, validate_rom_header
    except ImportError:
        return ["repo_tools.py not importable — cannot validate ROM archives"]

    roots = find_game_roots(REPO_ROOT)
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


PHASES = [
    ("Phase 0: Target Firmware Compile", check_arduino_compile),
    ("Phase 1: Python Syntax", check_python_files),
    ("Phase 2: Firmware Safety Guardrails", check_firmware_safety_guardrails),
    ("Phase 3: Static Engine Source Gate", check_flash_budget_static),
    ("Phase 4: Structural Registration", check_structural_soundness),
    ("Phase 5: Local ROM Archive Health", check_game_data),
]


def main(argv=None):
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

    all_issues = []
    for phase_name, phase_fn in PHASES:
        if phase_fn is check_arduino_compile:
            phase_fn = lambda: check_arduino_compile(require_target_compile)
        print(f"\n[{phase_name}]")
        errors = phase_fn()
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
