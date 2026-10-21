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

    # 2. Static AST Linter & Line-by-Line Code Density
    print("\n[2/5] Running Embedded Firmware AST Linter & Line-by-Line Audit...")
    linter = FirmwareAstLinter(FIRMWARE_DIR)
    ast_findings = linter.lint_all()
    file_stats = linter.get_repo_line_stats()
    total_lines = sum(f.total_lines for f in file_stats)
    total_sloc = sum(f.sloc for f in file_stats)
    total_comments = sum(f.comment_lines for f in file_stats)
    critical = [f for f in ast_findings if f.severity == "CRITICAL"]
    warnings = [f for f in ast_findings if f.severity == "WARNING"]
    print(f"  Audited {len(file_stats)} repository source files ({total_lines:,} total lines, {total_sloc:,} SLOC, {total_comments:,} comments).")
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
        if not cppcheck.is_available():
            print("Cppcheck was requested but is not installed or could not be located.")
            return 2
        cppcheck_findings = cppcheck.run()
        print(f"  Cppcheck available: {cppcheck.is_available()} (Findings: {len(cppcheck_findings)})")
        for finding in cppcheck_findings:
            print(
                f"    [{finding.severity.upper()}] {finding.file_path}:"
                f"{finding.line} [{finding.error_id}] {finding.message}"
            )
    else:
        print(f"  Cppcheck toolchain detected: {cppcheck.is_available()} (use --cppcheck for deep analysis)")

    print("\n" + "=" * 70)
    cppcheck_errors = [f for f in cppcheck_findings if f.severity == "error"]
    if critical or cppcheck_errors:
        print(
            f" AUDIT RESULT: FAIL -- {len(critical)} Critical AST violations, "
            f"{len(cppcheck_errors)} Cppcheck errors."
        )
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


def cmd_cppcheck(args) -> int:
    """Run Cppcheck across the complete firmware source tree."""
    cppcheck = CppcheckRunner(REPO_ROOT)
    if not cppcheck.is_available():
        print("Cppcheck is not installed or could not be located.")
        return 2

    findings = cppcheck.run()
    for finding in findings:
        print(
            f"[{finding.severity.upper()}] {finding.file_path}:"
            f"{finding.line} [{finding.error_id}] {finding.message}"
        )

    errors = sum(finding.severity == "error" for finding in findings)
    print(f"Cppcheck findings: {len(findings)} total, {errors} errors.")
    return 1 if errors else 0


def cmd_report(args) -> int:
    """Generates complete Markdown or JSON report."""
    bus_model = HardwareBusModel()
    bus_metrics = bus_model.calculate_all()
    
    linter = FirmwareAstLinter(FIRMWARE_DIR)
    ast_findings = linter.lint_all()
    file_stats = linter.get_repo_line_stats()
    
    elf_analyzer = ElfAnalyzer(REPO_ROOT)
    elf_res = elf_analyzer.analyze()
    
    bench_suite = HostBenchmarkSuite()
    bench_metrics = bench_suite.run_all()
    
    cppcheck = CppcheckRunner(REPO_ROOT)
    cppcheck_findings = cppcheck.run() if (hasattr(args, "cppcheck") and args.cppcheck and cppcheck.is_available()) else []
    
    if hasattr(args, "json") and args.json:
        content = ReportGenerator.generate_json(
            elf_res, bus_metrics, ast_findings, cppcheck_findings, bench_metrics, file_stats
        )
    else:
        content = ReportGenerator.generate_markdown(
            elf_res, bus_metrics, ast_findings, cppcheck_findings, bench_metrics, file_stats
        )
        
    if hasattr(args, "output") and args.output:
        out_path = Path(args.output)
        out_path.write_text(content, encoding="utf-8")
        print(f"Report saved to: {out_path}")
    else:
        print(content)
        
    return 0


def cmd_index(args) -> int:
    """Compiles the machine-readable AI knowledge base and graph."""
    from scripts.generate_ai_knowledge_base import build_knowledge_base
    build_knowledge_base()
    return 0


