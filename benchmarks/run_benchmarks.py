# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Run firmware host, simulator JS/WASM, and Python script benchmarks."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[1]
BENCH_DIR = REPO_ROOT / "benchmarks"
BASELINE_PATH = BENCH_DIR / "baseline_before.json"
BUDGETS_PATH = BENCH_DIR / "budgets.json"
LATEST_PATH = BENCH_DIR / "latest_results.json"


def _find_bench_binary() -> Path:
    build_dir = REPO_ROOT / "build" / "host-ctest"
    candidates = [
        build_dir / "Release" / "bmo_bench_host.exe",
        build_dir / "bmo_bench_host.exe",
        build_dir / "Release" / "bmo_bench_host",
        build_dir / "bmo_bench_host",
    ]
    for c in candidates:
        if c.is_file():
            return c
    cmake_bin = shutil.which("cmake")
    if not cmake_bin:
        raise FileNotFoundError("cmake not found to build bmo_bench_host")
    if not (build_dir / "CMakeCache.txt").is_file():
        subprocess.run([cmake_bin, "-S", str(REPO_ROOT), "-B", str(build_dir)], cwd=str(REPO_ROOT), check=True)
    subprocess.run(
        [cmake_bin, "--build", str(build_dir), "--config", "Release", "--target", "bmo_bench_host"],
        cwd=str(REPO_ROOT),
        check=True,
    )
    for c in candidates:
        if c.is_file():
            return c
    raise FileNotFoundError("bmo_bench_host binary missing after build")


def run_all_benchmarks(*, include_wasm_build: bool = False) -> dict[str, Any]:
    if str(REPO_ROOT) not in sys.path:
        sys.path.insert(0, str(REPO_ROOT))
    wasm_path = REPO_ROOT / "simulator" / "bmo_playtest_cores.wasm"
    if not include_wasm_build and not wasm_path.is_file():
        from scripts.build_wasm_cores import build_wasm_module

        build_wasm_module(output_path=wasm_path)

    bench_bin = _find_bench_binary()
    fw_proc = subprocess.run([str(bench_bin)], cwd=str(REPO_ROOT), capture_output=True, text=True, check=True)
    fw_metrics: dict[str, Any] = json.loads(fw_proc.stdout)

    node_bin = shutil.which("node")
    if not node_bin:
        raise FileNotFoundError("node not found in PATH for bench_simulator.mjs")
    sim_proc = subprocess.run(
        [node_bin, "--expose-gc", str(BENCH_DIR / "bench_simulator.mjs")],
        cwd=str(REPO_ROOT),
        capture_output=True,
        text=True,
        check=True,
    )
    sim_metrics: dict[str, Any] = json.loads(sim_proc.stdout)

    from benchmarks.bench_scripts import run_script_benchmarks

    script_metrics = run_script_benchmarks()

    if include_wasm_build:
        import time

        from scripts.build_wasm_cores import build_wasm_module

        t0 = time.perf_counter()
        build_wasm_module(output_path=None)
        script_metrics["wasm_full_build_ms"] = round((time.perf_counter() - t0) * 1000.0, 2)

    return {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "evidence_level": "HOST_TESTED",
        "os": os.name,
        "firmware_host": fw_metrics,
        "simulator": sim_metrics,
        "scripts": script_metrics,
    }


def _flatten_numeric(prefix: str, obj: dict[str, Any]) -> dict[str, float]:
    out: dict[str, float] = {}
    for key, val in obj.items():
        if key == "sink":
            continue
        full = f"{prefix}.{key}" if prefix else key
        if isinstance(val, (int, float)) and not isinstance(val, bool):
            out[full] = float(val)
    return out


def _check_baseline_regressions(
    current: dict[str, Any], flat_cur: dict[str, float], max_ratio: float
) -> list[str]:
    if not BASELINE_PATH.is_file():
        return []
    failures: list[str] = []
    baseline: dict[str, Any] = json.loads(BASELINE_PATH.read_text(encoding="utf-8"))
    same_host_os = current.get("os") == baseline.get("os") and os.environ.get("CI", "").lower() != "true"
    flat_base: dict[str, float] = {}
    for section in ("firmware_host", "simulator", "scripts"):
        if isinstance(baseline.get(section), dict):
            flat_base.update(_flatten_numeric(section, baseline[section]))
    for key, base_val in flat_base.items():
        if key not in flat_cur or base_val <= 0.0:
            continue
        is_deterministic_size = key.endswith(("imagedata_bytes", "script_bytes", "_calls", "_count"))
        if is_deterministic_size:
            limit = base_val
        elif not same_host_os:
            continue
        elif key.endswith("heap_delta_bytes"):
            limit = max(base_val * 2.0, base_val + 524288.0)
        elif key.endswith("_us") and base_val < 250.0:
            limit = max(base_val * max_ratio, base_val * 2.5)
        elif key.endswith("_ms") and base_val < 50.0:
            limit = max(base_val * max_ratio, 65.0)
        else:
            limit = base_val * max_ratio
        if flat_cur[key] > limit:
            failures.append(
                f"Regression vs baseline: {key}={flat_cur[key]:.2f} > limit {limit:.2f} (baseline {base_val:.2f})"
            )
    return failures


def check_against_budgets(current: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    if not BUDGETS_PATH.is_file():
        return [f"Missing budgets file: {BUDGETS_PATH}"]
    budgets: dict[str, Any] = json.loads(BUDGETS_PATH.read_text(encoding="utf-8"))
    caps: dict[str, float] = budgets.get("absolute_caps", {})

    flat_cur: dict[str, float] = {}
    for section in ("firmware_host", "simulator", "scripts"):
        if isinstance(current.get(section), dict):
            flat_cur.update(_flatten_numeric(section, current[section]))

    for key, cap in caps.items():
        if key in flat_cur and flat_cur[key] > float(cap):
            failures.append(f"Budget cap exceeded: {key}={flat_cur[key]:.2f} > {float(cap):.2f}")

    max_ratio = float(budgets.get("max_regression_ratio", 1.25))
    failures.extend(_check_baseline_regressions(current, flat_cur, max_ratio))
    return failures


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--save-baseline", action="store_true", help="Write benchmarks/baseline_before.json")
    parser.add_argument("--check", action="store_true", help="Verify budgets and baseline regressions")
    parser.add_argument("--with-wasm-build", action="store_true", help="Also time full WASM core build")
    parser.add_argument("--output", type=Path, default=LATEST_PATH, help="Write JSON results path")
    args = parser.parse_args(argv)

    results = run_all_benchmarks(include_wasm_build=args.with_wasm_build)
    text = json.dumps(results, indent=2) + "\n"
    if args.save_baseline:
        BASELINE_PATH.write_text(text, encoding="utf-8")
        print(f"Saved BEFORE baseline to {BASELINE_PATH.relative_to(REPO_ROOT)}")
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    print(text)

    if args.check:
        failures = check_against_budgets(results)
        if failures:
            for item in failures:
                print(f"FAIL: {item}", file=sys.stderr)
            return 1
        print("BENCHMARK BUDGET CHECK PASSED.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
