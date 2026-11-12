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


if __name__ == "__main__":
    unittest.main()
