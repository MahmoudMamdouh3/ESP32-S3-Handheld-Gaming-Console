"""
tools.guardian.cli — Command Line Interface for BMO Guardian Performance & Ground-Truth Suite
Usage:
  python -m tools.guardian audit
  python -m tools.guardian bus-calc
  python -m tools.guardian profile-elf
  python -m tools.guardian bench-host
  python -m tools.guardian cppcheck
  python -m tools.guardian report [--output PATH] [--json]
"""

import argparse
import sys
from pathlib import Path

# Safe encoding for Windows consoles
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

from .core.ast_linter import FirmwareAstLinter
from .core.bus_model import HardwareBusModel
from .core.cppcheck_runner import CppcheckRunner
from .core.elf_analyzer import ElfAnalyzer
from .core.host_bench import HostBenchmarkSuite
from .core.report_gen import ReportGenerator

REPO_ROOT = Path(__file__).resolve().parents[2]
FIRMWARE_DIR = REPO_ROOT / "firmware" / "BmoGameboy"


def cmd_audit(args) -> int:
    """Runs a complete brutal performance & quality audit."""
    print("=" * 70)
    print(" BMO GUARDIAN -- Ground-Truth Performance & Quality Audit")
    print("=" * 70)
    
    # 1. Bus Model
    print("\n[1/5] Calculating Hardware Bus & Frame Budgets (80MHz SPI)...")
    bus_model = HardwareBusModel()
    bus_metrics = bus_model.calculate_all()
    pinched = [b for b in bus_metrics if b.dma_required_for_60fps]
    print(f"  Processed {len(bus_metrics)} console viewport modes.")
    print(f"  Modes requiring DMA double-buffering for 60 FPS: {len(pinched)}")

    # 2. Static AST Linter
    print("\n[2/5] Running Embedded Firmware AST & Anti-Pattern Linter...")
    linter = FirmwareAstLinter(FIRMWARE_DIR)
    ast_findings = linter.lint_all()
    critical = [f for f in ast_findings if f.severity == "CRITICAL"]
    warnings = [f for f in ast_findings if f.severity == "WARNING"]
    print(f"  AST Violations: {len(critical)} Critical, {len(warnings)} Warnings.")
    for f in critical:
        print(f"    [CRITICAL] [{f.rule_id}] {f.file_path.name}:{f.line_number} -- {f.message}")
    for f in warnings[:5]:
        print(f"    [WARNING]  [{f.rule_id}] {f.file_path.name}:{f.line_number} -- {f.message}")

    # 3. ELF Introspection
    print("\n[3/5] Introspecting Xtensa ESP32-S3 Binary Symbols & Sections...")
    elf_analyzer = ElfAnalyzer(REPO_ROOT)
    elf_res = elf_analyzer.analyze()
    print(f"  Internal SRAM (DRAM): {elf_res.dram_used_bytes:,} / {elf_res.dram_total_bytes:,} bytes ({(elf_res.dram_used_bytes/elf_res.dram_total_bytes)*100:.1f}%)")
    print(f"  Flash (app0):         {elf_res.flash_used_bytes:,} / {elf_res.flash_total_bytes:,} bytes ({(elf_res.flash_used_bytes/elf_res.flash_total_bytes)*100:.1f}%)")

    # 4. Host Benchmarks
    print("\n[4/5] Running Host CPU & Mathematical SDF Microbenchmarks...")
    bench_suite = HostBenchmarkSuite()
    bench_metrics = bench_suite.run_all()
    for bm in bench_metrics:
        print(f"  * {bm.name:36s} | {bm.avg_latency_us:8.3f} us | {bm.throughput_mops:7.2f} MOps/s")

    # 5. Cppcheck Static Analysis
    print("\n[5/5] Checking Static Analysis Engine (Cppcheck)...")
    cppcheck = CppcheckRunner(REPO_ROOT)
    cppcheck_findings = []
    if hasattr(args, "cppcheck") and args.cppcheck:
        print("  Running Cppcheck deep static analysis...")
        cppcheck_findings = cppcheck.run() if cppcheck.is_available() else []
        print(f"  Cppcheck available: {cppcheck.is_available()} (Findings: {len(cppcheck_findings)})")
    else:
        print(f"  Cppcheck toolchain detected: {cppcheck.is_available()} (use --cppcheck for deep analysis)")

    print("\n" + "=" * 70)
    if critical:
        print(f" AUDIT RESULT: FAIL -- {len(critical)} Critical Violations Found.")
        return 1
    else:
        print(" AUDIT RESULT: PASS -- Clean Performance & Quality Baseline.")
        return 0


