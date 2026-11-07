import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from host_test_support import find_wasi_cxx, require_compiler

REPO_ROOT = Path(__file__).resolve().parents[1]


class BmoFaceSpringSourceTests(unittest.TestCase):
    def test_source_spring_host_simulation_and_bounds(self):
        compiler = find_wasi_cxx(REPO_ROOT)
        node = shutil.which("node")
        require_compiler(self, compiler, "Pinned WASI C++ compiler")
        require_compiler(self, node, "Node.js WASI runner")
        source = REPO_ROOT / "tests" / "host" / "harmonic_spring_test.cpp"
        include = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "core"

        with tempfile.TemporaryDirectory() as temp_dir:
            wasm = Path(temp_dir) / "harmonic_spring_test.wasm"
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
            result = subprocess.run(
                [node, "-e", runner, str(wasm)],
                cwd=REPO_ROOT,
                check=True,
                capture_output=True,
                text=True,
            )

        lines = [line for line in result.stdout.splitlines() if line.startswith("dt=")]
        self.assertEqual(len(lines), 2, result.stdout)
        self.assertIn("dt=0.01666667", lines[0])
        self.assertIn("dt=0.05000000", lines[1])
        values = [dict(field.split("=", 1) for field in line.split()) for line in lines]
        for sample in values:
            self.assertAlmostEqual(float(sample["value"]), 1.0, delta=0.001)
            self.assertTrue(float(sample["overshoot"]) >= 0.0)
        self.assertAlmostEqual(float(values[0]["overshoot"]), 0.01811899, delta=1e-5)
        self.assertAlmostEqual(float(values[1]["overshoot"]), 0.00582316, delta=1e-5)


if __name__ == "__main__":
    unittest.main()
