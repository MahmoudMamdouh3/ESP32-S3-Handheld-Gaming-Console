"""
tools.guardian.core.ast_linter — Embedded Firmware Static AST & Pattern Linter
Analyzes C, C++, and INO firmware files to detect memory leaks, unaligned access,
missing compiler optimizations, IRAM/DRAM misplacements, line density metrics, and hot-path anti-patterns.
"""

import re
from dataclasses import dataclass
from pathlib import Path
from typing import List


@dataclass
class LintFinding:
    rule_id: str
    severity: str  # CRITICAL, WARNING, ADVISORY
    file_path: Path
    line_number: int
    line_content: str
    message: str
    suggested_fix: str


@dataclass
class FileLineStats:
    file_path: Path
    rel_path: str
    category: str
    total_lines: int
    sloc: int
    comment_lines: int
    blank_lines: int
    code_density_percent: float


class FirmwareAstLinter:
    """Embedded static linter tailored for ESP32-S3 retro gaming console firmware."""

    def __init__(self, firmware_dir: Path):
        self.firmware_dir = firmware_dir
        self.repo_root = firmware_dir.parents[1] if firmware_dir.name == "BmoGameboy" else firmware_dir

    EXCLUDED_TOP_DIRS = {
        ".git", ".venv", "venv", "zig", "arduino-cli-bin", ".rom_cache",
        "games", "roms", "resources", "scratch", "build", "BMO asperite files", "test_compile"
    }
    EXCLUDED_ANY_DIRS = {"__pycache__", "legacy_headers", ".git"}

    def _is_excluded(self, path: Path) -> bool:
        try:
            rel_parts = path.relative_to(self.repo_root).parts
        except ValueError:
            rel_parts = path.parts
        if not rel_parts:
            return True
        if rel_parts[0] in self.EXCLUDED_TOP_DIRS:
            return True
        if self.EXCLUDED_ANY_DIRS.intersection(rel_parts):
            return True
        return False

    def get_repo_line_stats(self) -> List[FileLineStats]:
        """Calculates line statistics across all tracked source and documentation files in the repository."""
        stats: List[FileLineStats] = []
        target_suffixes = {".c", ".cpp", ".h", ".hpp", ".ino", ".py", ".md", ".json", ".csv"}

        for path in sorted(self.repo_root.rglob("*")):
            if self._is_excluded(path) or not path.is_file():
                continue
            suffix = path.suffix.lower()
            if suffix not in target_suffixes:
                continue

            rel_path = str(path.relative_to(self.repo_root)).replace("\\", "/")

            cat = "OTHER"
            if rel_path.startswith("firmware/BmoGameboy/src/core"):
                cat = "FIRMWARE_CORE"
            elif rel_path.startswith("firmware/BmoGameboy/src/emulators"):
                cat = "EMULATOR_WRAPPERS"
            elif rel_path.startswith("firmware/BmoGameboy/src/engine"):
                cat = "ENGINE_GBC"
            elif rel_path.startswith("firmware/BmoGameboy/src/vendor"):
                cat = "VENDOR_ENGINES"
            elif rel_path.startswith("firmware/BmoGameboy/src/assets"):
                cat = "BAKED_ROMS"
            elif rel_path.startswith("firmware/BmoGameboy"):
                cat = "FIRMWARE_TOP"
            elif rel_path.startswith("tools/guardian"):
                cat = "GUARDIAN_TOOLCHAIN"
            elif rel_path.startswith("tools"):
                cat = "TOOLS"
            elif rel_path.startswith("scripts"):
                cat = "SCRIPTS"
            elif rel_path.startswith("tests"):
                cat = "TESTS"
            elif rel_path.startswith(".agents/rules"):
                cat = "AGENT_RULES"
            elif rel_path.startswith("docs"):
                cat = "DOCUMENTATION"

            # Fast path for multi-megabyte baked ROM C array headers (> 256 KB)
            try:
                file_size = path.stat().st_size
            except OSError:
                continue

            if cat == "BAKED_ROMS" and file_size > 262144:
                try:
                    raw = path.read_bytes()
                    total = raw.count(b"\n") + 1
                except Exception:
                    continue
                stats.append(
                    FileLineStats(
                        file_path=path,
                        rel_path=rel_path,
                        category=cat,
                        total_lines=total,
                        sloc=total,
                        comment_lines=0,
                        blank_lines=0,
                        code_density_percent=100.0,
                    )
                )
                continue

            try:
                content = path.read_text(encoding="utf-8", errors="ignore")
            except Exception:
                continue

            lines = content.splitlines()
            total = len(lines)
            blanks = sum(1 for l in lines if not l.strip())
            comments = 0
            sloc = 0

            in_block_comment = False
            for line in lines:
                s = line.strip()
                if not s:
                    continue
                if suffix in {".c", ".cpp", ".h", ".hpp", ".ino"}:
                    if in_block_comment:
                        comments += 1
                        if "*/" in s:
                            in_block_comment = False
                    elif s.startswith("/*"):
                        comments += 1
                        if "*/" not in s:
                            in_block_comment = True
                    elif s.startswith("//"):
                        comments += 1
                    else:
                        sloc += 1
                elif suffix == ".py":
                    if s.startswith("#") or '"""' in s or "'''" in s:
                        comments += 1
                    else:
                        sloc += 1
                elif suffix == ".md":
                    if s.startswith("<!--"):
                        comments += 1
                    else:
                        sloc += 1
                else:
                    sloc += 1

            density = (sloc / total * 100.0) if total > 0 else 0.0
            stats.append(
                FileLineStats(
                    file_path=path,
                    rel_path=rel_path,
                    category=cat,
                    total_lines=total,
                    sloc=sloc,
                    comment_lines=comments,
                    blank_lines=blanks,
                    code_density_percent=density,
                )
            )

        return stats

    def lint_all(self) -> List[LintFinding]:
        findings: List[LintFinding] = []

        for path in sorted(self.firmware_dir.rglob("*")):
            if not path.is_file():
                continue
            suffix = path.suffix.lower()
            if suffix not in {".c", ".cpp", ".h", ".hpp", ".ino"}:
                continue
            rel_str = str(path.relative_to(self.firmware_dir)).replace("\\", "/")
            # Skip multi-megabyte baked ROM hex arrays during AST lint
            if rel_str.startswith("src/assets/") and path.stat().st_size > 262144:
                continue
            findings.extend(self.lint_file(path))

        return findings

    def lint_file(self, path: Path) -> List[LintFinding]:
        findings: List[LintFinding] = []
        try:
            content = path.read_text(encoding="utf-8", errors="ignore")
        except Exception:
            return findings

        lines = content.splitlines()
        try:
            rel_path = path.relative_to(self.firmware_dir)
        except ValueError:
            rel_path = path
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
            if not first_non_comment.startswith("#pragma GCC optimize"):
                findings.append(
                    LintFinding(
                        rule_id="MISSING_GCC_OPTIMIZE_PRAGMA",
                        severity="WARNING",
                        file_path=path,
                        line_number=1,
                        line_content=lines[0] if lines else "",
                        message='Emulator wrapper missing #pragma GCC optimize("O3,unroll-loops").',
                        suggested_fix='Add `#pragma GCC optimize("O3,unroll-loops")` to top of file.',
                    )
                )

        # Rule 3: Check for plain malloc() in vendor engines & firmware (should use PSRAM)
        in_host_fallback = False
        for idx, line in enumerate(lines, 1):
            clean = line.strip()
            if clean.startswith("//") or clean.startswith("/*") or clean.startswith("#define"):
                continue
            if "#else" in clean:
                if idx > 2 and ("Doom_MallocPSRAM" in lines[idx - 2] or "heap_caps_malloc" in lines[idx - 2]):
                    in_host_fallback = True
            elif "#endif" in clean:
                in_host_fallback = False

            if in_host_fallback:
                continue

            if re.search(r"\bmalloc\s*\([^)]+\)", clean) and "heap_caps_malloc" not in clean and "Doom_MallocPSRAM" not in clean:
                if "host_test" not in rel_path_str and "m_argv" not in rel_path_str and "m_misc" not in rel_path_str:
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
            loop_idx = next((i for i, l in enumerate(lines, 1) if "void loop()" in l), 9999)
            for idx, line in enumerate(lines, 1):
                clean = line.strip()
                if clean.startswith(("//", "/*")):
                    continue
                if idx > loop_idx and "countGamesForConsole(" in clean:
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
                if idx > loop_idx and "rebuildVisibleGames()" in clean:
                    prev_lines = " ".join(lines[max(0, idx - 3) : idx])
                    if "visibleGamesDirty" not in prev_lines and "visibleGamesDirty" not in clean:
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
                if re.search(r"\bdigitalRead\s*\(", clean):
                    findings.append(
                        LintFinding(
                            rule_id="DIRECT_DIGITAL_READ_IN_HOT_PATH",
                            severity="CRITICAL",
                            file_path=path,
                            line_number=idx,
                            line_content=clean,
                            message="Direct digitalRead() in BmoGameboy.ino bypasses atomic REG_READ(GPIO_IN_REG) button polling.",
                            suggested_fix="Use Buttons::isPressed(), Buttons::justPressed(), or Buttons::gb_joypad_state.",
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
            if re.search(r"uint16_t\s+[a-zA-Z0-9_]+\[\s*(240|256|320|480)\s*\]", clean):
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

        # Rule 8: Check for raw unaligned 16/32-bit pointer casts in ROM reading paths
        if "src/emulators/" in rel_path_str or "src/engine/" in rel_path_str:
            for idx, line in enumerate(lines, 1):
                clean = line.strip()
                if clean.startswith("//") or clean.startswith("/*"):
                    continue
                if re.search(r"\*\s*\(\s*uint16_t\s*\*\s*\)\s*&?[a-zA-Z0-9_]+\[", clean):
                    if "gb_rom_read" in clean or "rom" in clean.lower():
                        findings.append(
                            LintFinding(
                                rule_id="UNALIGNED_ROM_POINTER_CAST",
                                severity="WARNING",
                                file_path=path,
                                line_number=idx,
                                line_content=clean,
                                message="Unaligned 16-bit pointer dereference in ROM memory path.",
                                suggested_fix="Use explicit byte-wise little-endian reconstruction: `b0 | (b1 << 8)`.",
                            )
                        )

        # Rule 9: Check for forbidden Octal PSRAM (GPIO 33-37) or Strapping (GPIO 0, 3, 45, 46) pin definitions in config.h
        if rel_path_str.endswith("config.h"):
            for idx, line in enumerate(lines, 1):
                clean = line.strip()
                m_pin = re.match(r"^#define\s+(BTN_[A-Z0-9_]+|TFT_[A-Z0-9_]+|SD_[A-Z0-9_]+)\s+(\d+)\b", clean)
                if m_pin:
                    pin_macro, pin_num = m_pin.group(1), int(m_pin.group(2))
                    if pin_num in {0, 3, 33, 34, 35, 36, 37, 45, 46}:
                        findings.append(
                            LintFinding(
                                rule_id="FORBIDDEN_STRAPPING_OR_PSRAM_GPIO",
                                severity="CRITICAL",
                                file_path=path,
                                line_number=idx,
                                line_content=clean,
                                message=f"{pin_macro} assigned to reserved strapping/PSRAM GPIO {pin_num}.",
                                suggested_fix="Assign to a safe GPIO per .agents/rules/01_hardware.md.",
                            )
                        )

        # Rule 10: Check that any src/core/*.cpp file calling SD.open / SD.mkdir / SD.remove uses SpiArbiter
        if "src/core/" in rel_path_str and rel_path_str.endswith(".cpp"):
            has_sd_io = any(re.search(r"\bSD\.(open|mkdir|remove|exists)\s*\(", l) for l in lines if not l.strip().startswith("//"))
            if has_sd_io and "SpiArbiter::lock()" not in content:
                findings.append(
                    LintFinding(
                        rule_id="UNARBITRATED_SD_IO_IN_CORE",
                        severity="CRITICAL",
                        file_path=path,
                        line_number=1,
                        line_content="SD.* operation without SpiArbiter::lock()",
                        message="SD card filesystem operation in core driver without SpiArbiter::lock() mutex protection.",
                        suggested_fix="Acquire `SpiArbiter::lock()` before SD I/O and `SpiArbiter::unlock()` after.",
                    )
                )

        return findings
