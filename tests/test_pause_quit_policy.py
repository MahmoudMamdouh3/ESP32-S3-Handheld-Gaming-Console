import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from host_test_support import find_wasi_cxx, require_compiler

REPO_ROOT = Path(__file__).resolve().parents[1]


class PauseQuitPolicyTests(unittest.TestCase):
    def test_save_failure_requires_retry_or_explicit_quit_anyway(self):
        compiler = find_wasi_cxx(REPO_ROOT)
        node = shutil.which("node")
        require_compiler(self, compiler, "Pinned WASI C++ compiler")
        require_compiler(self, node, "Node.js WASI runner")
        source = REPO_ROOT / "tests" / "host" / "pause_quit_policy_test.cpp"
        include = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "ui"

        with tempfile.TemporaryDirectory() as temp_dir:
            wasm = Path(temp_dir) / "pause_quit_policy_test.wasm"
            subprocess.run(
                [str(compiler), "--target=wasm32-wasip1", "-std=c++17", "-I", str(include),
                 str(source), "-o", str(wasm)], cwd=REPO_ROOT, check=True,
                capture_output=True, text=True)
            runner = (
                "const fs=require('fs'); const {WASI}=require('node:wasi'); "
                "(async()=>{const wasi=new WASI({version:'preview1',args:[process.argv[1]]}); "
                "const mod=await WebAssembly.compile(fs.readFileSync(process.argv[1])); "
                "const instance=await WebAssembly.instantiate(mod,{wasi_snapshot_preview1:wasi.wasiImport}); "
                "wasi.start(instance);})().catch(e=>{console.error(e);process.exit(1);});"
            )
            subprocess.run([node, "-e", runner, str(wasm)], cwd=REPO_ROOT,
                           check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