def cmd_route(args) -> int:
    """Zero-shot task intent routing for AI agents."""
    import json
    query = " ".join(args.query).lower()
    tree_path = REPO_ROOT / "AGENT_DECISION_TREE.json"
    if not tree_path.exists():
        from scripts.generate_ai_knowledge_base import build_knowledge_base
        build_knowledge_base()
        
    try:
        tree = json.loads(tree_path.read_text(encoding="utf-8"))
        tasks = tree.get("tasks", {})
    except Exception as e:
        print(f"Error loading decision tree: {e}")
        return 1

    best_task = None
    best_score = 0
    
    for task_name, info in tasks.items():
        score = 0
        keywords = info.get("intent_keywords", [])
        for kw in keywords:
            if kw.lower() in query:
                score += 2
        for word in query.split():
            if len(word) > 2 and word in task_name.lower():
                score += 1
            if len(word) > 2 and any(word in kw.lower() for kw in keywords):
                score += 1
        if score > best_score:
            best_score = score
            best_task = (task_name, info)

    print("=" * 70)
    print(" BMO AI ZERO-SHOT INTENT ROUTER")
    print("=" * 70)
    print(f"Query: \"{query}\"\n")
    
    if not best_task or best_score == 0:
        print("No exact intent matched. Defaulting to general session workflow:")
        print("  * Mandatory Rule: .agents/rules/31_quick_start_primer.md")
        print("  * Verification Command: python scripts/validate_repo.py")
        return 0

    t_name, t_info = best_task
    print(f"🎯 Matched Task Intent: [{t_name}]")
    print("-" * 70)
    print("📚 MANDATORY RULES TO READ FIRST:")
    for r in t_info.get("mandatory_rules", []):
        print(f"   * .agents/rules/{r}")
    print("\n📂 PRIMARY TARGET FILES TO EDIT:")
    for f in t_info.get("primary_files", []):
        print(f"   * {f}")
    print("\n⚠️ HARD INVARIANTS & GUARDRAILS:")
    for g in t_info.get("guardrails", []):
        print(f"   * {g}")
    print(f"\n🧪 MANDATORY VERIFICATION COMMAND:")
    print(f"   {t_info.get('verification_command', 'python scripts/validate_repo.py')}")
    print("=" * 70)
    return 0


def cmd_lookup(args) -> int:
    """Instantly looks up any symbol, pin, or file in the AI Knowledge Graph."""
    import json
    target = args.symbol.strip()
    kg_path = REPO_ROOT / "AGENT_KNOWLEDGE_GRAPH.json"
    if not kg_path.exists():
        from scripts.generate_ai_knowledge_base import build_knowledge_base
        build_knowledge_base()
        
    try:
        kg = json.loads(kg_path.read_text(encoding="utf-8"))
    except Exception as e:
        print(f"Error loading knowledge graph: {e}")
        return 1

    symbols = kg.get("symbol_registry", {})
    pins = kg.get("hardware_pins", {})
    platforms = kg.get("platforms_matrix", {})

    print("=" * 70)
    print(f" BMO AI SYMBOL & PIN GRAPH LOOKUP: \"{target}\"")
    print("=" * 70)

    found = False

    # 1. Check Hardware Pins
    for group, p_map in pins.items():
        for p_name, p_val in p_map.items():
            if target.lower() in p_name.lower() or target.lower() == str(p_val):
                print(f"\n🔌 HARDWARE PIN MATCH ({group}):")
                print(f"   * Net Name: {p_name} -> GPIO {p_val}")
                found = True

    # 2. Check Platforms
    for p_id, p_info in platforms.items():
        if target.lower() in p_id or target.lower() in p_info["name"].lower() or any(target.lower() in ext for ext in p_info["ext"]):
            print(f"\n🎮 CONSOLE PLATFORM MATCH:")
            print(f"   * Name: {p_info['name']} (ID: {p_id})")
            print(f"   * Core Engine: {p_info['core']} ({p_info['status']})")
            print(f"   * Viewport Resolution: {p_info['res']} @ {p_info['fps']} FPS")
            print(f"   * Supported Extensions: {', '.join(p_info['ext'])}")
            found = True

    # 3. Check Symbols
    matching_syms = {k: v for k, v in symbols.items() if target.lower() in k.lower()}
    if matching_syms:
        print(f"\n🧩 SYMBOL DEFINITIONS FOUND ({len(matching_syms)} matches):")
        for s_name, s_meta in list(matching_syms.items())[:10]:
            iram_badge = " [⚡ IRAM_ATTR]" if s_meta.get("is_iram") else ""
            print(f"   * {s_name}{iram_badge}")
            print(f"     Location: {s_meta['file']}:{s_meta['line']}")
            print(f"     Signature: {s_meta['signature']}")
        found = True

    if not found:
        print(f"No direct symbol or pin matching \"{target}\" was found.")
        print("Tip: Run `python -m tools.guardian index` to refresh the index.")
    print("=" * 70)
    return 0


