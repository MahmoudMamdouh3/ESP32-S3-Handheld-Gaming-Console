import unittest
from pathlib import Path

from repo_tools import find_game_roots, sanitize_identifier


class RepoToolsTests(unittest.TestCase):
    def test_find_game_roots_finds_repo_game_dirs(self):
        roots = find_game_roots(Path(__file__).resolve().parents[1])
        self.assertIn('gameboy_games', roots)
        self.assertIn('gameboy_color_games', roots)
        self.assertTrue(roots['gameboy_games'].exists())

    def test_sanitize_identifier_uses_stable_names(self):
        self.assertEqual(sanitize_identifier('Super Mario Land (World) (Rev 1)'), 'supermariolandworldrev1')
        self.assertEqual(sanitize_identifier('Legend of Zelda, The - Link\'s Awakening'), 'legendofzeldathelinksawakening')
        self.assertEqual(sanitize_identifier('!@#$'), 'game')

    def test_validate_rom_header_rejects_small_files(self):
        from repo_tools import validate_rom_header
        ok, actual, expected = validate_rom_header(b'too small')
        self.assertFalse(ok)
        self.assertEqual(actual, 0)
        self.assertEqual(expected, 0)

    def test_validate_rom_header_computes_correct_checksum(self):
        from repo_tools import validate_rom_header
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

if __name__ == '__main__':
    unittest.main()
