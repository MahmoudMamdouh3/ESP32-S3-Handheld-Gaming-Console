import subprocess
import unittest
from pathlib import Path

from tools.guardian.core.psram_budget import PsramBudgetAnalyzer
from host_test_support import find_wasi_cxx, require_compiler


REPO_ROOT = Path(__file__).resolve().parents[1]


class PsramBudgetTests(unittest.TestCase):
    def test_pool_capacity_rounds_headroom_up_and_caps(self):
        self.assertEqual(PsramBudgetAnalyzer.compute_pool_capacity(1), 256)
        self.assertEqual(PsramBudgetAnalyzer.compute_pool_capacity(2000), 2500)
        self.assertEqual(PsramBudgetAnalyzer.compute_pool_capacity(2001), 2502)
        self.assertEqual(PsramBudgetAnalyzer.compute_pool_capacity(40000), 36000)

    def test_report_uses_rom_file_declaration_and_reports_aggregate_upper_bound(self):
        report = PsramBudgetAnalyzer(REPO_ROOT).analyze()
        self.assertEqual(report.rom_file_sizeof, 72)
        self.assertEqual(report.persistent_total_max, 3656192)
        self.assertIn("DOOM zone upper bound", PsramBudgetAnalyzer(REPO_ROOT).format_markdown())

    def test_firmware_zone_sizing_compile_time_contract(self):
        compiler = find_wasi_cxx(REPO_ROOT)
        source = REPO_ROOT / "tests" / "host" / "psram_zone_budget_test.cpp"
        require_compiler(self, compiler, "Pinned WASI C++ compiler")
        subprocess.run(
            [str(compiler), "--target=wasm32-wasip1", "-std=c++17", "-fsyntax-only", str(source)],
            cwd=REPO_ROOT,
            check=True,
            capture_output=True,
            text=True,
        )


if __name__ == "__main__":
    unittest.main()
