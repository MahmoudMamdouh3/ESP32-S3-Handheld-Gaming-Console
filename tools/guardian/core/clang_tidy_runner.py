"""
tools.guardian.core.clang_tidy_runner — LLVM Clang-Tidy Deep Static Analyzer Wrapper
Runs clang-tidy with high-signal embedded defect checks (null dereference, use-after-free,
uninitialized values, division by zero, suspicious sizeof/memset, infinite loops) across
host-compilable C/C++ emulator cores and harnesses.
"""

import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional


@dataclass
class ClangTidyFinding:
    file_path: str
    line: int
    column: int
    severity: str
    check_name: str
    message: str


class ClangTidyRunner:
    """Executes LLVM clang-tidy across host-compilable C/C++ firmware cores and harnesses."""

    EMBEDDED_CHECKS = ",".join([
        "-*",
        "clang-analyzer-core.*",
        "clang-analyzer-unix.Malloc",
        "clang-analyzer-cplusplus.NewDelete",
        "bugprone-assert-side-effect",
        "bugprone-infinite-loop",
        "bugprone-sizeof-expression",
        "bugprone-string-constructor",
        "bugprone-suspicious-memset-usage",
        "bugprone-use-after-move",
    ])

    DIAG_REGEX = re.compile(
        r"^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+):\s+(?P<sev>warning|error):\s+(?P<msg>.+?)\s+\[(?P<check>[^\]]+)\]$"
    )

    def __init__(self, repo_root: Path):
        self.repo_root = repo_root
        self.firmware_dir = repo_root / "firmware" / "BmoGameboy"
        self.clang_tidy_bin = self._find_clang_tidy()
        self.timeout_seconds = 300

    def _find_clang_tidy(self) -> Optional[str]:
        which_bin = shutil.which("clang-tidy")
        if which_bin:
            return which_bin

        candidates = [
            Path(sys.executable).resolve().parent / "Scripts" / "clang-tidy.exe",
            Path.home() / "AppData" / "Local" / "Programs" / "Python" / "Python310" / "Scripts" / "clang-tidy.exe",
            Path(r"C:\Program Files\LLVM\bin\clang-tidy.exe"),
        ]
        for c in candidates:
            if c.exists():
                return str(c)
        return None

    def is_available(self) -> bool:
        return self.clang_tidy_bin is not None

    def default_targets(self) -> List[Path]:
        third_party = self.firmware_dir / "src" / "third_party"
        targets = [
            self.repo_root / "tests" / "host" / "host_cores_runner.c",
            third_party / "agnes" / "agnes.c",
            third_party / "smsplus" / "smsplus_bmo.c",
            third_party / "huexpress" / "pce-go.c",
            third_party / "stella2014" / "src" / "Thumbulator.c",
            third_party / "fake08" / "src" / "vm.cpp",
            third_party / "gwenesis" / "bus" / "gwenesis_bus.c",
            third_party / "snes9x" / "src" / "apu.c",
            third_party / "oswan" / "WS.c",
            third_party / "race" / "cz80_support.c",
            third_party / "handy" / "cart.cpp",
            third_party / "doom" / "src" / "doomgeneric.c",
        ]
        return [t for t in targets if t.exists()]

    def run(self, files: Optional[List[Path]] = None) -> List[ClangTidyFinding]:
        if not self.is_available():
            return []

        target_files = files if files else self.default_targets()
        if not target_files:
            return []

        cmd = [
            self.clang_tidy_bin,
            "--quiet",
            f"-checks={self.EMBEDDED_CHECKS}",
            *[str(p) for p in target_files],
            "--",
            "-D_CRT_SECURE_NO_WARNINGS",
            f"-I{self.firmware_dir / 'src'}",
            f"-I{self.firmware_dir / 'src' / 'hal'}",
            f"-I{self.firmware_dir / 'src' / 'storage'}",
            f"-I{self.firmware_dir / 'src' / 'ui'}",
            f"-I{self.firmware_dir / 'src' / 'engines'}",
            f"-I{self.firmware_dir / 'src' / 'third_party'}",
        ]

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

        findings: List[ClangTidyFinding] = []
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
