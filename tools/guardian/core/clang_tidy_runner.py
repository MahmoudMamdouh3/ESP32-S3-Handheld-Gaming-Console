# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""
tools.guardian.core.clang_tidy_runner — LLVM Clang-Tidy Deep Static Analyzer Wrapper
Runs clang-tidy using the repository `.clang-tidy` configuration across first-party
firmware sources (`firmware/BmoGameboy/src/`) and host C/C++ verification suites.
"""

from __future__ import annotations

import json
import os
import re
import shutil
import site
import subprocess
import sys
import sysconfig
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


@dataclass
class ClangTidyFinding:
    file_path: str
    line: int
    column: int
    severity: str
    check_name: str
    message: str


class ClangTidyRunner:
    """Executes LLVM clang-tidy across first-party firmware and host C/C++ targets."""

    DIAG_REGEX = re.compile(
        r"^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+):\s+(?P<sev>warning|error):\s+(?P<msg>.+?)\s+\[(?P<check>[^\]]+)\]$"
    )
    XTENSA_UNSUPPORTED_FLAGS = {
        "-mlongcalls",
        "-fno-tree-switch-conversion",
        "-fstrict-volatile-bitfields",
        "-mdisable-hardware-atomics",
        "-mtext-section-literals",
    }

    def __init__(self, repo_root: Path):
        self.repo_root = repo_root
        self.firmware_dir = repo_root / "firmware" / "BmoGameboy"
        self.clang_tidy_bin = self._find_clang_tidy()
        self.timeout_seconds = 300

    def _find_clang_tidy(self) -> Optional[str]:
        which_bin = shutil.which("clang-tidy")
        if which_bin:
            return which_bin

        exe_name = "clang-tidy.exe" if os.name == "nt" else "clang-tidy"
        candidates: list[Path] = [
            Path(sys.executable).resolve().parent / "Scripts" / exe_name,
            Path(sys.executable).resolve().parent / exe_name,
        ]
        scripts_path = sysconfig.get_path("scripts")
        if scripts_path:
            candidates.append(Path(scripts_path) / exe_name)
        user_base = site.getuserbase()
        if user_base:
            candidates.append(Path(user_base) / "Scripts" / exe_name)
            candidates.append(Path(user_base) / "bin" / exe_name)
        for wasi_bin in sorted((self.repo_root / ".tools").glob(f"wasi-sdk-*/bin/{exe_name}")):
            candidates.append(wasi_bin)
        candidates.append(Path(r"C:\Program Files\LLVM\bin\clang-tidy.exe"))

        for c in candidates:
            if c.is_file():
                return str(c)
        return None

    def is_available(self) -> bool:
        return self.clang_tidy_bin is not None

    def default_targets(self) -> list[Path]:
        """Collect first-party firmware sources and host C/C++ verification targets."""
        src_root = self.firmware_dir / "src"
        targets: list[Path] = []
        compdb = self.repo_root / "build" / "compile_commands.json"
        if compdb.is_file():
            for folder in ("app", "hal", "storage", "ui", "cores"):
                d = src_root / folder
                if d.is_dir():
                    targets.extend(sorted(d.glob("*.cpp")))
        else:
            # Host-compilable first-party modules and host test/benchmark harnesses
            for p in (
                src_root / "hal" / "spi_arbiter.cpp",
                self.repo_root / "simulator" / "bmo_face_firmware.cpp",
                self.repo_root / "simulator" / "bmo_face_wasm.cpp",
            ):
                if p.is_file():
                    targets.append(p)
            targets.extend(sorted((self.repo_root / "tests" / "host").glob("*_test.cpp")))
            runner = self.repo_root / "tests" / "host" / "host_cores_runner.c"
            if runner.is_file():
                targets.append(runner)
        return [t for t in targets if t.exists()]

    def _prepare_compdb(self) -> Optional[Path]:
        src_compdb = self.repo_root / "build" / "compile_commands.json"
        if not src_compdb.is_file():
            return None
        try:
            entries = json.loads(src_compdb.read_text(encoding="utf-8", errors="ignore"))
        except Exception:
            return None
        if not isinstance(entries, list):
            return None

        cleaned = []
        for item in entries:
            if not isinstance(item, dict):
                continue
            args = item.get("arguments")
            if isinstance(args, list):
                filtered = [a for a in args if a not in self.XTENSA_UNSUPPORTED_FLAGS]
                new_item = dict(item)
                new_item["arguments"] = filtered
                cleaned.append(new_item)
            else:
                cmd_str = str(item.get("command", ""))
                for flag in self.XTENSA_UNSUPPORTED_FLAGS:
                    cmd_str = cmd_str.replace(f" {flag}", "")
                new_item = dict(item)
                new_item["command"] = cmd_str
                cleaned.append(new_item)

        out_dir = self.repo_root / "build" / "clang_tidy_compdb"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "compile_commands.json").write_text(json.dumps(cleaned), encoding="utf-8")
        return out_dir

    def run(self, files: Optional[list[Path]] = None) -> list[ClangTidyFinding]:
        if not self.is_available() or not self.clang_tidy_bin:
            return []

        target_files = files if files else self.default_targets()
        if not target_files:
            return []

        compdb_dir = self._prepare_compdb()
        cmd: list[str] = [self.clang_tidy_bin, "--quiet"]
        config_file = self.repo_root / ".clang-tidy"
        if config_file.is_file():
            cmd.append(f"--config-file={config_file}")
        if compdb_dir and files is None:
            cmd.extend(["-p", str(compdb_dir)])
            cmd.extend(str(p) for p in target_files)
        else:
            cmd.extend(str(p) for p in target_files)
            cmd.extend(
                [
                    "--",
                    "-std=c++17",
                    "-DBMO_HOST_TEST=1",
                    "-DLOG_LEVEL=0",
                    "-D_CRT_SECURE_NO_WARNINGS",
                    f"-I{self.firmware_dir}",
                    f"-I{self.firmware_dir / 'src'}",
                    f"-I{self.firmware_dir / 'src' / 'app'}",
                    f"-I{self.firmware_dir / 'src' / 'hal'}",
                    f"-I{self.firmware_dir / 'src' / 'storage'}",
                    f"-I{self.firmware_dir / 'src' / 'ui'}",
                    f"-I{self.firmware_dir / 'src' / 'cores'}",
                    f"-I{self.firmware_dir / 'src' / 'third_party'}",
                    f"-I{self.repo_root / 'simulator' / 'hal'}",
                    f"-I{self.repo_root / 'simulator' / 'bmo_face_compat'}",
                    f"-I{self.repo_root / 'tests' / 'host' / 'fakes'}",
                ]
            )

        try:
            res = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                errors="ignore",
                timeout=self.timeout_seconds,
            )
        except subprocess.TimeoutExpired as exc:
            raise RuntimeError(f"clang-tidy timed out after {self.timeout_seconds}s") from exc
        except OSError as exc:
            raise RuntimeError(f"Unable to run clang-tidy: {exc}") from exc

        findings: list[ClangTidyFinding] = []
        combined = (res.stdout or "") + "\n" + (res.stderr or "")
        for raw_line in combined.splitlines():
            m = self.DIAG_REGEX.match(raw_line.strip())
            if not m:
                continue
            check_name = m.group("check")
            if check_name.startswith("clang-diagnostic-"):
                continue
            f_path = m.group("file").replace("\\", "/")
            try:
                rel_p = str(Path(m.group("file")).resolve().relative_to(self.repo_root.resolve())).replace("\\", "/")
            except Exception:
                rel_p = f_path
            findings.append(
                ClangTidyFinding(
                    file_path=rel_p,
                    line=int(m.group("line")),
                    column=int(m.group("col")),
                    severity=m.group("sev"),
                    check_name=check_name,
                    message=m.group("msg"),
                )
            )
        return findings
