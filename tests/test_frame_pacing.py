import subprocess
import unittest
from pathlib import Path
import re
from host_test_support import find_wasi_cxx, require_compiler


REPO_ROOT = Path(__file__).resolve().parents[1]


class FramePacingTests(unittest.TestCase):
    def test_header_periods_match_documented_source_rate_derivations(self):
        header = (REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "core" / "frame_pacing.h").read_text(encoding="utf-8")
        self.assertRegex(header, r"case\s+WALNUT_GBC:\s*case\s+PEANUT_GB:\s*return\s+16742U")
        self.assertRegex(header, r"case\s+DOOM:\s*return\s+28571U")
        self.assertRegex(header, r"case\s+PICO8:\s*return\s+33333U")
        self.assertRegex(header, r"default:\s*return\s+16667U")
        self.assertIn("4,194,304 / 70,224", header)
        self.assertIn("TICRATE as 35", header)
        self.assertIn("30 Hz", header)
        self.assertIn("60 Hz", header)

        self.assertEqual((1_000_000 * 70_224) // 4_194_304, 16_742)
        self.assertEqual(1_000_000 // 35, 28_571)
        self.assertEqual(1_000_000 // 30, 33_333)
        self.assertEqual((1_000_000 + 60 - 1) // 60, 16_667)

    def test_nominal_per_core_budgets_compile(self):
        compiler = find_wasi_cxx(REPO_ROOT)
        source = REPO_ROOT / "tests" / "host" / "frame_pacing_test.cpp"
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
