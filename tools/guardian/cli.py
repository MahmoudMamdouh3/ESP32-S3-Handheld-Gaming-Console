"""Command-line interface for static firmware, bus, ELF, and PSRAM analyzers.

Usage:
  python -m tools.guardian audit [--cppcheck]
  python -m tools.guardian bus-calc
  python -m tools.guardian profile-elf [--elf PATH]
  python -m tools.guardian budget [--elf PATH]
  python -m tools.guardian psram-budget [--json]
  python -m tools.guardian cppcheck
  python -m tools.guardian clang-tidy [--files PATH ...]
  python -m tools.guardian doctor
  python -m tools.guardian verify
"""

import argparse
import json
import re
import shutil
import sys
from pathlib import Path
from typing import Dict

if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

from .core.ast_linter import FirmwareAstLinter
from .core.bus_model import HardwareBusModel
from .core.clang_tidy_runner import ClangTidyRunner
from .core.cppcheck_runner import CppcheckRunner
from .core.elf_analyzer import ElfAnalyzer
from .core.psram_budget import PsramBudgetAnalyzer

REPO_ROOT = Path(__file__).resolve().parents[2]
FIRMWARE_DIR = REPO_ROOT / "firmware" / "BmoGameboy"


def _parse_config_defines(config_path: Path) -> Dict[str, int]:
    defines: Dict[str, int] = {}
    if not config_path.exists():
        return defines
    for line in config_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        m = re.match(r"^\s*#define\s+([A-Z0-9_]+)\s+(\d+)\b", line)
        if m:
            defines[m.group(1)] = int(m.group(2))
    return defines


def cmd_audit(args) -> int:
    """Run firmware AST linter, analytical bus model, ELF metrics, and optional Cppcheck."""
    print("=" * 70)
    print(" BMO GUARDIAN -- Firmware & Hardware Static Audit")
    print("=" * 70)

    print("\n[1/4] Calculating Analytical SPI Frame Budgets (80 MHz)...")
    bus_metrics = HardwareBusModel().calculate_all()
    pinched = [b for b in bus_metrics if b.status == "LOW_SEQUENTIAL_HEADROOM"]
    saturated = [b for b in bus_metrics if b.status == "BUS_SATURATED"]
    print(f"  Viewport modes: {len(bus_metrics)} | Low sequential headroom (<40%): {len(pinched)} | Exceeds period: {len(saturated)}")

    print("\n[2/4] Running Embedded Firmware AST Linter...")
    linter = FirmwareAstLinter(FIRMWARE_DIR)
    ast_findings = linter.lint_all()
    file_stats = linter.get_repo_line_stats()
    total_lines = sum(f.total_lines for f in file_stats)
    total_sloc = sum(f.sloc for f in file_stats)
    critical = [f for f in ast_findings if f.severity == "CRITICAL"]
    warnings = [f for f in ast_findings if f.severity == "WARNING"]
    print(f"  Audited {len(file_stats)} files ({total_lines:,} lines, {total_sloc:,} SLOC).")
    print(f"  Findings: {len(critical)} Critical, {len(warnings)} Warnings.")
    for f in critical:
        print(f"    [CRITICAL] [{f.rule_id}] {f.file_path.name}:{f.line_number} -- {f.message}")
    for f in warnings[:5]:
        print(f"    [WARNING]  [{f.rule_id}] {f.file_path.name}:{f.line_number} -- {f.message}")

    print("\n[3/4] Introspecting Xtensa ESP32-S3 ELF...")
    elf_res = ElfAnalyzer(REPO_ROOT).analyze()
    print(f"  IRAM: {elf_res.iram_used_bytes:,} B | DRAM: {elf_res.dram_used_bytes:,}/{elf_res.dram_total_bytes:,} B | Flash: {elf_res.flash_used_bytes:,}/{elf_res.flash_total_bytes:,} B")

    print("\n[4/4] Cppcheck Static Analysis...")
    cppcheck = CppcheckRunner(REPO_ROOT)
    cppcheck_findings = []
    if getattr(args, "cppcheck", False):
        if not cppcheck.is_available():
            print("Cppcheck was requested but is not installed.")
            return 2
        cppcheck_findings = cppcheck.run()
        print(f"  Findings: {len(cppcheck_findings)}")
        for finding in cppcheck_findings:
            print(f"    [{finding.severity.upper()}] {finding.file_path}:{finding.line} [{finding.error_id}] {finding.message}")
    else:
        print(f"  Cppcheck available: {cppcheck.is_available()} (pass --cppcheck to run)")

    cppcheck_errors = [f for f in cppcheck_findings if f.severity == "error"]
    print("\n" + "=" * 70)
    if critical or cppcheck_errors:
        print(f" AUDIT RESULT: FAIL ({len(critical)} Critical AST, {len(cppcheck_errors)} Cppcheck errors)")
        return 1
    print(" AUDIT RESULT: PASS")
    return 0


