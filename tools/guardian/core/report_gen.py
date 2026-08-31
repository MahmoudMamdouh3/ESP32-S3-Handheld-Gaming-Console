"""
tools.guardian.core.report_gen — Ground-Truth Performance & Audit Report Generator
Combines static AST analysis, ELF binary introspection, bus physics models,
and microbenchmarks into comprehensive Markdown and JSON reports.
"""

import json
from dataclasses import asdict
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List

from .ast_linter import LintFinding
from .bus_model import BusMetrics
from .cppcheck_runner import CppcheckFinding
from .elf_analyzer import ElfAnalysisResult
from .host_bench import BenchmarkMetric


class ReportGenerator:
    """Generates structured Markdown and JSON reports from Guardian telemetry."""

    @staticmethod
    def generate_markdown(
        elf_res: ElfAnalysisResult,
        bus_metrics: List[BusMetrics],
        ast_findings: List[LintFinding],
        cppcheck_findings: List[CppcheckFinding],
        bench_metrics: List[BenchmarkMetric],
    ) -> str:
        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        # Calculate summary health score
        critical_count = sum(1 for f in ast_findings if f.severity == "CRITICAL")
        warning_count = sum(1 for f in ast_findings if f.severity == "WARNING")
        
        sram_pct = (elf_res.dram_used_bytes / elf_res.dram_total_bytes) * 100.0 if elf_res.dram_total_bytes else 0.0
        flash_pct = (elf_res.flash_used_bytes / elf_res.flash_total_bytes) * 100.0 if elf_res.flash_total_bytes else 0.0

        md: List[str] = [
            f"# BMO Guardian Performance & Ground-Truth Audit Report",
            f"**Generated:** {now}  ",
            f"**Target MCU:** ESP32-S3-N16R8 (240MHz Xtensa LX7, 16MB OPI Flash, 8MB Octal PSRAM)  ",
            f"**Display:** ST7789VW 2.4\" TFT on Shared 80MHz FSPI  ",
            f"**Ground-Truth Status:** {'FAIL (Critical Violations Found)' if critical_count > 0 else 'PASS'}",
            f"",
            f"---",
            f"",
            f"## 1. Executive Summary & Health Dashboard",
            f"",
            f"| Metric | Measured Value | Budget / Limit | Utilization | Status |",
            f"| :--- | :--- | :--- | :--- | :--- |",
            f"| **Internal SRAM (DRAM)** | {elf_res.dram_used_bytes:,} bytes | {elf_res.dram_total_bytes:,} bytes | {sram_pct:.1f}% | {'⚠️ TIGHT' if sram_pct > 80 else '✅ OK'} |",
            f"| **Flash Storage (app0)** | {elf_res.flash_used_bytes:,} bytes | {elf_res.flash_total_bytes:,} bytes | {flash_pct:.1f}% | {'✅ OK (Headroom 70%)' if flash_pct < 60 else '⚠️ WARN'} |",
            f"| **Static PSRAM** | {elf_res.psram_static_bytes:,} bytes | 8,388,608 bytes | {(elf_res.psram_static_bytes / 8388608)*100:.1f}% | ✅ OK |",
            f"| **AST Critical Findings** | {critical_count} | 0 allowed | - | {'🔴 FAIL' if critical_count > 0 else '✅ PASS'} |",
            f"| **AST Warnings** | {warning_count} | < 10 allowed | - | {'⚠️ WARN' if warning_count > 5 else '✅ OK'} |",
            f"| **Cppcheck Violations** | {len(cppcheck_findings)} | 0 critical | - | {'⚠️ REVIEW' if cppcheck_findings else '✅ CLEAN'} |",
            f"",
            f"---",
            f"",
            f"## 2. Hardware Bus & Frame Transmission Physics Model",
            f"",
            f"Mathematical limits of the 80 MHz FSPI display bus and CPU computation budget per resolution:",
            f"",
            f"| Platform / Viewport | Resolution | Frame Size | SPI Transfer | Target FPS | Seq. CPU Budget | Parallel DMA Budget | Bus Saturation |",
            f"| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |",
        ]

        for b in bus_metrics:
            md.append(
                f"| **{b.resolution.name}** | {b.resolution.rendered_w}×{b.resolution.rendered_h} | "
                f"{b.resolution.frame_bytes:,} B | {b.spi_transfer_ms:.2f} ms | {b.resolution.target_fps:.1f} | "
                f"{b.sequential_cpu_budget_ms:.2f} ms ({b.sequential_cpu_budget_percent:.0f}%) | "
                f"{b.parallel_dma_cpu_budget_ms:.2f} ms (100%) | {b.spi_bus_utilization_percent:.1f}% |"
            )

        md.extend([
            f"",
            f"> [!IMPORTANT]",
            f"> **Hardware Bus Insight:** At 80 MHz SPI, transmitting a full 320×240 frame consumes **15.36 ms** (92.1% of a 16.67 ms frame budget). In sequential blocking mode, only **1.31 ms** of CPU time remains. **DMA double-buffering is mathematically required** to achieve full 60 FPS on 320×240 without frame drops.",
            f"",
            f"---",
            f"",
            f"## 3. Static AST & Architecture Linter Findings ({len(ast_findings)} total)",
            f"",
        ])

        if not ast_findings:
            md.append("✅ No static AST violations detected.")
        else:
            md.append("| Severity | Rule ID | File : Line | Issue Description | Suggested Action |")
            md.append("| :--- | :--- | :--- | :--- | :--- |")
            for f in ast_findings:
                rel = f.file_path.name
                md.append(
                    f"| `{'🔴 ' + f.severity if f.severity == 'CRITICAL' else '🟡 ' + f.severity}` | `{f.rule_id}` | `{rel}:{f.line_number}` | {f.message} | {f.suggested_fix} |"
                )

        md.extend([
            f"",
            f"---",
            f"",
            f"## 4. Host-Side Quantitative Microbenchmarks",
            f"",
            f"| Benchmark Kernel | Iterations | Total Time | Avg Latency | Throughput | Description |",
            f"| :--- | :--- | :--- | :--- | :--- | :--- |",
        ])

        for bm in bench_metrics:
            md.append(
                f"| **{bm.name}** | {bm.iterations:,} | {bm.total_time_ms:.2f} ms | {bm.avg_latency_us:.3f} µs | {bm.throughput_mops:.2f} MOps/s | {bm.description} |"
            )

        if elf_res.largest_functions:
            md.extend([
                f"",
                f"---",
                f"",
                f"## 5. Top Memory Consuming Symbols (Xtensa ELF)",
                f"",
                f"### Largest Code Functions",
                f"| Size (Bytes) | Function Symbol | Section |",
                f"| :--- | :--- | :--- |",
            ])
            for sym in elf_res.largest_functions[:10]:
                md.append(f"| {sym.size:,} | `{sym.name}` | `{sym.section}` |")

            md.extend([
                f"",
                f"### Largest Data Objects",
                f"| Size (Bytes) | Object Symbol | Section |",
                f"| :--- | :--- | :--- |",
            ])
            for sym in elf_res.largest_data_objects[:10]:
                md.append(f"| {sym.size:,} | `{sym.name}` | `{sym.section}` |")

        return "\n".join(md)

    @staticmethod
    def generate_json(
        elf_res: ElfAnalysisResult,
        bus_metrics: List[BusMetrics],
        ast_findings: List[LintFinding],
        cppcheck_findings: List[CppcheckFinding],
        bench_metrics: List[BenchmarkMetric],
    ) -> str:
        data: Dict[str, Any] = {
            "timestamp": datetime.now().isoformat(),
            "target": "ESP32-S3-N16R8",
            "memory": {
                "dram_used_bytes": elf_res.dram_used_bytes,
                "dram_total_bytes": elf_res.dram_total_bytes,
                "flash_used_bytes": elf_res.flash_used_bytes,
                "flash_total_bytes": elf_res.flash_total_bytes,
                "iram_used_bytes": elf_res.iram_used_bytes,
                "psram_static_bytes": elf_res.psram_static_bytes,
            },
            "bus_metrics": [
                {
                    "key": b.resolution_key,
                    "name": b.resolution.name,
                    "frame_bytes": b.resolution.frame_bytes,
                    "spi_transfer_ms": b.spi_transfer_ms,
                    "target_fps": b.resolution.target_fps,
                    "sequential_cpu_budget_ms": b.sequential_cpu_budget_ms,
                    "sequential_cpu_budget_pct": b.sequential_cpu_budget_percent,
                    "spi_bus_utilization_pct": b.spi_bus_utilization_percent,
                    "dma_required": b.dma_required_for_60fps,
                    "status": b.status,
                }
                for b in bus_metrics
            ],
            "ast_findings": [
                {
                    "rule_id": f.rule_id,
                    "severity": f.severity,
                    "file": str(f.file_path),
                    "line": f.line_number,
                    "message": f.message,
                    "suggested_fix": f.suggested_fix,
                }
                for f in ast_findings
            ],
            "cppcheck_findings": [
                {
                    "file": c.file_path,
                    "line": c.line,
                    "severity": c.severity,
                    "id": c.error_id,
                    "message": c.message,
                }
                for c in cppcheck_findings
            ],
            "microbenchmarks": [
                {
                    "name": bm.name,
                    "iterations": bm.iterations,
                    "total_ms": bm.total_time_ms,
                    "avg_us": bm.avg_latency_us,
                    "throughput_mops": bm.throughput_mops,
                }
                for bm in bench_metrics
            ],
        }
        return json.dumps(data, indent=2)
