import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

# Ensure scripts directory is in sys.path
SCRIPTS_DIR = Path(__file__).resolve().parents[1] / 'scripts'
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

from repo_tools import find_game_roots, get_data_dir, sanitize_identifier, validate_rom_header
from tools import bmo_tools
from validate_repo import (
    check_python_files,
    check_firmware_safety_guardrails,
    check_structural_soundness,
)


class RepoToolsTests(unittest.TestCase):
    def test_find_game_roots_finds_repo_game_dirs(self):
        repo_root = Path(__file__).resolve().parents[1]
        roots = find_game_roots(repo_root)
        self.assertIsInstance(roots, dict)

    def test_get_data_dir_unset_and_override_behavior(self):
        repo_root = Path(__file__).resolve().parents[1]
        with patch.dict(os.environ, {}, clear=True):
            self.assertEqual(get_data_dir(repo_root), repo_root)
        with tempfile.TemporaryDirectory() as tmpdir:
            with patch.dict(os.environ, {"BMO_DATA_DIR": tmpdir}, clear=False):
                self.assertEqual(get_data_dir(repo_root), Path(tmpdir))
            missing = Path(tmpdir) / "does_not_exist"
            with patch.dict(os.environ, {"BMO_DATA_DIR": str(missing)}, clear=False):
                with self.assertRaises(FileNotFoundError):
                    get_data_dir(repo_root)

    def test_sanitize_identifier_uses_stable_names(self):
        self.assertEqual(sanitize_identifier('Super Mario Land (World) (Rev 1)'), 'supermariolandworldrev1')
        self.assertEqual(sanitize_identifier('Legend of Zelda, The - Link\'s Awakening'), 'legendofzeldathelinksawakening')
        self.assertEqual(sanitize_identifier('!@#$'), 'game')

    def test_validate_rom_header_rejects_small_files(self):
        ok, actual, expected = validate_rom_header(b'too small')
        self.assertFalse(ok)
        self.assertEqual(actual, 0)
        self.assertEqual(expected, 0)

    def test_validate_rom_header_computes_correct_checksum(self):
        # Construct a dummy ROM header (must be > 0x150 bytes)
        rom = bytearray(0x151)
        # Fill the title/header area with some data (0x0134 to 0x014C)
        checksum = 0
        for i in range(0x0134, 0x014C + 1):
            rom[i] = i & 0xFF
            checksum = (checksum - rom[i] - 1) & 0xFF
        rom[0x014D] = checksum  # Set correct checksum
        
        ok, actual, expected = validate_rom_header(bytes(rom))
        self.assertTrue(ok)
        self.assertEqual(actual, expected)
        self.assertEqual(actual, checksum)
        
        # Test invalid checksum
        rom[0x014D] = (checksum + 1) & 0xFF
        ok, actual, expected = validate_rom_header(bytes(rom))
        self.assertFalse(ok)
        self.assertEqual(actual, checksum)
        self.assertEqual(expected, (checksum + 1) & 0xFF)

    def test_check_python_files_passes(self):
        errors = check_python_files()
        self.assertEqual(errors, [], f"Python syntax check failed: {errors}")

    def test_check_firmware_safety_guardrails_passes(self):
        issues = check_firmware_safety_guardrails()
        self.assertEqual(issues, [], f"Firmware safety guardrails failed: {issues}")

    def test_check_structural_soundness_passes(self):
        issues = check_structural_soundness()
        self.assertEqual(issues, [], f"Structural soundness check failed: {issues}")

    def test_bmo_tools_package_exports_shared_helpers(self):
        from scripts.rom_key import make_rom_key

        self.assertEqual(bmo_tools.sanitize_identifier("Test ROM!"), sanitize_identifier("Test ROM!"))
        self.assertEqual(bmo_tools.make_rom_key("gb/Tetris.gb"), make_rom_key("gb/Tetris.gb"))
        self.assertEqual(bmo_tools.to_bgr565_swapped(255, 255, 255), 0xFFFF)
        with tempfile.TemporaryDirectory() as tmpdir:
            self.assertEqual(bmo_tools.require_dir(Path(tmpdir)), Path(tmpdir))
            with self.assertRaises(FileNotFoundError):
                bmo_tools.require_dir(Path(tmpdir) / "missing")


if __name__ == '__main__':
    unittest.main()
