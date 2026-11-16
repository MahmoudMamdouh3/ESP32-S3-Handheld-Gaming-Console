import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.guardian.core.elf_analyzer import ElfAnalyzer


class ElfAnalyzerTests(unittest.TestCase):
    def test_discovers_arduino_cli_data_directory_toolchain(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            data_dir = Path(tmpdir) / "arduino-data"
            bin_dir = (
                data_dir
                / "packages"
                / "esp32"
                / "tools"
                / "esp-x32"
                / "2601"
                / "bin"
            )
            bin_dir.mkdir(parents=True)
            (bin_dir / "xtensa-esp32s3-elf-size").touch()
            env = {
                "ARDUINO_DIRECTORIES_DATA": str(data_dir),
                "LOCALAPPDATA": "",
                "USERPROFILE": "",
            }

            with patch.dict(os.environ, env, clear=False):
                analyzer = ElfAnalyzer(Path(tmpdir))

            self.assertEqual(analyzer.toolchain_bin, bin_dir)
            self.assertEqual(analyzer.get_tool("size"), str(bin_dir / "xtensa-esp32s3-elf-size"))

    def test_discovers_arbitrary_toolchain_version_folder(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            data_dir = Path(tmpdir) / "arduino-data"
            bin_dir = (
                data_dir
                / "packages"
                / "esp32"
                / "tools"
                / "esp-x32"
                / "2703"
                / "bin"
            )
            bin_dir.mkdir(parents=True)
            (bin_dir / "xtensa-esp32s3-elf-size").touch()
            env = {
                "ARDUINO_DIRECTORIES_DATA": str(data_dir),
                "LOCALAPPDATA": "",
                "USERPROFILE": "",
            }

            with patch.dict(os.environ, env, clear=False):
                analyzer = ElfAnalyzer(Path(tmpdir))

            self.assertEqual(analyzer.toolchain_bin, bin_dir)

    def test_analyze_raises_explicitly_when_elf_or_toolchain_missing(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            env = {
                "ARDUINO_DIRECTORIES_DATA": str(root / "missing-arduino"),
                "LOCALAPPDATA": "",
                "USERPROFILE": "",
                "PATH": "",
            }
            with patch.dict(os.environ, env, clear=False):
                analyzer = ElfAnalyzer(root)
                with self.assertRaises(FileNotFoundError):
                    analyzer.analyze()

                fake_elf = root / "build" / "BmoGameboy.ino.elf"
                fake_elf.parent.mkdir(parents=True)
                fake_elf.write_bytes(b"\x7fELF")
                with self.assertRaises(RuntimeError):
                    analyzer.analyze(fake_elf)


if __name__ == "__main__":
    unittest.main()