def cmd_bus_calc(args) -> int:
    """Calculate analytical SPI transfer estimates per viewport mode."""
    metrics = HardwareBusModel().calculate_all()
    print(f"{'Console / Viewport':32s} | {'Res':9s} | {'SPI ms':7s} | {'Target':6s} | {'Seq CPU %':9s} | {'Status'}")
    print("-" * 85)
    for m in metrics:
        res_str = f"{m.resolution.rendered_w}x{m.resolution.rendered_h}"
        print(f"{m.resolution.name:32s} | {res_str:9s} | {m.spi_transfer_ms:6.2f}ms | {m.resolution.target_fps:5.1f} | {m.sequential_cpu_budget_percent:8.1f}% | {m.status}")
    return 0


def cmd_profile_elf(args) -> int:
    """Introspect compiled Xtensa ELF sections and largest symbols."""
    target_path = Path(args.elf) if getattr(args, "elf", None) else None
    res = ElfAnalyzer(REPO_ROOT).analyze(target_path)
    print(f"Target ELF: {res.elf_path or 'Fallback Baseline'}")
    print(f"Toolchain available: {res.toolchain_found}")
    print(f"IRAM Usage:  {res.iram_used_bytes:,} bytes")
    print(f"DRAM Usage:  {res.dram_used_bytes:,} / {res.dram_total_bytes:,} bytes")
    print(f"Flash Usage: {res.flash_used_bytes:,} / {res.flash_total_bytes:,} bytes")
    if res.sections:
        print("\nELF section sizes:")
        for section in res.sections:
            print(f"  {section.name:28s} {section.size:>10,d} bytes  {section.memory_type}")
    if res.largest_functions:
        print("\nTop 10 Largest Functions:")
        for s in res.largest_functions[:10]:
            print(f"  {s.size:6d} B  |  {s.name}")
    return 0


def cmd_cppcheck(args) -> int:
    """Run Cppcheck across the firmware source tree."""
    cppcheck = CppcheckRunner(REPO_ROOT)
    if not cppcheck.is_available():
        print("Cppcheck is not installed or could not be located.")
        return 2
    findings = cppcheck.run()
    for finding in findings:
        print(f"[{finding.severity.upper()}] {finding.file_path}:{finding.line} [{finding.error_id}] {finding.message}")
    errors = sum(f.severity == "error" for f in findings)
    print(f"Cppcheck findings: {len(findings)} total, {errors} errors.")
    return 1 if errors else 0


def cmd_clang_tidy(args) -> int:
    """Run clang-tidy static analysis across host-portable C/C++ targets."""
    runner = ClangTidyRunner(REPO_ROOT)
    if not runner.is_available():
        print("clang-tidy is not installed or could not be located.")
        return 2
    target_files = None
    if getattr(args, "files", None):
        target_files = [
            Path(f) if Path(f).is_absolute() else (REPO_ROOT / f)
            for f in args.files
            if (Path(f) if Path(f).is_absolute() else (REPO_ROOT / f)).exists()
        ]
    findings = runner.run(target_files)
    for f in findings:
        print(f"[{f.severity.upper()}] {f.file_path}:{f.line}:{f.column} [{f.check_name}] {f.message}")
    errors = sum(f.severity == "error" for f in findings)
    print(f"clang-tidy findings: {len(findings)} total, {errors} errors.")
    return 1 if errors else 0


def cmd_budget(args) -> int:
    """Inspect Flash app0 partition headroom against a compiled ELF."""
    total_flash = 16 * 1024 * 1024
    app0_size = 8 * 1024 * 1024
    target_path = Path(args.elf) if getattr(args, "elf", None) else None
    elf_res = ElfAnalyzer(REPO_ROOT).analyze(target_path)
    print("Flash & SRAM Allocation Overview (ESP32-S3-N16R8):")
    print(f"  * Physical Flash Chip         : {total_flash / (1024 * 1024):.1f} MB ({total_flash:,} bytes)")
    print(f"  * Firmware App Partition (app0): {app0_size / (1024 * 1024):.1f} MB ({app0_size:,} bytes)")
    print(f"  * Compiled ELF Flash Used     : {elf_res.flash_used_bytes / (1024 * 1024):.2f} MB ({elf_res.flash_used_bytes:,} bytes)")
    print(f"  * Compiled ELF DRAM Used      : {elf_res.dram_used_bytes / 1024:.1f} KB ({elf_res.dram_used_bytes:,} / {elf_res.dram_total_bytes:,} bytes)")
    print(f"  * Compiled ELF IRAM Used      : {elf_res.iram_used_bytes / 1024:.1f} KB ({elf_res.iram_used_bytes:,} bytes)")
    headroom = app0_size - elf_res.flash_used_bytes
    print(f"  * App0 Partition Headroom     : {headroom / (1024 * 1024):.2f} MB ({headroom:,} bytes free)")
    return 0


def cmd_psram_budget(args) -> int:
    """Compute source-derived persistent PSRAM ledger and DOOM zone sizing."""
    analyzer = PsramBudgetAnalyzer(REPO_ROOT)
    if getattr(args, "json", False):
        print(json.dumps(analyzer.to_dict(), indent=2))
    else:
        print(analyzer.format_markdown())
    return 0


