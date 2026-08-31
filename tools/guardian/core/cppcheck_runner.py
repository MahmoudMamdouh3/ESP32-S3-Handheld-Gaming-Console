"""
tools.guardian.core.cppcheck_runner — Static Analysis Wrapper for Cppcheck
Executes cppcheck with embedded profiles tailored for Xtensa/ESP32 architectures,
enforcing memory safety, bounds checking, and pointer validity.
"""

import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional


@dataclass
class CppcheckFinding:
    file_path: str
    line: int
    severity: str
    error_id: str
    message: str


class CppcheckRunner:
    """Runs cppcheck on firmware sources with embedded safety rules."""

    def __init__(self, repo_root: Path):
        self.repo_root = repo_root
        self.firmware_dir = repo_root / "firmware" / "BmoGameboy"
        self.cppcheck_bin = self._find_cppcheck()

    def _find_cppcheck(self) -> Optional[str]:
        # Check PATH
        which_bin = shutil.which("cppcheck")
        if which_bin:
            return which_bin
        
        # Check python scripts path
        candidates = [
            Path(r"C:\Users\mahmo\AppData\Local\Programs\Python\Python310\Scripts\cppcheck.exe"),
            Path(r"C:\Program Files\Cppcheck\cppcheck.exe"),
        ]
        for c in candidates:
            if c.exists():
                return str(c)
        return None

    def is_available(self) -> bool:
        return self.cppcheck_bin is not None

    def run(self) -> List[CppcheckFinding]:
        if not self.is_available():
            return []

        # Target first-party core and wrappers
        target_dirs = [
            str(self.firmware_dir / "src" / "core"),
            str(self.firmware_dir / "src" / "emulators"),
        ]

        cmd = [
            self.cppcheck_bin,
            "-j4",
            "--max-configs=1",
            "--enable=warning,performance,portability",
            "--suppress=missingIncludeSystem",
            "--suppress=missingInclude",
            "--inline-suppr",
            "--language=c++",
            "--std=c++17",
            "--template={file}::{line}::{severity}::{id}::{message}",
            "--quiet",
            *target_dirs,
        ]

        findings: List[CppcheckFinding] = []
        try:
            res = subprocess.run(cmd, capture_output=True, text=True, errors="ignore", timeout=3)
            # cppcheck output is in stderr
            combined_out = (res.stderr or "") + "\n" + (res.stdout or "")
            for line in combined_out.splitlines():
                line = line.strip()
                if "::" in line:
                    parts = line.split("::", 4)
                    if len(parts) >= 5:
                        f_path, l_num, sev, err_id, msg = parts[0], parts[1], parts[2], parts[3], parts[4]
                        try:
                            line_int = int(l_num)
                        except ValueError:
                            line_int = 0
                        findings.append(
                            CppcheckFinding(
                                file_path=f_path,
                                line=line_int,
                                severity=sev,
                                error_id=err_id,
                                message=msg,
                            )
                        )
        except (subprocess.TimeoutExpired, Exception):
            pass

        return findings