def cmd_bus_calc(args) -> int:
    """Calculates SPI transmission latency and CPU compute budgets."""
    bus_model = HardwareBusModel()
    metrics = bus_model.calculate_all()
    
    print(f"{'Console / Viewport':32s} | {'Res':9s} | {'SPI ms':7s} | {'Target':6s} | {'Seq CPU %':9s} | {'Status'}")
    print("-" * 85)
    for m in metrics:
        res_str = f"{m.resolution.rendered_w}x{m.resolution.rendered_h}"
        print(f"{m.resolution.name:32s} | {res_str:9s} | {m.spi_transfer_ms:6.2f}ms | {m.resolution.target_fps:5.1f} | {m.sequential_cpu_budget_percent:8.1f}% | {m.status}")
    return 0


def cmd_profile_elf(args) -> int:
    """Introspects compiled Xtensa binary."""
    elf_analyzer = ElfAnalyzer(REPO_ROOT)
    target_path = Path(args.elf) if hasattr(args, "elf") and args.elf else None
    res = elf_analyzer.analyze(target_path)
    
    print(f"Target ELF: {res.elf_path or 'Fallback Baseline'}")
    print(f"Toolchain available: {res.toolchain_found}")
    print(f"DRAM Usage: {res.dram_used_bytes:,} / {res.dram_total_bytes:,} bytes")
    print(f"Flash Usage: {res.flash_used_bytes:,} / {res.flash_total_bytes:,} bytes")
    
    if res.largest_functions:
        print("\nTop 10 Largest Functions:")
        for s in res.largest_functions[:10]:
            print(f"  {s.size:6d} B  |  {s.name}")
    return 0


def cmd_bench_host(args) -> int:
    """Runs microbenchmarks."""
    suite = HostBenchmarkSuite()
    metrics = suite.run_all()
    print(f"{'Benchmark Name':36s} | {'Iterations':10s} | {'Avg Time':12s} | {'Throughput'}")
    print("-" * 75)
    for m in metrics:
        print(f"{m.name:36s} | {m.iterations:10,d} | {m.avg_latency_us:8.3f} us  | {m.throughput_mops:7.2f} MOps/s")
    return 0


def cmd_report(args) -> int:
    """Generates complete Markdown or JSON report."""
    bus_model = HardwareBusModel()
    bus_metrics = bus_model.calculate_all()
    
    linter = FirmwareAstLinter(FIRMWARE_DIR)
    ast_findings = linter.lint_all()
    
    elf_analyzer = ElfAnalyzer(REPO_ROOT)
    elf_res = elf_analyzer.analyze()
    
    bench_suite = HostBenchmarkSuite()
    bench_metrics = bench_suite.run_all()
    
    cppcheck = CppcheckRunner(REPO_ROOT)
    cppcheck_findings = cppcheck.run() if (hasattr(args, "cppcheck") and args.cppcheck and cppcheck.is_available()) else []
    
    if hasattr(args, "json") and args.json:
        content = ReportGenerator.generate_json(
            elf_res, bus_metrics, ast_findings, cppcheck_findings, bench_metrics
        )
    else:
        content = ReportGenerator.generate_markdown(
            elf_res, bus_metrics, ast_findings, cppcheck_findings, bench_metrics
        )
        
    if hasattr(args, "output") and args.output:
        out_path = Path(args.output)
        out_path.write_text(content, encoding="utf-8")
        print(f"Report saved to: {out_path}")
    else:
        print(content)
        
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="BMO Guardian Performance & Ground-Truth Suite")
    subparsers = parser.add_subparsers(dest="command", help="Available subcommands")

    # audit
    p_audit = subparsers.add_parser("audit", help="Run complete performance & quality audit")
    p_audit.add_argument("--cppcheck", action="store_true", help="Run deep Cppcheck static analysis")

    # bus-calc
    subparsers.add_parser("bus-calc", help="Calculate SPI bus transfer times and CPU budgets")

    # profile-elf
    p_elf = subparsers.add_parser("profile-elf", help="Introspect Xtensa binary symbols and sections")
    p_elf.add_argument("--elf", help="Path to .elf binary (optional)")

    # bench-host
    subparsers.add_parser("bench-host", help="Run host-side microbenchmarks")

    # cppcheck
    subparsers.add_parser("cppcheck", help="Run cppcheck static analysis")

    # report
    p_rep = subparsers.add_parser("report", help="Generate full performance & ground-truth report")
    p_rep.add_argument("--output", "-o", help="Output file path (e.g. report.md)")
    p_rep.add_argument("--json", action="store_true", help="Output JSON instead of Markdown")

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        return 0

    commands = {
        "audit": cmd_audit,
        "bus-calc": cmd_bus_calc,
        "profile-elf": cmd_profile_elf,
        "bench-host": cmd_bench_host,
        "cppcheck": cmd_audit,
        "report": cmd_report,
    }

    handler = commands.get(args.command)
    if handler:
        return handler(args)
    return 1


if __name__ == "__main__":
    sys.exit(main())