def cmd_budget(args) -> int:
    """Visualizes Flash partition budget, baked ROM sizes, and headroom."""
    print("=" * 70)
    print(" BMO GUARDIAN -- Flash & Memory Budget Explorer")
    print("=" * 70)
    
    total_flash = 16 * 1024 * 1024
    app0_size = 8 * 1024 * 1024
    
    roms_dir = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "assets" / "roms"
    baked_roms = []
    if roms_dir.exists():
        for f in roms_dir.glob("*.h"):
            # Count lines with hex bytes (16 bytes per line standard)
            with open(f, "r", encoding="utf-8", errors="ignore") as hf:
                lines = [line for line in hf if line.strip().startswith("0x")]
            bin_size = len(lines) * 16
            baked_roms.append((f.name, bin_size))
            
    total_baked_bytes = sum(s for _, s in baked_roms)
    
    print(f"\n📦 Flash Allocation Overview (16MB Octal SPI Flash):")
    print(f"  * Physical Flash Chip: {total_flash / (1024*1024):.1f} MB (16,777,216 bytes)")
    print(f"  * Firmware App Partition (app0): {app0_size / (1024*1024):.1f} MB (8,388,608 bytes)")
    print(f"  * Baked Binary ROMs In Flash ({len(baked_roms)} headers): {total_baked_bytes / (1024*1024):.2f} MB ({total_baked_bytes:,} bytes)")
    
    for name, size in baked_roms:
        print(f"    - {name:30s} : {size / 1024:8.1f} KB ({size / (1024*1024):.2f} MB)")
        
    headroom = app0_size - total_baked_bytes
    print(f"\n⚡ Flash App Headroom: ~{headroom / (1024*1024):.2f} MB ({headroom:,} bytes) available for code & assets")
    print("=" * 70)
    return 0


def cmd_qol(args) -> int:
    """Displays the system-wide Quality of Life (QoL) and UI/UX roadmap ledger."""
    qol_path = REPO_ROOT / ".agents" / "rules" / "42_quality_of_life_and_roadmap_ledger.md"
    if not qol_path.exists():
        print("Rule 42 (QoL Ledger) not found.")
        return 1
        
    print("=" * 70)
    print(" BMO QUALITY OF LIFE (QoL) & ROADMAP LEDGER (Rule 42)")
    print("=" * 70)
    
    with open(qol_path, "r", encoding="utf-8") as f:
        content = f.read()
        
    for line in content.splitlines():
        if line.startswith("## ") or line.startswith("### "):
            print(f"\n{line.strip('# ')}")
        elif line.startswith("| **"):
            parts = [p.strip() for p in line.split("|")[1:-1]]
            if len(parts) >= 3:
                name, status, desc = parts[0], parts[1], parts[2]
                status_badge = "✅" if "IMPLEMENTED" in status else "🔄" if "PROGRESS" in status else "⏳" if "ROADMAP" in status else "🛑"
                print(f"  {status_badge} {name:32s} [{status}] : {desc}")
                
    print("\n" + "=" * 70)
    return 0


