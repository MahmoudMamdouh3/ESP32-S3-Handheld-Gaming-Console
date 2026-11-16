# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Device upload/monitor, simulator launcher, benchmark gate, and environment doctor."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys

from tools.tasks import BUILD_ROOT, FIRMWARE_DIR, REPO_ROOT, run_checked
from tools.tasks.build import find_arduino_cli, find_clang_format, parse_sketch_yaml


def _resolve_serial_port(cli: str, explicit_port: str | None) -> str | None:
    if explicit_port:
        return explicit_port
    proc = subprocess.run([cli, "board", "list", "--format", "json"], capture_output=True, text=True)
    if proc.returncode != 0:
        return None
    try:
        data = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return None
    ports = [item.get("port", {}).get("address") for item in (data.get("detected_ports") or []) if item.get("port", {}).get("address")]
    if len(ports) == 1:
        return str(ports[0])
    print("No serial ports detected. Connect the ESP32-S3 via USB-C and pass --port COMx." if not ports else f"Multiple serial ports detected: {', '.join(ports)}. Pass --port explicitly.")
    return None


def cmd_flash(args: argparse.Namespace) -> int:
    """Safety-check config.h and upload compiled firmware to the attached ESP32-S3."""
    sys.path.insert(0, str(REPO_ROOT))
    from scripts import validate_repo

    if issues := validate_repo.check_firmware_safety_guardrails():  # type: ignore[no-untyped-call]
        print("Refusing to flash due to firmware safety guardrail failure:")
        for item in issues:
            print(f"  - {item}")
        return 1
    cli = find_arduino_cli()
    if not cli:
        print("arduino-cli not found.")
        return 1
    port = _resolve_serial_port(cli, args.port)
    if not port:
        return 1
    sketch_cfg = parse_sketch_yaml()
    build_dir = BUILD_ROOT / "target-compile"
    cmd = [cli, "upload", "--profile", str(sketch_cfg["default_profile"]), "--port", port]
    if build_dir.is_dir():
        cmd.extend(["--input-dir", str(build_dir)])
    cmd.append(str(FIRMWARE_DIR))
    print(f"Flashing {port} ...")
    return run_checked(cmd).returncode


def cmd_monitor(args: argparse.Namespace) -> int:
    """Open serial monitor at 115200 baud cleanly."""
    cli = find_arduino_cli()
    if not cli:
        print("arduino-cli not found.")
        return 1
    port = _resolve_serial_port(cli, args.port)
    return run_checked([cli, "monitor", "--port", port, "--config", f"baudrate={args.baud}"]).returncode if port else 1


def cmd_sim(args: argparse.Namespace) -> int:
    """Launch the local loopback simulator (building WASM cores automatically if needed)."""
    cmd = [sys.executable, str(REPO_ROOT / "simulator" / "run_simulator.py"), *(["--no-open"] if args.no_open else [])]
    return run_checked(cmd).returncode


def cmd_bench(args: argparse.Namespace) -> int:
    """Run host firmware, simulator JS/WASM, and script benchmarks."""
    cmd = [sys.executable, str(REPO_ROOT / "benchmarks" / "run_benchmarks.py")]
    if args.save_baseline:
        cmd.append("--save-baseline")
    if args.check:
        cmd.append("--check")
    if args.with_wasm_build:
        cmd.append("--with-wasm-build")
    return run_checked(cmd).returncode


def cmd_doctor(args: argparse.Namespace) -> int:
    """Diagnose local developer environment, toolchains, git hooks, and Python dependencies."""
    del args
    print("BMO Handheld — Environment Doctor\n" + "=" * 56)
    required_failures = 0
    py_ok = sys.version_info >= (3, 10)
    print(f"  [{'OK' if py_ok else 'FAIL'}] Python: {sys.version.split()[0]} (requires >= 3.10)")
    if not py_ok:
        required_failures += 1

    for mod, pkg in (("PIL", "Pillow"), ("numpy", "numpy"), ("trimesh", "trimesh"), ("manifold3d", "manifold3d")):
        try:
            __import__(mod)
            print(f"  [OK]   Python package: {pkg}")
        except ImportError:
            print(f"  [WARN] Python package missing: {pkg} (run `python -m pip install -r requirements-dev.txt`)")

    hooks_val = (subprocess.run(["git", "config", "--get", "core.hooksPath"], cwd=str(REPO_ROOT), capture_output=True, text=True).stdout or "").strip()
    print(f"  [{'OK' if hooks_val in {'.githooks', '.githooks/'} else 'WARN'}] Git core.hooksPath: {hooks_val or '<unset>'} (expected .githooks)")
    cli = find_arduino_cli()
    print(f"  [{'OK' if cli else 'WARN'}] arduino-cli: {cli or 'not found (run `python tasks.py setup --install-arduino-cli`)'}")
    cf = find_clang_format()
    print(f"  [{'OK' if cf else 'WARN'}] clang-format: {cf or 'not found'}")
    for tool_name in ("cmake", "ctest", "ruff", "cppcheck"):
        found = shutil.which(tool_name)
        print(f"  [{'OK' if found else 'INFO'}] {tool_name}: {found or 'not found in PATH'}")
    print("=" * 56)
    print("DOCTOR PASSED." if not required_failures else f"DOCTOR FAILED ({required_failures} critical issue(s)).")
    return 1 if required_failures else 0
