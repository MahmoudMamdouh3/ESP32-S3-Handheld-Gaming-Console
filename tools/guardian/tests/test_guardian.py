"""Unit tests for Guardian hardware, bus, AST, static analysis, link, and color checks."""

import io
import json
import runpy
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch

from scripts.check_markdown_links import find_markdown_link_issues
from scripts.verify_color_tokens import main as verify_color_tokens_main
from tools.guardian.cli import cmd_doctor, cmd_psram_budget
from tools.guardian.core.ast_linter import FirmwareAstLinter
from tools.guardian.core.bus_model import HardwareBusModel
from tools.guardian.core.clang_tidy_runner import ClangTidyRunner
from tools.guardian.core.cppcheck_runner import CppcheckFinding, CppcheckRunner

REPO_ROOT = Path(__file__).resolve().parents[3]
FIRMWARE_DIR = REPO_ROOT / "firmware" / "BmoGameboy"


class TestGuardianEngine(unittest.TestCase):
    """Verify Guardian hardware analyzers and static checks."""

    def test_01_bus_model_mathematical_precision(self):
        bus_model = HardwareBusModel(spi_clock_hz=80_000_000, cpu_clock_hz=240_000_000)

        metrics = bus_model.calculate_metrics("MENU_FULLSCREEN")
        self.assertEqual(metrics.resolution.frame_bytes, 153600)
        self.assertAlmostEqual(metrics.spi_transfer_ms, 15.3612, places=2)
        self.assertGreater(metrics.spi_bus_utilization_percent, 90.0)
        self.assertEqual(metrics.status, "LOW_SEQUENTIAL_HEADROOM")
        self.assertAlmostEqual(metrics.ideal_overlap_cpu_budget_ms, metrics.frame_period_ms)
        self.assertEqual(metrics.ideal_overlap_cpu_budget_percent, 100.0)

        gb_metrics = bus_model.calculate_metrics("GAMEBOY_DMG_SCALED")
        self.assertEqual(gb_metrics.resolution.frame_bytes, 103680)
        self.assertAlmostEqual(gb_metrics.spi_transfer_ms, 10.3692, places=2)

    def test_bus_model_does_not_infer_dma_or_frame_drops(self):
        fast_bus = HardwareBusModel(spi_clock_hz=1_000_000)
        saturated = fast_bus.calculate_metrics("MENU_FULLSCREEN")
        self.assertEqual(saturated.status, "BUS_SATURATED")
        self.assertGreater(saturated.spi_bus_utilization_percent, 100.0)

        normal_bus = HardwareBusModel()
        low_headroom = normal_bus.calculate_metrics("MENU_FULLSCREEN")
        self.assertEqual(low_headroom.status, "LOW_SEQUENTIAL_HEADROOM")

    def test_02_all_supported_resolutions_registered(self):
        all_metrics = HardwareBusModel().calculate_all()
        self.assertGreaterEqual(len(all_metrics), 15)
        keys = [m.resolution_key for m in all_metrics]
        for expected in (
            "GAMEBOY_DMG_SCALED",
            "NES_NATIVE",
            "DOOM_NATIVE",
            "SMS_NATIVE",
            "PCE_NATIVE",
            "GENESIS_NATIVE",
            "SNES_NATIVE",
        ):
            self.assertIn(expected, keys)

    def test_03_ast_linter_detects_real_firmware_state(self):
        findings = FirmwareAstLinter(FIRMWARE_DIR).lint_all()
        self.assertIsInstance(findings, list)
        for f in findings:
            self.assertIn(f.severity, {"CRITICAL", "WARNING", "ADVISORY"})
            self.assertTrue(f.rule_id)
            self.assertTrue(f.message)
        self.assertEqual([f for f in findings if f.severity == "CRITICAL"], [])

    def test_04_cppcheck_scans_all_firmware_sources(self):
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

    def test_05_cppcheck_timeout_is_reported(self):
        runner = CppcheckRunner(REPO_ROOT)
        runner.cppcheck_bin = "cppcheck"
        with patch(
            "tools.guardian.core.cppcheck_runner.subprocess.run",
            side_effect=subprocess.TimeoutExpired("cppcheck", 600),
        ):
            with self.assertRaisesRegex(RuntimeError, "timed out"):
                runner.run()

    def test_06_cppcheck_findings_and_branch_notes(self):
        runner = CppcheckRunner(REPO_ROOT)
        runner.cppcheck_bin = "cppcheck"
        output = (
            "src/example.c::0::information::branchCheck::scan note\n"
            "src/third_party/example.c::17::error::exampleCheck::detected issue\n"
        )
        with patch(
            "tools.guardian.core.cppcheck_runner.subprocess.run",
            return_value=subprocess.CompletedProcess([], 1, "", output),
        ):
            findings = runner.run()

        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].severity, "error")
        self.assertEqual(findings[0].line, 17)

        information = (
            "src/example.cpp::0::information::normalCheckLevelMaxBranches::"
            "Limiting analysis of branches."
        )
        with patch(
            "tools.guardian.core.cppcheck_runner.subprocess.run",
            return_value=subprocess.CompletedProcess([], 1, "", information),
        ):
            self.assertEqual(runner.run(), [])

    def test_07_guardian_module_propagates_cppcheck_failure(self):
        finding = CppcheckFinding("src/example.c", 17, "error", "exampleCheck", "detected issue")
        with (
            patch.object(sys, "argv", ["tools.guardian", "cppcheck"]),
            patch.object(CppcheckRunner, "is_available", return_value=True),
            patch.object(CppcheckRunner, "run", return_value=[finding]),
            redirect_stdout(io.StringIO()),
        ):
            with self.assertRaises(SystemExit) as result:
                runpy.run_module("tools.guardian", run_name="__main__")
        self.assertEqual(result.exception.code, 1)

    def test_08_ast_linter_ignores_comments_and_handles_local_arrays(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            firmware = root / "firmware" / "BmoGameboy"
            sketch = firmware / "BmoGameboy.ino"
            sketch.parent.mkdir(parents=True)
            sketch.write_text(
                "void loop() {\n  // rebuildVisibleGames() is called only when dirty.\n}\n",
                encoding="utf-8",
            )
            findings = FirmwareAstLinter(firmware).lint_file(sketch)
            self.assertFalse(any(f.rule_id == "UNGUARDED_PER_FRAME_FILTER" for f in findings))

            source = firmware / "src" / "hal" / "probe.cpp"
            source.parent.mkdir(parents=True)
            source.write_text(
                "void probe() {\n  uint8_t scratch[2048];\n  static uint8_t shared[4096];\n}\n",
                encoding="utf-8",
            )
            self.assertEqual(
                [item.rule_id for item in FirmwareAstLinter(firmware).lint_file(source)],
                ["LARGE_LOCAL_STACK_ARRAY"],
            )

            vendor = firmware / "src" / "third_party" / "probe.c"
            vendor.parent.mkdir(parents=True)
            vendor.write_text("void probe() {\n  uint8_t scratch[4096];\n}\n", encoding="utf-8")
            self.assertEqual(FirmwareAstLinter(firmware).lint_file(vendor), [])

    def test_09_clang_tidy_runner_executes_and_parses_diagnostics(self):
        runner = ClangTidyRunner(REPO_ROOT)
        targets = runner.default_targets()
        self.assertGreaterEqual(len(targets), 12)
        runner.clang_tidy_bin = "clang-tidy"
        fake_out = "tests/host/host_cores_runner.c:42:9: warning: Dereference of null pointer [clang-analyzer-core.NullDereference]\n"
        with patch(
            "tools.guardian.core.clang_tidy_runner.subprocess.run",
            return_value=subprocess.CompletedProcess([], 0, fake_out, ""),
        ):
            findings = runner.run()
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].check_name, "clang-analyzer-core.NullDereference")
        self.assertEqual(findings[0].line, 42)

    def test_10_markdown_link_audit_rejects_file_uris_and_respects_nested_fences(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "docs").mkdir()
            (root / "docs" / "index.md").write_text(
                "`````\n"
                "[ignored inside fence](missing.md)\n"
                "`````\n"
                "[local](../ok.md) [missing](gone.md) [legacy](file:///C:/old.md)\n",
                encoding="utf-8",
            )
            (root / "ok.md").write_text("# okay\n", encoding="utf-8")
            issues = find_markdown_link_issues(root)
            self.assertEqual([item[2] for item in issues], ["UNRESOLVED_LINK", "FILE_URI_LINK"])
            self.assertEqual(issues[0][1], 4)

    def test_11_doctor_psram_budget_and_color_tokens(self):
        class Args:
            json = True

        buf = io.StringIO()
        with redirect_stdout(buf):
            self.assertEqual(cmd_doctor(None), 0)
            psram_buf = io.StringIO()
            with redirect_stdout(psram_buf):
                self.assertEqual(cmd_psram_budget(Args()), 0)
            payload = json.loads(psram_buf.getvalue())
            self.assertEqual(payload["rom_file_sizeof"], 72)
            self.assertEqual(verify_color_tokens_main(), 0)


if __name__ == "__main__":
    unittest.main()