def cmd_doctor(args) -> int:
    """One-command environment, hardware pin sync, and guardrail health check for AI agents."""
    import json
    import shutil
    from scripts.generate_ai_knowledge_base import parse_config_h

    print("=" * 70)
    print(" BMO GUARDIAN DOCTOR -- AI Harness & Hardware Safety Preflight")
    print("=" * 70)

    issues = 0
    cfg_path = FIRMWARE_DIR / "src" / "core" / "config.h"
    cfg = parse_config_h(cfg_path)

    # 1. Check Hard Stops in config.h
    print("\n[1/4] Verifying Hardware Hard Stops in src/core/config.h...")
    if cfg.get("FEATURE_BATTERY_MONITOR", 0) != 0:
        print("  [FAIL] FEATURE_BATTERY_MONITOR != 0 (Floating GPIO1 bootloop hazard!)")
        issues += 1
    else:
        print("  [PASS] FEATURE_BATTERY_MONITOR == 0 (Safe)")

    if cfg.get("FEATURE_AUDIO", 0) != 0:
        print("  [FAIL] FEATURE_AUDIO != 0 (Floating I2S DMA hazard!)")
        issues += 1
    else:
        print("  [PASS] FEATURE_AUDIO == 0 (Safe)")

    # 2. Check Knowledge Graph Sync against config.h
    print("\n[2/4] Verifying AI Knowledge Graph Synchronization...")
    kg_path = REPO_ROOT / "AGENT_KNOWLEDGE_GRAPH.json"
    if not kg_path.exists():
        print("  [WARN] AGENT_KNOWLEDGE_GRAPH.json missing — regenerating...")
        from scripts.generate_ai_knowledge_base import build_knowledge_base
        build_knowledge_base()
    try:
        kg = json.loads(kg_path.read_text(encoding="utf-8"))
        btn_pins = kg.get("hardware_pins", {}).get("buttons", {})
        if btn_pins.get("btn_start") != cfg.get("BTN_START", 18) or btn_pins.get("btn_a") != cfg.get("BTN_A", 16):
            print("  [FAIL] AGENT_KNOWLEDGE_GRAPH.json pin map out of sync with config.h!")
            issues += 1
        else:
            print(f"  [PASS] Pin map synchronized with config.h (TFT_CS={cfg.get('TFT_CS')}, SD_CS={cfg.get('SD_CS')}, BTN_A={cfg.get('BTN_A')}, BTN_START={cfg.get('BTN_START')})")
    except Exception as exc:
        print(f"  [FAIL] Could not parse AGENT_KNOWLEDGE_GRAPH.json: {exc}")
        issues += 1

    # 3. Check AST Linter Criticals
    print("\n[3/4] Running Embedded AST Linter Critical Check...")
    linter = FirmwareAstLinter(FIRMWARE_DIR)
    findings = linter.lint_all()
    criticals = [f for f in findings if f.severity == "CRITICAL"]
    if criticals:
        print(f"  [FAIL] {len(criticals)} Critical AST violations found:")
        for c in criticals:
            print(f"    - [{c.rule_id}] {c.file_path.name}:{c.line_number}: {c.message}")
        issues += len(criticals)
    else:
        print(f"  [PASS] 0 Critical AST violations ({len(findings)} advisory/warnings)")

    # 4. Check Toolchain Availability
    print("\n[4/4] Checking Local Developer & Verification Toolchains...")
    arduino_cli = REPO_ROOT / "arduino-cli.exe"
    zig_bin = REPO_ROOT / "tools" / "zig" / "zig.exe"
    if not zig_bin.exists():
        zig_candidates = list((REPO_ROOT / "tools" / "zig").glob("**/zig.exe")) if (REPO_ROOT / "tools" / "zig").exists() else []
        if zig_candidates:
            zig_bin = zig_candidates[0]
    cppcheck = CppcheckRunner(REPO_ROOT)
    print(f"  * arduino-cli : {'READY' if arduino_cli.exists() or shutil.which('arduino-cli') else 'NOT FOUND'}")
    print(f"  * zig cc      : {'READY' if zig_bin.exists() or shutil.which('zig') else 'NOT FOUND'}")
    print(f"  * cppcheck    : {'READY' if cppcheck.is_available() else 'NOT FOUND'}")

    print("\n" + "=" * 70)
    if issues == 0:
        print(" DOCTOR RESULT: HEALTHY (0 blocking issues). Ready for agent work.")
        return 0
    print(f" DOCTOR RESULT: {issues} ISSUE(S) DETECTED. Fix before proceeding.")
    return 1


