# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Shared constants and subprocess helpers for BMO Handheld task modules."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FIRMWARE_DIR = REPO_ROOT / "firmware" / "BmoGameboy"
SKETCH_YAML = FIRMWARE_DIR / "sketch.yaml"
PARTITIONS_CSV = FIRMWARE_DIR / "partitions.csv"
REQUIREMENTS_TXT = REPO_ROOT / "requirements.txt"
BUILD_ROOT = REPO_ROOT / "build"

ARDUINO_CLI_VERSION = "1.6.0"
ARDUINO_CLI_CHECKSUMS: dict[tuple[str, str], tuple[str, str]] = {
    ("Windows", "AMD64"): (
        f"https://github.com/arduino/arduino-cli/releases/download/v{ARDUINO_CLI_VERSION}/"
        f"arduino-cli_{ARDUINO_CLI_VERSION}_Windows_64bit.zip",
        "2769b9b282ce597db9679d8ec14d726d57004ca4205ef4d7a795bf36b6cdc9d8",
    ),
    ("Linux", "x86_64"): (
        f"https://github.com/arduino/arduino-cli/releases/download/v{ARDUINO_CLI_VERSION}/"
        f"arduino-cli_{ARDUINO_CLI_VERSION}_Linux_64bit.tar.gz",
        "321ee249f44c78053234fec0f36101092bc6021bf24939e15e0cc2449c5ed250",
    ),
    ("Darwin", "x86_64"): (
        f"https://github.com/arduino/arduino-cli/releases/download/v{ARDUINO_CLI_VERSION}/"
        f"arduino-cli_{ARDUINO_CLI_VERSION}_macOS_64bit.tar.gz",
        "af6a30cf5f57cc076b45fe55c6fd389440156f70706e7f8d3302a11a9fe32913",
    ),
    ("Darwin", "arm64"): (
        f"https://github.com/arduino/arduino-cli/releases/download/v{ARDUINO_CLI_VERSION}/"
        f"arduino-cli_{ARDUINO_CLI_VERSION}_macOS_ARM64.tar.gz",
        "dc70f037fcbc8e7add892282bed5bab5eac317ff00bf0dd2f21e4a8e1d338a1c",
    ),
}

BUILD_PROFILES: dict[str, list[str]] = {
    "default": [],
    "no-doom": ["-DBMO_ENABLE_DOOM=0"],
    "mit": ["-DBMO_EXPERIMENTAL_CORES=0", "-DBMO_ENABLE_DOOM=0"],
    "audio-compile-only": ["-DFEATURE_AUDIO=1", "-DBMO_HARDWARE_CONFIRMED_AUDIO=1"],
    "battery-compile-only": ["-DFEATURE_BATTERY_MONITOR=1", "-DBMO_HARDWARE_CONFIRMED_BATTERY=1"],
    "debug-log-compile-only": ["-DLOG_LEVEL=LOG_LEVEL_DEBUG"],
    "profiler-compile-only": ["-DBMO_PROFILE=1"],
}

FIRST_PARTY_WARN_RE = re.compile(
    r"firmware[/\\]BmoGameboy[/\\](BmoGameboy\.ino|src[/\\](app|hal|storage|ui|cores|tests)[/\\]).*?:\s*warning:",
    re.IGNORECASE,
)


def run_checked(
    cmd: list[str],
    *,
    cwd: Path = REPO_ROOT,
    capture: bool = False,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    """Execute a subprocess relative to REPO_ROOT without throwing on non-zero return."""
    return subprocess.run(cmd, cwd=str(cwd), text=True, capture_output=capture, env=env)
