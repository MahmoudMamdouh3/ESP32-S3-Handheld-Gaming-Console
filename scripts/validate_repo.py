"""
validate_repo.py — BMO Gameboy AI Guardian CI Validator
Multi-phase repository validator.

VERIFIED_HOST status requires Phase 0 (arduino-cli compile) to pass.
Python text-pattern checks alone do NOT constitute compilation verification.

Phases:
  0  Binary Compilation Gate (arduino-cli — skipped with WARNING if CLI absent)
  1  Python Syntax
  2  Firmware Safety Guardrails (enhanced: all 14 teardowns, Serial.print ban,
     partition overlap, exact flag values)
  3  Flash Budget — static vendor source size gate
  4  Structural Soundness (every emulator registered, display methods present,
     AGENT_MANIFEST engine_status fields)
  5  ROM Archive Data Health
  6  Rules & Manifest Integrity
  7  Cross-Document & Rule Truth Parity
  8  UI Design System & Anti-AI-Slop Standards
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

REPO_ROOT    = Path(__file__).resolve().parents[1]
SCRIPTS_DIR  = Path(__file__).resolve().parent
TESTS_DIR    = REPO_ROOT / 'tests'
FIRMWARE_DIR = REPO_ROOT / 'firmware' / 'BmoGameboy'
RULES_DIR    = REPO_ROOT / '.agents' / 'rules'

if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

# Canonical build FQBN — CDCOnBoot=cdc, FlashMode=qio for Quad Flash + Octal PSRAM module
ARDUINO_FQBN = (
    'esp32:esp32:esp32s3:'
    'CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,'
    'PartitionScheme=custom,PSRAM=opi,USBMode=default'
)
ARDUINO_PROFILE = 'esp32s3'
TARGET_COMPILE_STATUS = 'NOT_RUN'

DEBUG_LOG_MACROS = {'LOG_LEVEL_DEBUG', 'LOG_DEBUG', 'LOG_DEBUG_STR'}
DEBUG_ONLY_MACRO_NAME = re.compile(
    r'(?:^|_)(?:DEBUG|UNIT_TESTS?|TEST_MODE|TEST_ONLY)(?:_|$)', re.IGNORECASE
)

# All expected emulator wrapper stems
EXPECTED_EMULATORS = [
    'emu_walnut', 'emu_peanut', 'emu_nes',     'emu_doom',
    'emu_sms',    'emu_pce',    'emu_atari',    'emu_pico',
    'emu_genesis','emu_snes',   'emu_wswan',    'emu_ngp',
    'emu_lynx',   'emu_colem',
]

# All destroy() calls that must be wired in BmoGameboy.ino
EXPECTED_TEARDOWNS = [
    'WalnutEmu::destroy()', 'PeanutEmu::destroy()',
    'NesEmu::destroy()',    'DoomEmu::destroy()',
    'SmsEmu::destroy()',    'PceEmu::destroy()',
    'AtariEmu::destroy()',  'PicoEmu::destroy()',
    'GenesisEmu::destroy()', 'SNESEmu::destroy()',
    'WSwanEmu::destroy()',  'NGPEmu::destroy()',
    'LynxEmu::destroy()',   'ColemEmu::destroy()',
]

# All ROM extensions that must be in sd_card.cpp
EXPECTED_EXTENSIONS = [
    '.gb', '.gbc', '.nes', '.wad',
    '.sms', '.gg', '.pce', '.a26', '.p8',
    '.gen', '.md', '.smd', '.sfc', '.smc',
    '.ws', '.wsc', '.ngp', '.ngc',
    '.lnx', '.col', '.sg',
]

# Display streaming methods that must be declared in display_emu.h
REQUIRED_STREAM_METHODS = [
    'streamPixelRow', 'streamNESFrame', 'streamDoomFrame',
    'streamSMSFrame', 'streamPCEFrame', 'streamAtariFrame',
    'streamPicoFrame', 'streamGenesisFrame', 'streamSNESFrame',
    'streamWSwanFrame', 'streamNGPFrame', 'streamLynxFrame',
    'streamColemFrame',
]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _find_arduino_cli():
    for candidate in ['arduino-cli', 'arduino-cli.exe']:
        if shutil.which(candidate):
            return candidate
    for candidate in [REPO_ROOT / 'arduino-cli.exe', REPO_ROOT / 'arduino-cli']:
        if candidate.exists():
            return str(candidate)
    return None


def _parse_partitions_csv(csv_path):
    """Return dict of {name: {offset_int, size_int}}."""
    result = {}
    for line in csv_path.read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        cols = [c.strip() for c in line.split(',')]
        if len(cols) < 5:
            continue
        name, raw_size = cols[0], cols[4]
        try:
            offset = int(cols[3], 16) if cols[3].startswith('0x') else int(cols[3])
        except ValueError:
            offset = 0
        try:
            if raw_size.upper().endswith('M'):
                size = int(raw_size[:-1]) * 1024 * 1024
            elif raw_size.startswith('0x') or raw_size.startswith('0X'):
                size = int(raw_size, 16)
            else:
                size = int(raw_size)
        except ValueError:
            continue
        result[name] = {'offset': offset, 'size': size}
    return result


# ---------------------------------------------------------------------------
# Phase 0 — Binary Compilation Gate
# ---------------------------------------------------------------------------

def check_arduino_compile(require_target_compile=False):
    """
    Run arduino-cli compile with the production FQBN.
    Missing arduino-cli is an explicit skip in local advisory mode and an
    error in strict mode. A successful run also verifies the repository
    partition table and checks the linked app image against app0.
    Returns list of error strings.
    """
    global TARGET_COMPILE_STATUS
    issues = []
    cli = _find_arduino_cli()
    if not cli:
        TARGET_COMPILE_STATUS = 'SKIPPED'
        print('  arduino-cli is unavailable; target compile was skipped.')
        if require_target_compile:
            issues.append('arduino-cli is unavailable in strict target-compile mode')
        return issues

    build_dir = REPO_ROOT / 'build' / 'target-compile'
    build_dir.mkdir(parents=True, exist_ok=True)
    print(f'  Compiling profile: {ARDUINO_PROFILE} ({ARDUINO_FQBN})')
    try:
        result = subprocess.run(
            [cli, 'compile', '--clean', '--profile', ARDUINO_PROFILE,
             '--build-path', str(build_dir), str(FIRMWARE_DIR)],
            capture_output=True, text=True, timeout=1800,
        )
    except subprocess.TimeoutExpired:
        TARGET_COMPILE_STATUS = 'FAILED'
        issues.append('arduino-cli compile timed out after 1800 s')
        return issues
    except Exception as exc:
        TARGET_COMPILE_STATUS = 'FAILED'
        issues.append(f'arduino-cli invocation error: {exc}')
        return issues

    combined = result.stdout + result.stderr
    if combined.strip():
        print(combined.rstrip())
    if result.returncode != 0:
        TARGET_COMPILE_STATUS = 'FAILED'
        issues.append('arduino-cli compile returned non-zero exit (see output above)')
        return issues

    # Parse flash/SRAM numbers
    text_m = re.search(r'Sketch uses (\d+) bytes.*?Maximum is (\d+) bytes', combined)
    sram_m = re.search(r'Global variables use (\d+) bytes.*?Maximum is (\d+) bytes', combined)

    if text_m:
        used, mx = int(text_m.group(1)), int(text_m.group(2))
        pct = used / mx * 100
        headroom_pct = (mx - used) / mx * 100
        print(f'  Flash: {used:,} / {mx:,} bytes ({pct:.1f}%) — headroom {headroom_pct:.1f}%')
        if used > mx:
            issues.append(
                f'FLASH OVERFLOW: {used:,} bytes = {pct:.1f}% of {mx:,} bytes max. '
                'Select PartitionScheme=custom (8 MB app0) in IDE or CLI FQBN.'
            )
        elif headroom_pct < 20:
            issues.append(
                f'Flash headroom critically low: {headroom_pct:.1f}% free — '
                'must maintain >= 20% headroom.'
            )
    else:
        issues.append('arduino-cli output did not contain the sketch flash-usage line')

    if sram_m:
        sused, smx = int(sram_m.group(1)), int(sram_m.group(2))
        spct = sused / smx * 100
        print(f'  SRAM:  {sused:,} / {smx:,} bytes ({spct:.1f}%)')
        if spct > 90:
            issues.append(
                f'SRAM critically high: {spct:.1f}% — risk of stack overflow.'
            )

    repo_partitions = FIRMWARE_DIR / 'partitions.csv'
    built_partitions = build_dir / 'partitions.csv'
    if not built_partitions.is_file():
        issues.append('target build did not produce partitions.csv')
    elif built_partitions.read_bytes() != repo_partitions.read_bytes():
        issues.append('target build partition table differs from firmware/BmoGameboy/partitions.csv')

    app0_size = _parse_partitions_csv(repo_partitions).get('app0', {}).get('size')
    app_images = sorted(build_dir.glob('*.ino.bin'))
    if app0_size is None:
        issues.append('repository partition table has no app0 size')
    elif len(app_images) != 1:
        issues.append(f'target build produced {len(app_images)} candidate app images; expected one')
    else:
        image_size = app_images[0].stat().st_size
        print(f'  App image: {image_size:,} / {app0_size:,} bytes (app0); '
              f'{app0_size - image_size:,} bytes free')
        if image_size > app0_size:
            issues.append(f'app image exceeds app0 by {image_size - app0_size:,} bytes')

    if issues:
        TARGET_COMPILE_STATUS = 'FAILED'
    else:
        TARGET_COMPILE_STATUS = 'VERIFIED'
    return issues


def _debug_only_definitions(text):
    """Return debug/test-only macro definitions found in a header or profile."""
    found = []
    definition = re.compile(
        r'^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)|(?:^|\s)-D([A-Za-z_][A-Za-z0-9_]*)',
        re.MULTILINE,
    )
    for match in definition.finditer(text):
        name = match.group(1) or match.group(2)
        if name not in DEBUG_LOG_MACROS and DEBUG_ONLY_MACRO_NAME.search(name):
            found.append(name)
    return found


def check_release_debug_macro_guardrails():
    issues = []
    ignored_dirs = {'.git', '.tools', 'build', 'node_modules', '__pycache__', 'games', 'roms', 'sdcard'}
    for current_root, directory_names, file_names in os.walk(REPO_ROOT):
        directory_names[:] = [name for name in directory_names if name not in ignored_dirs]
        for file_name in file_names:
            header = Path(current_root) / file_name
            if header.suffix.lower() not in {'.h', '.hh', '.hpp', '.hxx'}:
                continue
            relative = header.relative_to(REPO_ROOT)
            try:
                text = header.read_text(encoding='utf-8', errors='ignore')
            except OSError:
                continue
            for macro in _debug_only_definitions(text):
                issues.append(f'{relative.as_posix()}: release header defines debug/test-only macro {macro}')

    profile = FIRMWARE_DIR / 'sketch.yaml'
    if profile.is_file():
        for macro in _debug_only_definitions(profile.read_text(encoding='utf-8', errors='ignore')):
            issues.append(f'{profile.relative_to(REPO_ROOT).as_posix()}: release profile defines debug/test-only macro {macro}')
    return issues


# ---------------------------------------------------------------------------
# Phase 1 — Python Syntax
# ---------------------------------------------------------------------------

def check_python_files():
    errors = []
    guardian_dir = REPO_ROOT / 'tools' / 'guardian'
    for folder in [SCRIPTS_DIR, TESTS_DIR, guardian_dir]:
        if not folder.exists():
            continue
        for script in sorted(folder.rglob('*.py')):
            try:
                compile(script.read_text(encoding='utf-8'), str(script), 'exec')
            except SyntaxError as exc:
                errors.append(f'{script.name}: syntax error: {exc}')
    return errors


# ---------------------------------------------------------------------------
# Phase 2 — Firmware Safety Guardrails (enhanced)
# ---------------------------------------------------------------------------

def check_firmware_safety_guardrails():
    issues = []

    # 2a. Hard-stop feature flags — exact value 0
    config_path = FIRMWARE_DIR / 'src' / 'core' / 'config.h'
    if not config_path.exists():
        issues.append('config.h not found')
        return issues
    config_text = config_path.read_text(encoding='utf-8')
    for flag in ('FEATURE_BATTERY_MONITOR', 'FEATURE_AUDIO'):
        if not re.search(rf'#define\s+{flag}\s+0\b', config_text):
            issues.append(f'config.h: {flag} must be exactly 0 (hardware safety hard-stop)')

    # 2b. All 14 emulator teardowns in BmoGameboy.ino
    ino_path = FIRMWARE_DIR / 'BmoGameboy.ino'
    if ino_path.exists():
        ino_text = ino_path.read_text(encoding='utf-8')
        for td in EXPECTED_TEARDOWNS:
            if td not in ino_text:
                issues.append(f'BmoGameboy.ino: missing teardown call: {td}')
    else:
        issues.append('BmoGameboy.ino not found')

    # 2c. No naked Serial.print in emulator wrappers (hot path ban)
    emu_dir = FIRMWARE_DIR / 'src' / 'emulators'
    if emu_dir.exists():
        for cpp in sorted(emu_dir.glob('*.cpp')):
            text = cpp.read_text(encoding='utf-8')
            if re.search(r'\bSerial\.print', text):
                issues.append(
                    f'{cpp.name}: naked Serial.print in emulator wrapper — '
                    'use LOG_ macros (see 15_performance_budgets.md)'
                )

    # 2d. No unaligned pointer casts in emu_walnut.cpp
    walnut_cpp = FIRMWARE_DIR / 'src' / 'emulators' / 'emu_walnut.cpp'
    if walnut_cpp.exists():
        wt = walnut_cpp.read_text(encoding='utf-8')
        if '*(uint16_t*)&' in wt or '*(uint32_t*)&' in wt:
            issues.append('emu_walnut.cpp: unaligned raw pointer dereference (Xtensa safety)')

    # 2e. Partition table: app0 >= 8 MB, no overlaps
    partitions_csv = FIRMWARE_DIR / 'partitions.csv'
    if partitions_csv.exists():
        parts = _parse_partitions_csv(partitions_csv)
        if 'app0' not in parts:
            issues.append('partitions.csv: app0 partition missing')
        elif parts['app0']['size'] < 0x800000:
            sz = parts['app0']['size']
            issues.append(
                f'partitions.csv: app0 is {sz:,} bytes ({sz/1024/1024:.2f} MB) — must be >= 8 MB'
            )
        sorted_p = sorted(parts.values(), key=lambda p: p['offset'])
        for i in range(len(sorted_p) - 1):
            a, b = sorted_p[i], sorted_p[i + 1]
            if a['offset'] + a['size'] > b['offset']:
                issues.append(
                    f'partitions.csv: overlap between partition at '
                    f'0x{a["offset"]:06X}+{a["size"]:06X} and 0x{b["offset"]:06X}'
                )
    else:
        issues.append('partitions.csv not found in sketch directory')

    issues.extend(check_release_debug_macro_guardrails())
    return issues


# ---------------------------------------------------------------------------
# Phase 3 — Flash Budget (static vendor source size)
# ---------------------------------------------------------------------------

def check_flash_budget_static():
    """
    Sanity-check: sum vendor source tree size.
    Real gate is Phase 0 (arduino-cli). This catches accidentally added blobs.
    """
    issues = []
    vendor_dir = FIRMWARE_DIR / 'src' / 'vendor'
    if not vendor_dir.exists():
        return issues
    total = sum(f.stat().st_size for f in vendor_dir.rglob('*') if f.is_file())
    total_kb = total / 1024
    print(f'  Vendor source tree: {total_kb:.1f} KB')
    if total > 8 * 1024 * 1024:
        issues.append(
            f'Vendor source tree is {total_kb:.0f} KB — '
            'check for accidentally included binary blobs or uncompressed assets'
        )
    return issues


# ---------------------------------------------------------------------------
# Phase 4 — Structural Soundness
# ---------------------------------------------------------------------------

def check_structural_soundness():
    """
    Every emulator must have: .h, .cpp wrapper, destroy() wired in .ino,
    display stream method, and an entry in AGENT_MANIFEST.json with
    engine_status field. SD card must register all extensions.
    """
    issues = []
    emu_dir = FIRMWARE_DIR / 'src' / 'emulators'

    # 4a. All wrapper files exist
    for stem in EXPECTED_EMULATORS:
        for ext in ('.h', '.cpp'):
            f = emu_dir / (stem + ext)
            if not f.exists():
                issues.append(f'Missing emulator wrapper: src/emulators/{stem}{ext}')

    # 4b. AGENT_MANIFEST engine_status field per emulator
    manifest_path = REPO_ROOT / 'AGENT_MANIFEST.json'
    if manifest_path.exists():
        try:
            manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
            emulator_entries = manifest.get('emulators', [])
            wrappers_in_manifest = {e.get('wrapper', '') for e in emulator_entries}
            valid_statuses = {'production', 'stub', 'partial'}
            for stem in EXPECTED_EMULATORS:
                key = f'src/emulators/{stem}.cpp'
                if not any(key in w for w in wrappers_in_manifest):
                    issues.append(f'AGENT_MANIFEST.json: {stem}.cpp not registered')
            for entry in emulator_entries:
                status = entry.get('engine_status', '')
                if status not in valid_statuses:
                    issues.append(
                        f'AGENT_MANIFEST.json: emulator "{entry.get("name", "?")}" '
                        f'has invalid engine_status "{status}" '
                        f'(must be one of: {sorted(valid_statuses)})'
                    )
        except Exception as exc:
            issues.append(f'AGENT_MANIFEST.json: parse error: {exc}')

    # 4c. sd_card.cpp registers all extensions
    sd_cpp = FIRMWARE_DIR / 'src' / 'core' / 'sd_card.cpp'
    if sd_cpp.exists():
        sd_text = sd_cpp.read_text(encoding='utf-8')
        for ext in EXPECTED_EXTENSIONS:
            if ext not in sd_text:
                issues.append(f'sd_card.cpp: extension "{ext}" not registered')
    else:
        issues.append('sd_card.cpp not found')

    # 4d. display_emu.h declares all stream methods
    display_h = FIRMWARE_DIR / 'src' / 'core' / 'display_emu.h'
    if display_h.exists():
        header = display_h.read_text(encoding='utf-8')
        for m in REQUIRED_STREAM_METHODS:
            if m not in header:
                issues.append(f'display_emu.h: missing stream method: {m}')

    return issues


# ---------------------------------------------------------------------------
# Phase 5 — ROM Archive Data Health
# ---------------------------------------------------------------------------

def check_game_data():
    issues = []
    try:
        from repo_tools import find_game_roots, validate_rom_header
    except ImportError:
        issues.append('repo_tools.py not importable — cannot validate ROM archives')
        return issues

    roots = find_game_roots(REPO_ROOT)
    if not roots:
        return issues  # gitignored in clean clones

    import zipfile
    for label, game_dir in roots.items():
        zip_files = sorted(game_dir.glob('*.zip'))
        if not zip_files:
            issues.append(f'{label}: no zip files found.')
            continue
        for zip_file in zip_files:
            try:
                with zipfile.ZipFile(zip_file) as archive:
                    rom_name = next(
                        (n for n in archive.namelist()
                         if n.lower().endswith(('.gb', '.gbc'))), None
                    )
                    if rom_name is None:
                        issues.append(f'{zip_file.name}: no ROM file found inside archive.')
                        continue
                    rom_bytes = archive.read(rom_name)
                    ok, actual, expected = validate_rom_header(rom_bytes)
                    if not ok and len(rom_bytes) > 0x150:
                        issues.append(
                            f'{zip_file.name}: rom header checksum mismatch '
                            f'(actual 0x{actual:02X}, expected 0x{expected:02X})'
                        )
            except zipfile.BadZipFile:
                issues.append(f'{zip_file.name}: invalid zip archive.')
    return issues


# ---------------------------------------------------------------------------
# Phase 6 — Rules & Manifest Integrity
# ---------------------------------------------------------------------------

def check_rules_and_manifest_integrity():
    issues = []
    
    # Trigger live automated rebuild of AI Knowledge Graph, Decision Tree, Manifest, and Context Index
    try:
        from generate_ai_knowledge_base import build_knowledge_base
        build_knowledge_base()
    except Exception as exc:
        issues.append(f'Failed to build AI Knowledge Base: {exc}')

    manifest_path = REPO_ROOT / 'AGENT_MANIFEST.json'
    if not manifest_path.exists():
        issues.append('AGENT_MANIFEST.json is missing')
    else:
        try:
            json.loads(manifest_path.read_text(encoding='utf-8'))
        except Exception as exc:
            issues.append(f'AGENT_MANIFEST.json invalid JSON: {exc}')

    context_path = RULES_DIR / 'CONTEXT_INDEX.json'
    if not context_path.exists():
        issues.append('.agents/rules/CONTEXT_INDEX.json is missing')
    else:
        try:
            json.loads(context_path.read_text(encoding='utf-8'))
        except Exception as exc:
            issues.append(f'.agents/rules/CONTEXT_INDEX.json invalid JSON: {exc}')

    readme_path = RULES_DIR / 'README.md'
    if readme_path.exists():
        readme_text = readme_path.read_text(encoding='utf-8')
        for rule_file in sorted(RULES_DIR.glob('*.md')):
            if rule_file.name == 'README.md':
                continue
            if rule_file.name not in readme_text:
                issues.append(f'{rule_file.name} is not indexed in .agents/rules/README.md')

    return issues


# ---------------------------------------------------------------------------
# Phase 7 — Cross-Document & Rule Truth Parity
# ---------------------------------------------------------------------------

def check_doc_truth_parity():
    issues = []
    try:
        from tools.guardian.core.doc_verifier import DocTruthVerifier
        drift = DocTruthVerifier(REPO_ROOT).verify_all()
        for d in drift:
            issues.append(f'[{d.category}] {d.file_path}:{d.line_number}: {d.message}')
    except Exception as exc:
        issues.append(f'DocTruthVerifier failed: {exc}')
    return issues


# ---------------------------------------------------------------------------
# Phase 8 — UI/UX Design System & Anti-AI-Slop Standards
# ---------------------------------------------------------------------------

def check_ui_design_system():
    issues = []
    try:
        from tools.guardian.core.ui_auditor import UiAuditor
        report = UiAuditor(REPO_ROOT).run_all_checks()
        for issue in report.issues:
            issues.append(f'[{issue.category}] {issue.file_path}:{issue.line_number}: {issue.message}')
    except Exception as exc:
        issues.append(f'UiAuditor failed: {exc}')
    return issues


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

PHASES = [
    ('Phase 0: Binary Compilation Gate',       check_arduino_compile),
    ('Phase 1: Python Syntax',                 check_python_files),
    ('Phase 2: Firmware Safety Guardrails',    check_firmware_safety_guardrails),
    ('Phase 3: Flash Budget (static)',         check_flash_budget_static),
    ('Phase 4: Structural Soundness',          check_structural_soundness),
    ('Phase 5: ROM Archive Health',            check_game_data),
    ('Phase 6: Rules & Manifest Integrity',    check_rules_and_manifest_integrity),
    ('Phase 7: Cross-Doc & Rule Truth',        check_doc_truth_parity),
    ('Phase 8: UI Design System & Anti-AI-Slop', check_ui_design_system),
]


def main(argv=None):
    parser = argparse.ArgumentParser(description='Validate BMO Gameboy source, build, and documentation contracts.')
    parser.add_argument(
        '--require-target-compile',
        action='store_true',
        help='fail if the Arduino CLI target compile cannot run (also honors BMO_REQUIRE_TARGET_COMPILE=1)',
    )
    args = parser.parse_args(argv)
    env_strict = os.environ.get('BMO_REQUIRE_TARGET_COMPILE', '').strip().lower() in {'1', 'true', 'yes', 'on'}
    require_target_compile = args.require_target_compile or env_strict

    start = time.perf_counter()
    print('=' * 62)
    print('BMO Gameboy — AI Guardian CI Validator')
    print('=' * 62)

    all_issues = []
    for phase_name, phase_fn in PHASES:
        if phase_fn is check_arduino_compile:
            phase_fn = lambda: check_arduino_compile(require_target_compile)
        print(f'\n[{phase_name}]')
        errors = phase_fn()
        if errors:
            for e in errors:
                print(f'  FAIL: {e}')
            all_issues.extend(errors)
        else:
            print('  PASS')

    duration_ms = (time.perf_counter() - start) * 1000
    print('\n' + '=' * 62)
    if TARGET_COMPILE_STATUS == 'SKIPPED':
        print('TARGET COMPILE: SKIPPED - NOT VERIFIED')
    elif TARGET_COMPILE_STATUS == 'VERIFIED':
        print('TARGET COMPILE: VERIFIED')
    elif TARGET_COMPILE_STATUS == 'FAILED':
        print('TARGET COMPILE: FAILED - NOT VERIFIED')
    if all_issues:
        print(f'VALIDATION FAILED in {duration_ms:.0f} ms — {len(all_issues)} issue(s):')
        for issue in all_issues:
            print(f'  - {issue}')
        raise SystemExit(1)
    print(f'VALIDATION PASSED in {duration_ms:.0f} ms — all phases clean.')


if __name__ == '__main__':
    main()
