"""
test_guardian.py — Unit Tests for the Guardian Ground-Truth Engine
"""

import json
import unittest
from pathlib import Path

from tools.guardian.core.ast_linter import FirmwareAstLinter, LintFinding
from tools.guardian.core.bus_model import HardwareBusModel, SUPPORTED_RESOLUTIONS
from tools.guardian.core.cppcheck_runner import CppcheckRunner
from tools.guardian.core.elf_analyzer import ElfAnalyzer
from tools.guardian.core.host_bench import HostBenchmarkSuite
from tools.guardian.core.report_gen import ReportGenerator

REPO_ROOT = Path(__file__).resolve().parents[3]
FIRMWARE_DIR = REPO_ROOT / "firmware" / "BmoGameboy"


class TestGuardianEngine(unittest.TestCase):
    """Verifies that the Guardian framework operates reliably."""

    def test_01_bus_model_mathematical_precision(self):
        """Verify that bus model calculates transfer times accurately."""
        bus_model = HardwareBusModel(spi_clock_hz=80_000_000, cpu_clock_hz=240_000_000)
        
        # Test 320x240 (153,600 bytes)
        metrics = bus_model.calculate_metrics("MENU_FULLSCREEN")
        self.assertEqual(metrics.resolution.frame_bytes, 153600)
        # At 80MHz (10MB/s), 153,600 bytes is ~15.36 ms + 1.2us overhead
        self.assertAlmostEqual(metrics.spi_transfer_ms, 15.3612, places=2)
        self.assertGreater(metrics.spi_bus_utilization_percent, 90.0)
        self.assertTrue(metrics.dma_required_for_60fps)

        # Test Game Boy 240x216 (103,680 bytes)
        gb_metrics = bus_model.calculate_metrics("GAMEBOY_DMG_SCALED")
        self.assertEqual(gb_metrics.resolution.frame_bytes, 103680)
        self.assertAlmostEqual(gb_metrics.spi_transfer_ms, 10.3692, places=2)

    def test_02_all_supported_resolutions_registered(self):
        """Verify that all 15 console resolutions are present in the model."""
        bus_model = HardwareBusModel()
        all_metrics = bus_model.calculate_all()
        self.assertGreaterEqual(len(all_metrics), 15)
        keys = [m.resolution_key for m in all_metrics]
        self.assertIn("GAMEBOY_DMG_SCALED", keys)
        self.assertIn("NES_NATIVE", keys)
        self.assertIn("DOOM_NATIVE", keys)
        self.assertIn("SMS_NATIVE", keys)
        self.assertIn("PCE_NATIVE", keys)
        self.assertIn("GENESIS_NATIVE", keys)
        self.assertIn("SNES_NATIVE", keys)

    def test_03_ast_linter_detects_real_firmware_state(self):
        """Verify that AST linter executes cleanly across firmware files."""
        linter = FirmwareAstLinter(FIRMWARE_DIR)
        findings = linter.lint_all()
        self.assertIsInstance(findings, list)
        # Verify finding structure
        for f in findings:
            self.assertIn(f.severity, {"CRITICAL", "WARNING", "ADVISORY"})
            self.assertTrue(f.rule_id)
            self.assertTrue(f.message)

    def test_04_host_benchmarks_execute_and_produce_metrics(self):
        """Verify host microbenchmarks run and produce valid quantitative numbers."""
        suite = HostBenchmarkSuite()
        metrics = suite.run_all()
        self.assertEqual(len(metrics), 7)
        for m in metrics:
            self.assertGreater(m.iterations, 0)
            self.assertGreater(m.total_time_ms, 0.0)
            self.assertGreater(m.throughput_mops, 0.0)
            self.assertTrue(m.name)

    def test_05_report_generator_markdown_and_json(self):
        """Verify report generator produces valid Markdown and parseable JSON."""
        bus_model = HardwareBusModel()
        bus_metrics = bus_model.calculate_all()
        linter = FirmwareAstLinter(FIRMWARE_DIR)
        ast_findings = linter.lint_all()
        file_stats = linter.get_repo_line_stats()
        elf_analyzer = ElfAnalyzer(REPO_ROOT)
        elf_res = elf_analyzer.analyze()
        suite = HostBenchmarkSuite()
        bench_metrics = suite.run_all()
        cppcheck = CppcheckRunner(REPO_ROOT)
        cppcheck_findings = []

        # Markdown
        md = ReportGenerator.generate_markdown(
            elf_res, bus_metrics, ast_findings, cppcheck_findings, bench_metrics, file_stats
        )
        self.assertIn("BMO Handheld Console — Complete Line-by-Line Benchmark", md)
        self.assertIn("Hardware Bus & Frame Transmission Physics Model", md)
        self.assertIn("Comprehensive Line-by-Line Repository Codebase Audit", md)

        # JSON
        json_str = ReportGenerator.generate_json(
            elf_res, bus_metrics, ast_findings, cppcheck_findings, bench_metrics, file_stats
        )
        data = json.loads(json_str)
        self.assertIn("timestamp", data)
        self.assertIn("bus_metrics", data)
        self.assertIn("microbenchmarks", data)
        self.assertIn("file_stats", data)
        self.assertGreater(len(data["file_stats"]), 50)


if __name__ == "__main__":
    unittest.main()
