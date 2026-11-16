#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Cross-platform task runner entrypoint delegating to tools/tasks/ modules."""

from __future__ import annotations

from tools.tasks import (
    ARDUINO_CLI_CHECKSUMS,
    ARDUINO_CLI_VERSION,
    BUILD_PROFILES,
    BUILD_ROOT,
    FIRMWARE_DIR,
    FIRST_PARTY_WARN_RE,
    PARTITIONS_CSV,
    REPO_ROOT,
    REQUIREMENTS_TXT,
    SKETCH_YAML,
)
from tools.tasks.bench import cmd_bench, cmd_doctor, cmd_flash, cmd_monitor, cmd_sim
from tools.tasks.build import (
    cmd_build,
    cmd_format,
    cmd_setup,
    cmd_size,
    find_arduino_cli,
    find_clang_format,
    first_party_cpp_files,
    install_arduino_cli_verified,
    parse_sketch_yaml,
)
from tools.tasks.checks import check_include_case_sensitivity, cmd_check
from tools.tasks.cli import build_parser, main
from tools.tasks.tests import cmd_test

__all__ = [
    "ARDUINO_CLI_CHECKSUMS",
    "ARDUINO_CLI_VERSION",
    "BUILD_PROFILES",
    "BUILD_ROOT",
    "FIRMWARE_DIR",
    "FIRST_PARTY_WARN_RE",
    "PARTITIONS_CSV",
    "REPO_ROOT",
    "REQUIREMENTS_TXT",
    "SKETCH_YAML",
    "build_parser",
    "check_include_case_sensitivity",
    "cmd_bench",
    "cmd_build",
    "cmd_check",
    "cmd_doctor",
    "cmd_flash",
    "cmd_format",
    "cmd_monitor",
    "cmd_setup",
    "cmd_sim",
    "cmd_size",
    "cmd_test",
    "find_arduino_cli",
    "find_clang_format",
    "first_party_cpp_files",
    "install_arduino_cli_verified",
    "main",
    "parse_sketch_yaml",
]

if __name__ == "__main__":
    raise SystemExit(main())