def cmd_explain(args) -> int:
    """Instant context card for any file or symbol (for weak/small-context AI models)."""
    import json
    target = args.target.strip().replace("\\", "/")
    kg_path = REPO_ROOT / "AGENT_KNOWLEDGE_GRAPH.json"
    if not kg_path.exists():
        from scripts.generate_ai_knowledge_base import build_knowledge_base
        build_knowledge_base()
    kg = json.loads(kg_path.read_text(encoding="utf-8"))
    files_index = kg.get("files_index", {})
    symbols = kg.get("symbol_registry", {})

    print("=" * 70)
    print(f" BMO AI CONTEXT EXPLAINER: \"{target}\"")
    print("=" * 70)

    matched_files = [k for k in files_index if target.lower() in k.lower()]
    if matched_files:
        for rel_p in matched_files[:5]:
            info = files_index[rel_p]
            stem = Path(rel_p).name
            reverse_deps = [
                other_p for other_p, other_info in files_index.items()
                if any(stem in inc for inc in other_info.get("includes", []))
            ]
            print(f"\n📄 FILE: {rel_p} [{info.get('category')}]")
            print(f"   * Metrics  : {info.get('total_lines')} lines ({info.get('sloc')} SLOC, {info.get('comments')} comments, {info.get('density_percent')}% density)")
            print(f"   * Includes : {', '.join(info.get('includes', [])[:8]) or 'None'}")
            print(f"   * Pins/Cfg : {', '.join(info.get('pins_accessed', [])) or 'None'}")
            print(f"   * Symbols  : {', '.join(info.get('symbols', [])[:12]) or 'None'}")
            print(f"   * Used By  : {', '.join(reverse_deps[:8]) or 'Entrypoint / Leaf'}")
        print("=" * 70)
        return 0

    # Fallback to symbol lookup
    args.symbol = target
    return cmd_lookup(args)


def cmd_verify(args) -> int:
    """Fast all-in-one verification gate (<2s): AST lint + unit tests + Guardian tests."""
    import unittest

    print("=" * 70)
    print(" BMO GUARDIAN FAST VERIFY -- AST Lint + Unit Test Gate")
    print("=" * 70)

    linter = FirmwareAstLinter(FIRMWARE_DIR)
    findings = linter.lint_all()
    criticals = [f for f in findings if f.severity == "CRITICAL"]
    if criticals:
        print(f"[FAIL] {len(criticals)} Critical AST violations found:")
        for c in criticals:
            print(f"  * [{c.rule_id}] {c.file_path.name}:{c.line_number} -- {c.message}")
        return 1
    print(f"[PASS] AST Linter: 0 Critical violations ({len(findings)} warnings).")

    suite = unittest.TestSuite()
    suite.addTests(unittest.TestLoader().discover(str(REPO_ROOT / "tests"), pattern="test_*.py"))
    suite.addTests(unittest.TestLoader().discover(str(REPO_ROOT / "tools" / "guardian" / "tests"), pattern="test_*.py"))

    runner = unittest.TextTestRunner(verbosity=1)
    result = runner.run(suite)
    if not result.wasSuccessful():
        print("[FAIL] Unit test suite failed.")
        return 1

    print("[PASS] All repository and Guardian unit tests succeeded.")
    return 0


