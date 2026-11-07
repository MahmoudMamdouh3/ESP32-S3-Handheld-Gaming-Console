import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from scripts.rom_key import make_rom_key, normalize_rom_path
from host_test_support import find_wasi_cxx, require_compiler


REPO_ROOT = Path(__file__).resolve().parents[1]


class RomKeyTests(unittest.TestCase):
    def test_key_is_bounded_and_uses_full_relative_path(self):
        left = make_rom_key("folder-a/Long Game Name With Shared Prefix 001.gb")
        right = make_rom_key("folder-b/Long Game Name With Shared Prefix 001.gb")
        self.assertLessEqual(len(left), 32)
        self.assertNotEqual(left, right)
        self.assertRegex(left, r"^[a-z0-9_-]+_[0-9a-f]{8}$")

    def test_normalizes_slashes_and_ascii_case_for_fat_paths(self):
        self.assertEqual(make_rom_key("/Games\\Zelda.GB"), make_rom_key("games/zelda.gb"))
        self.assertEqual(normalize_rom_path("/Games\\Zelda.GB"), "games/zelda.gb")

    def test_sanitizes_empty_and_punctuation_only_stems(self):
        self.assertTrue(make_rom_key("folder/!!!.nes").startswith("rom_"))
        self.assertTrue(make_rom_key("A---B.nes").startswith("a---b_"))

    def test_rejects_empty_paths(self):
        with self.assertRaises(ValueError):
            make_rom_key("")

    def test_rejects_directory_without_filename(self):
        with self.assertRaises(ValueError):
            make_rom_key("folder/")

    def test_cpp_and_python_implementations_match(self):
        compiler = find_wasi_cxx(REPO_ROOT)
        node = shutil.which("node")
        require_compiler(self, compiler, "Pinned WASI C++ compiler")
        if not node:
            self.skipTest("Node.js is required to run the WebAssembly ROM-key parity fixture")

        source = REPO_ROOT / "tests" / "host" / "rom_key_parity.cpp"
        include = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "core"
        cases = [
            "folder-a/Long Game Name With Shared Prefix 001.gb",
            "/Games\\Zelda.GB",
            "folder/!!!.nes",
            "A---B.nes",
            "folder/___a very long prefix followed by a valid title!.pce",
            "folder/éclair.sfc",
        ]

        temp_root = REPO_ROOT / "build" / "test-tmp"
        temp_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="bmo-rom-key-", dir=temp_root) as temp_dir:
            wasm = Path(temp_dir) / "rom_key_parity.wasm"
            subprocess.run(
                [str(compiler), "--target=wasm32-wasip1", "-std=c++17", "-I", str(include),
                 str(source), "-o", str(wasm)],
                cwd=REPO_ROOT,
                check=True,
                capture_output=True,
                text=True,
            )
            node_runner = (
                "const fs=require('node:fs');"
                "const {WASI}=require('node:wasi');"
                "const wasi=new WASI({version:'preview1',args:process.argv.slice(1),env:process.env,preopens:{}});"
                "WebAssembly.compile(fs.readFileSync(process.argv[1])).then(m=>"
                "wasi.start(new WebAssembly.Instance(m,{wasi_snapshot_preview1:wasi.wasiImport})));"
            )
            result = subprocess.run(
                [node, "-e", node_runner, str(wasm), *cases],
                cwd=REPO_ROOT,
                check=True,
                capture_output=True,
                text=True,
            )
        self.assertEqual(result.stdout.splitlines(), [make_rom_key(path) for path in cases])


if __name__ == "__main__":
    unittest.main()
