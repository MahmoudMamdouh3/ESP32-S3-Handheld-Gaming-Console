import json
import re
import time
from pathlib import Path

from repo_tools import find_game_roots, validate_rom_header

REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_DIR = Path(__file__).resolve().parent
TESTS_DIR = REPO_ROOT / 'tests'
FIRMWARE_DIR = REPO_ROOT / 'firmware' / 'BmoGameboy'
RULES_DIR = REPO_ROOT / '.agents' / 'rules'


def check_python_files():
    errors = []
    for folder in [SCRIPTS_DIR, TESTS_DIR]:
        if not folder.exists():
            continue
        for script in sorted(folder.glob('*.py')):
            try:
                compile(script.read_text(encoding='utf-8'), str(script), 'exec')
            except SyntaxError as exc:
                errors.append(f'{script.name}: syntax error: {exc}')
    return errors


def check_game_data():
    issues = []
    roots = find_game_roots(REPO_ROOT)
    if not roots:
        # Game directories are gitignored in clean clones
        return issues

    for label, game_dir in roots.items():
        zip_files = sorted(game_dir.glob('*.zip'))
        if not zip_files:
            issues.append(f'{label}: no zip files found.')
            continue

        for zip_file in zip_files:
            import zipfile
            try:
                with zipfile.ZipFile(zip_file) as archive:
                    rom_name = next((name for name in archive.namelist() if name.lower().endswith(('.gb', '.gbc'))), None)
                    if rom_name is None:
                        issues.append(f'{zip_file.name}: no ROM file found inside the archive.')
                        continue
                    rom_bytes = archive.read(rom_name)
                    ok, actual, expected = validate_rom_header(rom_bytes)
                    if not ok and len(rom_bytes) > 0x150:
                        issues.append(
                            f'{zip_file.name}: rom header checksum mismatch (actual 0x{actual:02X}, expected 0x{expected:02X})'
                        )
            except zipfile.BadZipFile:
                issues.append(f'{zip_file.name}: invalid zip archive.')
    return issues


def check_firmware_safety_guardrails():
    issues = []
    config_path = FIRMWARE_DIR / 'src' / 'core' / 'config.h'
    if not config_path.exists():
        issues.append('config.h not found')
        return issues

    config_text = config_path.read_text(encoding='utf-8')

    # Check dormant hardware feature flags
    if not re.search(r'#define\s+FEATURE_BATTERY_MONITOR\s+0', config_text):
        issues.append('config.h: FEATURE_BATTERY_MONITOR must remain 0 while hardware is dormant')
    if not re.search(r'#define\s+FEATURE_AUDIO\s+0', config_text):
        issues.append('config.h: FEATURE_AUDIO must remain 0 while hardware is dormant')

    # Check BmoGameboy.ino teardown
    ino_path = FIRMWARE_DIR / 'BmoGameboy.ino'
    if ino_path.exists():
        ino_text = ino_path.read_text(encoding='utf-8')
        for core in ['WalnutEmu::destroy()', 'PeanutEmu::destroy()', 'NesEmu::destroy()', 'DoomEmu::destroy()']:
            if core not in ino_text:
                issues.append(f'BmoGameboy.ino: missing teardown call {core} in SELECT+UP handler')

    # Check for unaligned pointer casts in emu_walnut.cpp (ROM reads)
    walnut_cpp = FIRMWARE_DIR / 'src' / 'emulators' / 'emu_walnut.cpp'
    if walnut_cpp.exists():
        walnut_text = walnut_cpp.read_text(encoding='utf-8')
        if '*(uint16_t*)&' in walnut_text or '*(uint32_t*)&' in walnut_text:
            issues.append('emu_walnut.cpp: contains dangerous unaligned raw pointer dereference')

    # Check partition table has 8MB app0
    partitions_csv = FIRMWARE_DIR / 'partitions.csv'
    if partitions_csv.exists():
        part_text = partitions_csv.read_text(encoding='utf-8')
        if 'app0' not in part_text or ('8M' not in part_text and '0x800000' not in part_text):
            issues.append('partitions.csv: app0 partition must be at least 8MB')

    return issues


def check_rules_and_manifest_integrity():
    issues = []
    # Check AGENT_MANIFEST.json
    manifest_path = REPO_ROOT / 'AGENT_MANIFEST.json'
    if not manifest_path.exists():
        issues.append('AGENT_MANIFEST.json is missing')
    else:
        try:
            json.loads(manifest_path.read_text(encoding='utf-8'))
        except Exception as exc:
            issues.append(f'AGENT_MANIFEST.json invalid JSON: {exc}')

    # Check CONTEXT_INDEX.json
    context_path = RULES_DIR / 'CONTEXT_INDEX.json'
    if not context_path.exists():
        issues.append('.agents/rules/CONTEXT_INDEX.json is missing')
    else:
        try:
            json.loads(context_path.read_text(encoding='utf-8'))
        except Exception as exc:
            issues.append(f'.agents/rules/CONTEXT_INDEX.json invalid JSON: {exc}')

    # Check rules README index
    readme_path = RULES_DIR / 'README.md'
    if readme_path.exists():
        readme_text = readme_path.read_text(encoding='utf-8')
        for rule_file in sorted(RULES_DIR.glob('*.md')):
            if rule_file.name == 'README.md':
                continue
            if rule_file.name not in readme_text:
                issues.append(f'{rule_file.name} is not indexed in .agents/rules/README.md')

    return issues


def main():
    start = time.perf_counter()
    print('Validating repo health & AI agent guardrails...')

    python_issues = check_python_files()
    repo_issues = check_game_data()
    safety_issues = check_firmware_safety_guardrails()
    integrity_issues = check_rules_and_manifest_integrity()

    all_issues = python_issues + repo_issues + safety_issues + integrity_issues
    duration_ms = (time.perf_counter() - start) * 1000

    if all_issues:
        print(f'Validation failed in {duration_ms:.2f} ms.')
        for issue in all_issues:
            print(f' - {issue}')
        raise SystemExit(1)

    print(f'Validation passed in {duration_ms:.2f} ms.')
    print('All checks passed: Python syntax, ROM archives, safety guardrails, and ruleset integrity.')


if __name__ == '__main__':
    main()