def cmd_benchmark_ai(args) -> int:
    """Evaluates AI-First Engineering Workflow metrics (routing accuracy, symbol grounding, pin parity, core coverage)."""
    import json
    from scripts.generate_ai_knowledge_base import parse_config_h

    print("=" * 70)
    print(" BMO GUARDIAN AI-FIRST WORKFLOW BENCHMARK")
    print("=" * 70)

    kg_path = REPO_ROOT / "AGENT_KNOWLEDGE_GRAPH.json"
    dt_path = REPO_ROOT / "AGENT_DECISION_TREE.json"
    manifest_path = REPO_ROOT / "AGENT_MANIFEST.json"
    if not (kg_path.exists() and dt_path.exists() and manifest_path.exists()):
        from scripts.generate_ai_knowledge_base import build_knowledge_base
        build_knowledge_base()

    kg = json.loads(kg_path.read_text(encoding="utf-8"))
    dt = json.loads(dt_path.read_text(encoding="utf-8")).get("tasks", {})
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    # 1. Intent Routing Accuracy across all 10 decision tree pathways
    benchmark_queries = [
        ("add new emulator console core", "add_or_modify_emulator_core"),
        ("fix st7789 display spi palette", "modify_display_rendering_or_spi"),
        ("update bmo mascot face expression", "bmo_mascot_face"),
        ("check gpio button pinout wiring", "modify_gpio_or_pin"),
        ("bake rom header into flash app0", "bake_rom_into_flash"),
        ("benchmark performance audit latency fps", "benchmark_and_performance_audit"),
        ("fix save state crc32 slot restore", "save_states_and_persistence"),
        ("update bmo_index box art bmp catalog", "binary_rom_index_and_box_art"),
        ("session handoff changelog commit log", "session_handoff_and_logging"),
        ("critical pushback suggest opinion idea", "critical_pushback_and_fact_checking"),
    ]
    routed_hits = 0
    for query_str, expected_intent in benchmark_queries:
        q_words = set(query_str.lower().split())
        best_id = None
        best_score = -1
        for intent_id, pdata in dt.items():
            triggers = [t.lower() for t in pdata.get("intent_keywords", [])]
            score = sum(2 if w in triggers else sum(1 for t in triggers if w in t) for w in q_words)
            if score > best_score:
                best_score = score
                best_id = intent_id
        if best_id == expected_intent and best_score > 0:
            routed_hits += 1

    routing_pct = (routed_hits / len(benchmark_queries)) * 100.0
    print(f"  [1/4] Task Intent Routing Coverage : {routed_hits}/{len(benchmark_queries)} ({routing_pct:.1f}%)")

    # 2. Hardware Pin Parity (config.h vs AGENT_KNOWLEDGE_GRAPH.json)
    cfg = parse_config_h(FIRMWARE_DIR / "src" / "core" / "config.h")
    hp = kg.get("hardware_pins", {})
    pin_checks = [
        (hp.get("display", {}).get("tft_cs"), cfg.get("TFT_CS")),
        (hp.get("display", {}).get("tft_dc"), cfg.get("TFT_DC")),
        (hp.get("display", {}).get("tft_rst"), cfg.get("TFT_RST")),
        (hp.get("sd_card", {}).get("sd_cs"), cfg.get("SD_CS")),
        (hp.get("sd_card", {}).get("sd_miso"), cfg.get("SD_MISO")),
        (hp.get("buttons", {}).get("btn_a"), cfg.get("BTN_A")),
        (hp.get("buttons", {}).get("btn_b"), cfg.get("BTN_B")),
        (hp.get("buttons", {}).get("btn_start"), cfg.get("BTN_START")),
        (hp.get("buttons", {}).get("btn_select"), cfg.get("BTN_SELECT")),
    ]
    pin_hits = sum(1 for a, b in pin_checks if a == b and a is not None)
    pin_pct = (pin_hits / len(pin_checks)) * 100.0
    print(f"  [2/4] Hardware Pin Ground-Truth    : {pin_hits}/{len(pin_checks)} ({pin_pct:.1f}%)")

    # 3. Core Symbol Registry Coverage
    symbols = kg.get("symbol_registry", {})
    required_symbols = [
        "begin", "update", "runFrame", "destroy", "saveState", "loadState",
        "saveBatteryRam", "loadBatteryRam", "saveScreenshotBmp", "drawConsoleBezel",
        "scanDirectory", "recordRecentLaunch", "setSwapAB", "setTurboEnabled",
    ]
    sym_hits = sum(
        1 for s in required_symbols
        if s in symbols or any(k.endswith(f"::{s}") for k in symbols)
    )
    sym_pct = (sym_hits / len(required_symbols)) * 100.0
    print(f"  [3/4] Symbol Registry Grounding    : {sym_hits}/{len(required_symbols)} ({sym_pct:.1f}%)")

    # 4. Emulator Production Core Ratio
    platforms = manifest.get("emulators", [])
    prod_cores = [p for p in platforms if p.get("engine_status") == "production"]
    core_pct = (len(prod_cores) / max(1, len(platforms))) * 100.0
    print(f"  [4/4] Production Emulator Engines  : {len(prod_cores)}/{len(platforms)} ({core_pct:.1f}%)")
    print("=" * 70)

    if routing_pct == 100.0 and pin_pct == 100.0 and sym_pct == 100.0 and core_pct == 100.0:
        print(" AI WORKFLOW BENCHMARK: 100.0% PASS across all 4 dimensions.")
        return 0
    return 1


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
    subparsers.add_parser("cppcheck", help="Run Cppcheck across all firmware sources")

    # report
    p_rep = subparsers.add_parser("report", help="Generate full performance & ground-truth report")
    p_rep.add_argument("--output", "-o", help="Output file path (e.g. report.md)")
    p_rep.add_argument("--json", action="store_true", help="Output JSON instead of Markdown")

    # index
    subparsers.add_parser("index", help="Compile machine-readable AI knowledge graph and decision tree")

    # route
    p_route = subparsers.add_parser("route", help="Zero-shot intent router for AI agents (e.g. `route \"add new rom\"`)")
    p_route.add_argument("query", nargs="+", help="English task prompt or query keywords")

    # lookup
    p_lookup = subparsers.add_parser("lookup", help="Look up symbol, function, or pin definition in Knowledge Graph")
    p_lookup.add_argument("symbol", help="Symbol, function name, pin name, or console extension to inspect")

    # budget
    subparsers.add_parser("budget", help="Explore Flash memory partition allocation and baked ROM budget")

    # qol
    subparsers.add_parser("qol", help="View the system-wide Quality of Life (QoL) and UI/UX roadmap ledger")

    # doctor
    subparsers.add_parser("doctor", help="One-command health, hardware pin sync, and guardrail preflight check")

    # explain
    p_explain = subparsers.add_parser("explain", help="Instant context card for any file or symbol (dependencies, pins, symbols)")
    p_explain.add_argument("target", help="File path substring or symbol name to explain")

    # verify
    subparsers.add_parser("verify", help="Fast all-in-one verification gate (AST lint + all unit tests)")

    # benchmark-ai
    subparsers.add_parser("benchmark-ai", help="Run quantitative AI-first workflow grounding & routing benchmark")

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        return 0

    commands = {
        "audit": cmd_audit,
        "bus-calc": cmd_bus_calc,
        "profile-elf": cmd_profile_elf,
        "bench-host": cmd_bench_host,
        "cppcheck": cmd_cppcheck,
        "report": cmd_report,
        "index": cmd_index,
        "route": cmd_route,
        "lookup": cmd_lookup,
        "budget": cmd_budget,
        "qol": cmd_qol,
        "doctor": cmd_doctor,
        "explain": cmd_explain,
        "verify": cmd_verify,
        "benchmark-ai": cmd_benchmark_ai,
    }

    handler = commands.get(args.command)
    if handler:
        return handler(args)
    return 1


if __name__ == "__main__":
    sys.exit(main())
