# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Argument parser and dispatch entrypoint for tasks.py."""

from __future__ import annotations

import argparse

from tools.tasks import BUILD_PROFILES
from tools.tasks.bench import cmd_bench, cmd_doctor, cmd_flash, cmd_monitor, cmd_sim
from tools.tasks.build import cmd_build, cmd_format, cmd_setup, cmd_size
from tools.tasks.checks import cmd_check
from tools.tasks.tests import cmd_test


def build_parser() -> argparse.ArgumentParser:
    """Build the unified tasks.py CLI argument parser."""
    parser = argparse.ArgumentParser(prog="tasks.py", description="Unified BMO Handheld developer task runner.")
    sub = parser.add_subparsers(dest="command", required=True)

    p_setup = sub.add_parser("setup", help="Install hooks, verify Python deps, and sync pinned Arduino core/libs.")
    p_setup.add_argument("--install-arduino-cli", action="store_true", help="Download SHA-256 verified arduino-cli into .tools/.")
    p_setup.add_argument("--no-hooks", action="store_true", help="Skip configuring git core.hooksPath.")
    p_setup.add_argument("--no-pip", action="store_true", help="Skip checking Python package imports.")
    p_setup.set_defaults(func=cmd_setup)

    p_check = sub.add_parser("check", help="Fast verification (< 1 min, no firmware compile).")
    p_check.add_argument("--fast", action="store_true", help="Ultra-fast pre-commit check (< 15 s, skips compiler/CTest tests).")
    p_check.add_argument("--cppcheck", action="store_true", help="Include cppcheck static analysis.")
    p_check.add_argument("--no-tests", action="store_true", help="Skip Python unit tests.")
    p_check.add_argument("--with-all-tests", action="store_true", help="Include slower WASI execution and CAD mesh tests.")
    p_check.add_argument("--sanitizers", action="store_true", help="Run host C++17 CTest suite with ASan+UBSan.")
    p_check.add_argument("--strict-lint", action="store_true", help="Fail if ruff is unavailable.")
    p_check.set_defaults(func=cmd_check)

    sub.add_parser("doctor", help="Inspect Python, git hooks, arduino-cli, and host C/C++ toolchain status.").set_defaults(func=cmd_doctor)

    p_test = sub.add_parser("test", help="Run full unittest suite and repository validator.")
    p_test.add_argument("--strict", action="store_true", help="Require target compile inside validate_repo.py.")
    p_test.add_argument("--sanitizers", action="store_true", help="Build and run host C++17 CTest suite with ASan+UBSan.")
    p_test.set_defaults(func=cmd_test)

    p_build = sub.add_parser("build", help="Compile firmware profile(s) with sketch.yaml and strict warning reporting.")
    p_build.add_argument("--profile", choices=[*BUILD_PROFILES.keys(), "all"], default="default")
    p_build.add_argument("--warnings", choices=["strict", "all", "none"], default="strict")
    p_build.add_argument("--clean", action="store_true")
    p_build.add_argument("--export-binaries", action="store_true")
    p_build.add_argument("--compile-commands", action="store_true", help="Generate build/compile_commands.json only.")
    p_build.add_argument("--build-dir", default="")
    p_build.add_argument("--fail-on-warnings", action="store_true")
    p_build.set_defaults(func=cmd_build)

    p_fmt = sub.add_parser("format", help="Check or apply clang-format on first-party C/C++ sources only.")
    p_fmt.add_argument("--check", action="store_true", help="Report formatting diffs without modifying files.")
    p_fmt.add_argument("files", nargs="*", help="Optional specific first-party files to format/check.")
    p_fmt.set_defaults(func=cmd_format)

    p_size = sub.add_parser("size", help="Report compiled Flash/DRAM/IRAM and PSRAM budget.")
    p_size.add_argument("--elf", default="", help="Path to compiled .elf (auto-discovered if omitted).")
    p_size.add_argument("--markdown-summary", default="", help="Append Markdown report to file.")
    p_size.add_argument("--check-budgets", action="store_true", help="Fail if Flash > 85%% app0 or DRAM > 85%%.")
    p_size.set_defaults(func=cmd_size)

    p_flash = sub.add_parser("flash", help="Safety-check and flash firmware to connected ESP32-S3.")
    p_flash.add_argument("--port", default="", help="Serial port (e.g., COM3 or /dev/ttyACM0).")
    p_flash.set_defaults(func=cmd_flash)

    p_mon = sub.add_parser("monitor", help="Open serial monitor at 115200 baud.")
    p_mon.add_argument("--port", default="", help="Serial port (e.g., COM3 or /dev/ttyACM0).")
    p_mon.add_argument("--baud", type=int, default=115200)
    p_mon.set_defaults(func=cmd_monitor)

    p_sim = sub.add_parser("sim", help="Launch local browser simulator.")
    p_sim.add_argument("--no-open", action="store_true")
    p_sim.set_defaults(func=cmd_sim)

    p_bench = sub.add_parser("bench", help="Run host firmware, simulator, and script benchmarks.")
    p_bench.add_argument("--save-baseline", action="store_true", help="Record benchmarks/baseline_before.json.")
    p_bench.add_argument("--check", action="store_true", help="Fail if any budget or baseline regression occurs.")
    p_bench.add_argument("--with-wasm-build", action="store_true", help="Include full WASM build timing.")
    p_bench.set_defaults(func=cmd_bench)
    return parser


def main(argv: list[str] | None = None) -> int:
    """Parse CLI arguments and invoke the selected task handler."""
    parsed = build_parser().parse_args(argv)
    return int(parsed.func(parsed))
