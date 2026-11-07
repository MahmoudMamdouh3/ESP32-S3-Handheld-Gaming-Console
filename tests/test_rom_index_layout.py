import subprocess
import unittest
from pathlib import Path
from host_test_support import find_wasi_cxx, require_compiler


REPO_ROOT = Path(__file__).resolve().parents[1]


class RomIndexLayoutTests(unittest.TestCase):
    def test_v2_disk_layout_compile_time_contract(self):
        compiler = find_wasi_cxx(REPO_ROOT)
        source = REPO_ROOT / "tests" / "host" / "rom_index_layout_test.cpp"
        include = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "core"
        require_compiler(self, compiler, "Pinned WASI C++ compiler")
        subprocess.run(
            [str(compiler), "--target=wasm32-wasip1", "-std=c++17", "-I", str(include),
             "-fsyntax-only", str(source)],
            cwd=REPO_ROOT,
            check=True,
            capture_output=True,
            text=True,
        )


if __name__ == "__main__":
    unittest.main()
