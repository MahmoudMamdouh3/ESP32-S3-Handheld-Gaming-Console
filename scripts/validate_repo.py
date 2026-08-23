import time
from pathlib import Path

from repo_tools import find_game_roots, validate_rom_header

REPO_ROOT = Path(__file__).resolve().parent


def check_python_files():
    errors = []
    for script in sorted(REPO_ROOT.glob('*.py')):
        if script.name == 'validate_repo.py':
            continue
        try:
            compile(script.read_text(encoding='utf-8'), str(script), 'exec')
        except SyntaxError as exc:
            errors.append(f'{script.name}: syntax error: {exc}')
    return errors


def check_game_data():
    issues = []
    roots = find_game_roots(REPO_ROOT)
    if not roots:
        issues.append('No game directories found under the repo root.')
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


def main():
    start = time.perf_counter()
    print('Validating repo health...')

    python_issues = check_python_files()
    repo_issues = check_game_data()

    all_issues = python_issues + repo_issues
    duration_ms = (time.perf_counter() - start) * 1000

    if all_issues:
        print(f'Validation failed in {duration_ms:.2f} ms.')
        for issue in all_issues:
            print(f' - {issue}')
        raise SystemExit(1)

    print(f'Validation passed in {duration_ms:.2f} ms.')
    print('Python syntax checks passed and game archives validated.')


if __name__ == '__main__':
    main()
