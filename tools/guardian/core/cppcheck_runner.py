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
        self.timeout_seconds = 600

    def _find_cppcheck(self) -> Optional[str]:
        # Check PATH
        which_bin = shutil.which("cppcheck")
        if which_bin:
            return which_bin
        
        # Check python scripts path and standard Windows install locations dynamically
        import sys
        candidates = [
            Path(sys.executable).resolve().parent / "Scripts" / "cppcheck.exe",
            Path.home() / "AppData" / "Local" / "Programs" / "Python" / "Python310" / "Scripts" / "cppcheck.exe",
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
            str(self.firmware_dir / "src"),
        ]

        cmd = [
            self.cppcheck_bin,
            "-j8",
            "--max-configs=1",
            "--enable=warning,performance,portability",
            "--suppress=missingIncludeSystem",
            "--suppress=missingInclude",
            "--inline-suppr",
            "--error-exitcode=1",
            "--template={file}::{line}::{severity}::{id}::{message}",
            "--quiet",
            *target_dirs,
        ]

        findings: List[CppcheckFinding] = []
        try:
            res = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                errors="ignore",
                timeout=self.timeout_seconds,
            )
        except subprocess.TimeoutExpired as exc:
            raise RuntimeError(
                f"Cppcheck timed out after {self.timeout_seconds} seconds."
            ) from exc
        except OSError as exc:
            raise RuntimeError(f"Unable to run Cppcheck: {exc}") from exc

        combined_out = (res.stderr or "") + "\n" + (res.stdout or "")
        for line in combined_out.splitlines():
            line = line.strip()
            if "::" in line:
                parts = line.split("::", 4)
                if len(parts) >= 5:
                    f_path, l_num, sev, err_id, msg = parts[0], parts[1], parts[2], parts[3], parts[4]
                    if sev not in {"error", "warning", "performance", "portability", "style"}:
                        continue
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

        if res.returncode != 0 and not findings:
            detail = combined_out.strip() or "no diagnostic output"
            raise RuntimeError(
                f"Cppcheck exited with code {res.returncode}: {detail}"
            )

        return findings
