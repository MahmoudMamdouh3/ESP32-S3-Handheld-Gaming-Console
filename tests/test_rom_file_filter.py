import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from host_test_support import find_wasi_cxx, require_compiler

REPO_ROOT = Path(__file__).resolve().parents[1]


class RomFileFilterTests(unittest.TestCase):
    def test_synthetic_sd_listing_samples(self):
        compiler = find_wasi_cxx(REPO_ROOT)
        node = shutil.which("node")
        require_compiler(self, compiler, "Pinned WASI C++ compiler")
        require_compiler(self, node, "Node.js WASI runner")
        source = REPO_ROOT / "tests" / "host" / "rom_file_filter_test.cpp"
        include = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "storage"

        with tempfile.TemporaryDirectory() as temp_dir:
            wasm = Path(temp_dir) / "rom_file_filter_test.wasm"
            subprocess.run(
                [str(compiler), "--target=wasm32-wasip1", "-std=c++17", "-I", str(include),
                 str(source), "-o", str(wasm)],
                cwd=REPO_ROOT,
                check=True,
                capture_output=True,
                text=True,
            )
            runner = (
                "const fs=require('fs'); const {WASI}=require('node:wasi'); "
                "(async()=>{const wasi=new WASI({version:'preview1',args:[process.argv[1]]}); "
                "const mod=await WebAssembly.compile(fs.readFileSync(process.argv[1])); "
                "const instance=await WebAssembly.instantiate(mod,{wasi_snapshot_preview1:wasi.wasiImport}); "
                "wasi.start(instance);})().catch(e=>{console.error(e);process.exit(1);});"
            )
            subprocess.run([node, "-e", runner, str(wasm)], cwd=REPO_ROOT,
                           check=True, capture_output=True, text=True)

    def test_atari_7800_extension_is_not_mapped_to_atari_2600(self):
        scanner = (REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "storage" / "sd_card.cpp").read_text(encoding="utf-8")
        atari = (REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "engines" / "stella" / "atari.c").read_text(encoding="utf-8")
        self.assertNotIn(".a78", scanner.lower())
        self.assertIn("Atari 2600 (VCS)", atari)
        for script_rel in (
            "scripts/deploy_to_sd.py",
            "scripts/deploy_sd_complete.py",
            "simulator/console_history_data.js",
        ):
            self.assertNotIn(".a78", (REPO_ROOT / script_rel).read_text(encoding="utf-8").lower())


if __name__ == "__main__":
    unittest.main()