def cmd_doctor(args) -> int:
    """Preflight check for hardware hard stops, AST criticals, and available toolchains."""
    issues = 0
    cfg_path = FIRMWARE_DIR / "src" / "hal" / "config.h"
    cfg = _parse_config_defines(cfg_path)

    print("[1/3] Hardware Hard Stops (config.h):")
    for macro, label in (
        ("FEATURE_BATTERY_MONITOR", "no battery voltage divider on GPIO 1"),
        ("FEATURE_AUDIO", "no I2S DAC fitted on GPIO 38-40"),
    ):
        if cfg.get(macro, -1) != 0:
            print(f"  [FAIL] {macro} != 0 ({label})")
            issues += 1
        else:
            print(f"  [PASS] {macro} == 0")

    print("\n[2/3] Firmware AST Linter:")
    findings = FirmwareAstLinter(FIRMWARE_DIR).lint_all()
    criticals = [f for f in findings if f.severity == "CRITICAL"]
    if criticals:
        for c in criticals:
            print(f"  [FAIL] [{c.rule_id}] {c.file_path.name}:{c.line_number}: {c.message}")
        issues += len(criticals)
    else:
        print(f"  [PASS] 0 Critical AST findings ({len(findings)} advisory/warning)")

    print("\n[3/3] Local Toolchains:")
    arduino_cli = (REPO_ROOT / "arduino-cli.exe").exists() or bool(shutil.which("arduino-cli"))
    elf_analyzer = ElfAnalyzer(REPO_ROOT)
    cppcheck = CppcheckRunner(REPO_ROOT)
    clang_tidy = ClangTidyRunner(REPO_ROOT)
    print(f"  * arduino-cli : {'READY' if arduino_cli else 'NOT FOUND'}")
    print(f"  * xtensa nm   : {'READY' if elf_analyzer.get_tool('nm') else 'NOT FOUND'}")
    print(f"  * cppcheck    : {'READY' if cppcheck.is_available() else 'NOT FOUND'}")
    print(f"  * clang-tidy  : {'READY' if clang_tidy.is_available() else 'NOT FOUND'}")
    return 1 if issues else 0


def cmd_verify(args) -> int:
    """Run AST critical check and all repository + guardian unittest suites."""
    import unittest

    findings = FirmwareAstLinter(FIRMWARE_DIR).lint_all()
    criticals = [f for f in findings if f.severity == "CRITICAL"]
    if criticals:
        print(f"[FAIL] {len(criticals)} Critical AST violations found:")
        for c in criticals:
            print(f"  * [{c.rule_id}] {c.file_path.name}:{c.line_number} -- {c.message}")
        return 1
    print(f"[PASS] AST Linter: 0 Critical violations ({len(findings)} warnings/advisories).")

    suite = unittest.TestSuite()
    suite.addTests(unittest.TestLoader().discover(str(REPO_ROOT / "tests"), pattern="test_*.py"))
    suite.addTests(unittest.TestLoader().discover(str(REPO_ROOT / "tools" / "guardian" / "tests"), pattern="test_*.py"))
    result = unittest.TextTestRunner(verbosity=1).run(suite)
    return 0 if result.wasSuccessful() else 1


def main() -> int:
    parser = argparse.ArgumentParser(description="BMO Handheld Hardware & Firmware Static Analyzers")
    subparsers = parser.add_subparsers(dest="command")

    p_audit = subparsers.add_parser("audit", help="Run AST linter, bus estimates, and ELF summary")
    p_audit.add_argument("--cppcheck", action="store_true", help="Also run Cppcheck")

    subparsers.add_parser("bus-calc", help="Calculate analytical SPI transfer estimates")

    p_elf = subparsers.add_parser("profile-elf", help="Inspect Xtensa ELF section sizes and large symbols")
    p_elf.add_argument("--elf", help="Path to .elf binary")

    p_budget = subparsers.add_parser("budget", help="Inspect Flash app0 headroom from ELF")
    p_budget.add_argument("--elf", help="Path to .elf binary")

    p_psram = subparsers.add_parser("psram-budget", help="Compute source-derived PSRAM ledger")
    p_psram.add_argument("--json", action="store_true", help="Output JSON")

    subparsers.add_parser("cppcheck", help="Run Cppcheck across firmware sources")
    p_tidy = subparsers.add_parser("clang-tidy", help="Run clang-tidy across host-portable C/C++ targets")
    p_tidy.add_argument("--files", nargs="*", help="Specific C/C++ files")

    subparsers.add_parser("doctor", help="Check hardware hard stops, AST criticals, and toolchains")
    subparsers.add_parser("verify", help="Run AST critical check and unittest suites")

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        return 0

    commands = {
        "audit": cmd_audit,
        "bus-calc": cmd_bus_calc,
        "profile-elf": cmd_profile_elf,
        "budget": cmd_budget,
        "psram-budget": cmd_psram_budget,
        "cppcheck": cmd_cppcheck,
        "clang-tidy": cmd_clang_tidy,
        "doctor": cmd_doctor,
        "verify": cmd_verify,
    }
    return commands[args.command](args)


if __name__ == "__main__":
    sys.exit(main())
