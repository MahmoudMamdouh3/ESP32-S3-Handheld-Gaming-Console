# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Host CTest and unittest suites."""

from __future__ import annotations

import argparse
import os
import shutil
import sys

from tools.tasks import REPO_ROOT, run_checked


def _resolve_sanitizer_flags() -> list[str] | str:
    if os.name != "nt":
        return []
    clang_cxx = shutil.which("clang++") or os.environ.get("CXX", "")
    clang_cc = shutil.which("clang") or os.environ.get("CC", "")
    gxx, gcc = shutil.which("g++"), shutil.which("gcc")
    if gxx and gcc:
        return [f"-DCMAKE_C_COMPILER={gcc}", f"-DCMAKE_CXX_COMPILER={gxx}"]
    if clang_cxx and clang_cc and "wasi" not in str(clang_cxx).lower():
        return [f"-DCMAKE_C_COMPILER={clang_cc}", f"-DCMAKE_CXX_COMPILER={clang_cxx}"]
    return "ASan+UBSan requires GCC or Clang host compiler cleanly (use Linux CI or MinGW/Clang)."


def run_host_ctest(use_sanitizers: bool = False) -> str | None:
    """Configure, build, and execute host C++17 CTest suite when CMake/CTest are present."""
    cmake_bin, ctest_bin = shutil.which("cmake"), shutil.which("ctest")
    if not cmake_bin or not ctest_bin:
        print("  INFO: cmake/ctest not found; skipping native C++17 CTest suite.")
        return None
    if not (REPO_ROOT / "CMakeLists.txt").is_file():
        return "CMakeLists.txt missing for host CTest suite"

    if use_sanitizers:
        build_dir = REPO_ROOT / "build" / "host-ctest-san"
        compilers = _resolve_sanitizer_flags()
        if isinstance(compilers, str):
            return compilers
        cmds = [
            [cmake_bin, "-S", str(REPO_ROOT), "-B", str(build_dir), "-DBMO_ENABLE_ASAN_UBSAN=ON", "-DCMAKE_BUILD_TYPE=Debug", *compilers],
            [cmake_bin, "--build", str(build_dir)],
            [ctest_bin, "--test-dir", str(build_dir), "--output-on-failure"],
        ]
    else:
        build_dir = REPO_ROOT / "build" / "host-ctest"
        cmds = [] if (build_dir / "CMakeCache.txt").is_file() else [[cmake_bin, "-S", str(REPO_ROOT), "-B", str(build_dir)]]
        cmds.extend([
            [cmake_bin, "--build", str(build_dir), "--config", "Release"],
            [ctest_bin, "--test-dir", str(build_dir), "-C", "Release", "--output-on-failure"],
        ])

    for cmd in cmds:
        if run_checked(cmd).returncode != 0:
            return f"host CTest step failed: {' '.join(cmd)}"
    return None


def cmd_test(args: argparse.Namespace) -> int:
    """Run full host unit test suites and validate_repo."""
    if ctest_err := run_host_ctest(use_sanitizers=getattr(args, "sanitizers", False)):
        print(f"FAIL: {ctest_err}")
        return 1
    cmds = [
        [sys.executable, "-m", "unittest", "discover", "-s", "tests", "-v"],
        [sys.executable, "-m", "unittest", "discover", "-s", "tools/guardian/tests", "-v"],
        [sys.executable, "scripts/validate_repo.py", *(["--require-target-compile"] if args.strict else [])],
    ]
    for cmd in cmds:
        print(f"==> Running: {' '.join(cmd)}")
        if run_checked(cmd).returncode != 0:
            return 1
    return 0
