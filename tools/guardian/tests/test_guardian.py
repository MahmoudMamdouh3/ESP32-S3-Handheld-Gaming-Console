"""
test_guardian.py — Unit Tests for the Guardian Ground-Truth Engine
"""

import json
import io
import runpy
import subprocess
import tempfile
import sys
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch

from tools.guardian.core.ast_linter import FirmwareAstLinter, LintFinding
from tools.guardian.core.bus_model import HardwareBusModel, SUPPORTED_RESOLUTIONS
from tools.guardian.core.cppcheck_runner import CppcheckFinding, CppcheckRunner
from tools.guardian.core.doc_verifier import DocTruthVerifier
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
        self.assertGreaterEqual(len(metrics), 7)
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

    def test_06_cppcheck_scans_all_firmware_sources(self):
        runner = CppcheckRunner(REPO_ROOT)
        runner.cppcheck_bin = "cppcheck"
        with patch(
            "tools.guardian.core.cppcheck_runner.subprocess.run",
            return_value=subprocess.CompletedProcess([], 0, "", ""),
        ) as run:
            self.assertEqual(runner.run(), [])

        command = run.call_args.args[0]
        self.assertIn(str(FIRMWARE_DIR / "src"), command)
        self.assertEqual(run.call_args.kwargs["timeout"], 600)
        self.assertIn("--error-exitcode=1", command)

    def test_07_cppcheck_timeout_is_reported(self):
        runner = CppcheckRunner(REPO_ROOT)
        runner.cppcheck_bin = "cppcheck"
        with patch(
            "tools.guardian.core.cppcheck_runner.subprocess.run",
            side_effect=subprocess.TimeoutExpired("cppcheck", 600),
        ):
            with self.assertRaisesRegex(RuntimeError, "timed out"):
                runner.run()

    def test_08_cppcheck_findings_are_returned(self):
        runner = CppcheckRunner(REPO_ROOT)
        runner.cppcheck_bin = "cppcheck"
        output = (
            "src/example.c::0::information::branchCheck::scan note\n"
            "src/vendor/example.c::17::error::exampleCheck::detected issue\n"
        )
        with patch(
            "tools.guardian.core.cppcheck_runner.subprocess.run",
            return_value=subprocess.CompletedProcess([], 1, "", output),
        ):
            findings = runner.run()

        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].severity, "error")
        self.assertEqual(findings[0].line, 17)

    def test_09_guardian_module_propagates_cppcheck_failure(self):
        finding = CppcheckFinding(
            "src/example.c", 17, "error", "exampleCheck", "detected issue"
        )
        with (
            patch.object(sys, "argv", ["tools.guardian", "cppcheck"]),
            patch.object(CppcheckRunner, "is_available", return_value=True),
            patch.object(CppcheckRunner, "run", return_value=[finding]),
            redirect_stdout(io.StringIO()),
        ):
            with self.assertRaises(SystemExit) as result:
                runpy.run_module("tools.guardian", run_name="__main__")

        self.assertEqual(result.exception.code, 1)

    def test_10_ast_linter_ignores_per_frame_filter_comments(self):
        with tempfile.TemporaryDirectory() as tmp:
            sketch = Path(tmp) / "BmoGameboy.ino"
            sketch.write_text(
                "void loop() {\n"
                "    // rebuildVisibleGames() is called only when dirty.\n"
                "}\n",
                encoding="utf-8",
            )
            findings = FirmwareAstLinter(Path(tmp)).lint_all()

        self.assertFalse(
            any(f.rule_id == "UNGUARDED_PER_FRAME_FILTER" for f in findings)
        )

    def test_11_doc_truth_verifier_zero_drift(self):
        """Verify zero documentation/rule pin or architecture drift against config.h."""
        verifier = DocTruthVerifier(REPO_ROOT)
        issues = verifier.verify_all()
        self.assertEqual(issues, [], f"DocTruthVerifier found drift: {issues}")

    def test_12_prompt_pack_generates_context_packet(self):
        """Verify prompt-pack generates a complete task context packet with live C++ signatures."""
        from tools.guardian.cli import cmd_prompt_pack

        class DummyArgs:
            query = ["fix", "st7789", "display", "spi"]

        buf = io.StringIO()
        with redirect_stdout(buf):
            rc = cmd_prompt_pack(DummyArgs())
        out = buf.getvalue()
        self.assertEqual(rc, 0)
        self.assertIn("MATCHED INTENT: modify_display_rendering_or_spi", out)
        self.assertIn("TFT_CS=10", out)
        self.assertIn("display_emu.h", out)

    def test_13_benchmark_ai_passes_100_percent(self):
        """Verify 5-dimension AI workflow benchmark achieves 100.0% pass."""
        from tools.guardian.cli import cmd_benchmark_ai

        buf = io.StringIO()
        with redirect_stdout(buf):
            rc = cmd_benchmark_ai(None)
        self.assertEqual(rc, 0, buf.getvalue())

    def test_14_doc_truth_verifier_rejects_buzzwords(self):
        """Verify DocTruthVerifier catches banned AI buzzwords (Rule 43)."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "README.md").write_text(
                "# Project\nThis uses a state-of-the-art buttery-smooth renderer.\n",
                encoding="utf-8",
            )
            verifier = DocTruthVerifier(root)
            issues = verifier.verify_no_buzzwords()
            self.assertEqual(len(issues), 2)
            self.assertEqual(issues[0].category, "BUZZWORD_VIOLATION")

    def test_15_patch_check_catches_unbalanced_braces_and_buzzwords(self):
        from tools.guardian.cli import cmd_patch_check

        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "bad_syntax.c"
            p.write_text("void broken() { int x = 1;\n", encoding="utf-8")

            class DummyArgs:
                files = [str(p)]
                strict = False

            buf = io.StringIO()
            with redirect_stdout(buf):
                rc = cmd_patch_check(DummyArgs())
            self.assertEqual(rc, 1)
            self.assertIn("Unbalanced curly braces", buf.getvalue())

    def test_16_export_harness_scaffolds_portable_rules(self):
        from tools.guardian.cli import cmd_export_harness

        with tempfile.TemporaryDirectory() as tmp:
            class DummyArgs:
                destination = tmp
                project_name = "ExportTest"

            buf = io.StringIO()
            with redirect_stdout(buf):
                rc = cmd_export_harness(DummyArgs())
            self.assertEqual(rc, 0)
            dest_path = Path(tmp)
            self.assertTrue((dest_path / "AGENTS.md").exists())
            self.assertTrue((dest_path / ".agents" / "rules" / "00_hard_stops.md").exists())
            self.assertTrue((dest_path / ".agents" / "rules" / "43_no_ai_jargon_and_buzzwords.md").exists())
            self.assertTrue((dest_path / "tools" / "guardian" / "cli.py").exists())
            self.assertTrue((dest_path / "scripts" / "install_git_hooks.py").exists())


if __name__ == "__main__":
    unittest.main()
