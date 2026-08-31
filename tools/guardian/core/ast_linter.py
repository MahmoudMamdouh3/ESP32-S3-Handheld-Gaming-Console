"""
tools.guardian.core.ast_linter — Embedded Firmware Static AST & Pattern Linter
Analyzes C, C++, and INO firmware files to detect memory leaks, unaligned access,
missing compiler optimizations, IRAM/DRAM misplacements, and hot-path anti-patterns.
"""

import re
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional


@dataclass
class LintFinding:
    rule_id: str
    severity: str  # CRITICAL, WARNING, ADVISORY
    file_path: Path
    line_number: int
    line_content: str
    message: str
    suggested_fix: str


class FirmwareAstLinter:
    """Embedded static linter tailored for ESP32-S3 retro gaming console firmware."""

    def __init__(self, firmware_dir: Path):
        self.firmware_dir = firmware_dir

    def lint_all(self) -> List[LintFinding]:
        findings: List[LintFinding] = []
        
        # Scan all C, C++, H, and INO files in firmware
        for path in sorted(self.firmware_dir.rglob("*")):
            if not path.is_file():
                continue
            suffix = path.suffix.lower()
            if suffix not in {".c", ".cpp", ".h", ".hpp", ".ino"}:
                continue
                
            findings.extend(self.lint_file(path))
            
        return findings

    def lint_file(self, path: Path) -> List[LintFinding]:
        findings: List[LintFinding] = []
        try:
            content = path.read_text(encoding="utf-8", errors="ignore")
        except Exception as e:
            return findings

        lines = content.splitlines()
        rel_path = path.relative_to(self.firmware_dir)
        rel_path_str = str(rel_path).replace("\\", "/")

        # Rule 1: Check for naked Serial.print in emulator wrappers
        if "src/emulators/" in rel_path_str:
            for idx, line in enumerate(lines, 1):
                clean = line.strip()
                if clean.startswith("//") or clean.startswith("/*"):
                    continue
                if re.search(r"\bSerial\.(print|println|printf|write)\b", clean):
                    findings.append(
                        LintFinding(
                            rule_id="NAKED_SERIAL_PRINT_IN_EMU",
                            severity="CRITICAL",
                            file_path=path,
                            line_number=idx,
                            line_content=clean,
                            message="Naked Serial.print call inside emulator wrapper hot path.",
                            suggested_fix="Replace with gated LOG_ERROR, LOG_WARN, or LOG_INFO macro.",
                        )
                    )

        # Rule 2: Check for missing #pragma GCC optimize in emulator wrappers
        if "src/emulators/emu_" in rel_path_str and rel_path_str.endswith(".cpp"):
            first_non_comment = ""
            for line in lines:
                s = line.strip()
                if s and not s.startswith("//") and not s.startswith("/*"):
                    first_non_comment = s
                    break
            if not first_non_comment.startswith('#pragma GCC optimize'):
                findings.append(
                    LintFinding(
                        rule_id="MISSING_GCC_OPTIMIZE_PRAGMA",
                        severity="WARNING",
                        file_path=path,
                        line_number=1,
                        line_content=lines[0] if lines else "",
                        message="Emulator wrapper missing #pragma GCC optimize(\"O3,unroll-loops\").",
                        suggested_fix="Add `#pragma GCC optimize(\"O3,unroll-loops\")` to top of file.",
                    )
                )

        # Rule 3: Check for plain malloc() in vendor engines & firmware (should use PSRAM)
        for idx, line in enumerate(lines, 1):
            clean = line.strip()
            if clean.startswith("//") or clean.startswith("/*") or clean.startswith("#define"):
                continue
            # Look for malloc() that is not wrapped or heap_caps_malloc
            if re.search(r"\bmalloc\s*\([^)]+\)", clean) and "heap_caps_malloc" not in clean and "Doom_MallocPSRAM" not in clean:
                # Ignore small host test files or harmless string mallocs
                if "host_test" not in rel_path_str and "m_argv" not in rel_path_str:
                    severity = "CRITICAL" if any(k in clean.lower() for k in ["screen", "framebuffer", "ctx", "zone", "ram", "buffer"]) else "WARNING"
                    findings.append(
                        LintFinding(
                            rule_id="NAKED_MALLOC_WITHOUT_SPIRAM",
                            severity=severity,
                            file_path=path,
                            line_number=idx,
                            line_content=clean,
                            message="Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM.",
                            suggested_fix="Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`.",
                        )
                    )

        # Rule 4: Check for sub-optimal SD card SPI frequency in sd_card.cpp
        if rel_path_str.endswith("sd_card.cpp"):
            for idx, line in enumerate(lines, 1):
                if "SD.begin" in line and "4000000" in line:
                    findings.append(
                        LintFinding(
                            rule_id="SD_SPI_SPEED_SUBOPTIMAL",
                            severity="CRITICAL",
                            file_path=path,
                            line_number=idx,
                            line_content=line.strip(),
                            message="SD card mounted at 4 MHz instead of 25 MHz standard (5-6x slower ROM load).",
                            suggested_fix="Increase SD.begin clock to 25000000 (25 MHz).",
                        )
                    )

        # Rule 5: Check for O(N) linear scans called in per-frame loop in BmoGameboy.ino
        if rel_path_str.endswith("BmoGameboy.ino"):
            for idx, line in enumerate(lines, 1):
                clean = line.strip()
                if "countGamesForConsole(" in clean and "loop" in content[:content.find(clean)]:
                    findings.append(
                        LintFinding(
                            rule_id="PER_FRAME_O_N_SCAN",
                            severity="CRITICAL",
                            file_path=path,
                            line_number=idx,
                            line_content=clean,
                            message="countGamesForConsole() called inside per-frame UI loop (up to 245K iterations/frame).",
                            suggested_fix="Cache console game counts in a static array at SD scan time.",
                        )
                    )
                if "rebuildVisibleGames()" in clean and idx > 250:
                    # In game menu frame loop
                    findings.append(
                        LintFinding(
                            rule_id="UNGUARDED_PER_FRAME_FILTER",
                            severity="WARNING",
                            file_path=path,
                            line_number=idx,
                            line_content=clean,
                            message="rebuildVisibleGames() called unconditionally on every game menu frame.",
                            suggested_fix="Gate with a `visibleGamesDirty` boolean flag.",
                        )
                    )

        # Rule 6: Check for hard stop flags in config.h
        if rel_path_str.endswith("config.h"):
            for idx, line in enumerate(lines, 1):
                clean = line.strip()
                if re.match(r"^#define\s+FEATURE_AUDIO\s+[^0\s]", clean):
                    findings.append(
                        LintFinding(
                            rule_id="HARD_STOP_AUDIO_NONZERO",
                            severity="CRITICAL",
                            file_path=path,
                            line_number=idx,
                            line_content=clean,
                            message="HARD STOP: FEATURE_AUDIO is non-zero without verified soldered hardware.",
                            suggested_fix="Keep FEATURE_AUDIO 0 until physical DAC is verified.",
                        )
                    )
                if re.match(r"^#define\s+FEATURE_BATTERY_MONITOR\s+[^0\s]", clean):
                    findings.append(
                        LintFinding(
                            rule_id="HARD_STOP_BATTERY_NONZERO",
                            severity="CRITICAL",
                            file_path=path,
                            line_number=idx,
                            line_content=clean,
                            message="HARD STOP: FEATURE_BATTERY_MONITOR is non-zero without verified voltage divider.",
                            suggested_fix="Keep FEATURE_BATTERY_MONITOR 0 to prevent floating ADC bootloop.",
                        )
                    )

        # Rule 7: Check for stack-allocated large frame buffers
        for idx, line in enumerate(lines, 1):
            clean = line.strip()
            # Catch e.g. uint16_t row_buf[256] inside functions
            if re.search(r"uint16_t\s+[a-zA-Z0-9_]+\[\s*(240|256|320|480)\s*\]", clean):
                # If inside function (indented), flag warning
                if line.startswith("  ") or line.startswith("\t"):
                    if "static" not in clean:
                        findings.append(
                            LintFinding(
                                rule_id="STACK_ALLOCATED_SCANLINE_BUFFER",
                                severity="WARNING",
                                file_path=path,
                                line_number=idx,
                                line_content=clean,
                                message="Large scanline buffer allocated on stack in hot path.",
                                suggested_fix="Make static module-level or 4-byte aligned DRAM/PSRAM buffer.",
                            )
                        )

        return findings
