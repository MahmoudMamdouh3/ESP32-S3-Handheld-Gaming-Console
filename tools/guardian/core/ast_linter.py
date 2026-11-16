# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""
tools.guardian.core.ast_linter — Embedded Firmware Regex/Pattern Heuristic Linter
Analyzes C, C++, and INO firmware files using deterministic regex and line-scan
heuristics (no libclang or tree-sitter AST dependency; kept under its existing module
and class names for CLI/import compatibility) to detect memory leaks, unaligned access,
missing compiler optimizations, IRAM/DRAM misplacements, line density metrics, and hot-path anti-patterns.
"""

import os
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import ClassVar


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

    EXCLUDED_TOP_DIRS: ClassVar[frozenset[str]] = frozenset({
        ".git", ".tools", ".venv", "venv", "zig", "arduino-cli-bin", ".rom_cache",
        "games", "roms", "resources", "scratch", "build", "BMO asperite files", "test_compile",
    })
    EXCLUDED_ANY_DIRS: ClassVar[frozenset[str]] = frozenset({"__pycache__", "legacy_headers", ".git"})
    _CATEGORY_PREFIXES: ClassVar[tuple[tuple[str, str], ...]] = (
        ("firmware/BmoGameboy/src/hal", "FIRMWARE_CORE"),
        ("firmware/BmoGameboy/src/storage", "FIRMWARE_CORE"),
        ("firmware/BmoGameboy/src/ui", "FIRMWARE_CORE"),
        ("firmware/BmoGameboy/src/cores", "EMULATOR_WRAPPERS"),
        ("firmware/BmoGameboy/src/third_party", "THIRD_PARTY_ENGINES"),
        ("firmware/BmoGameboy", "FIRMWARE_TOP"),
        ("simulator", "SIMULATOR"),
        ("hardware", "HARDWARE"),
        ("tools/guardian", "GUARDIAN_TOOLCHAIN"),
        ("tools", "TOOLS"),
        ("scripts", "SCRIPTS"),
        ("tests", "TESTS"),
        ("docs", "DOCUMENTATION"),
    )

    def _is_excluded(self, path: Path) -> bool:
        try:
            rel_parts = path.relative_to(self.repo_root).parts
        except ValueError:
            rel_parts = path.parts
        if not rel_parts or rel_parts[0] in self.EXCLUDED_TOP_DIRS:
            return True
        return bool(self.EXCLUDED_ANY_DIRS.intersection(rel_parts))

    def _classify_rel_path(self, rel_path: str) -> str:
        for prefix, category in self._CATEGORY_PREFIXES:
            if rel_path.startswith(prefix):
                return category
        return "OTHER"

    @staticmethod
    def _count_c_comments_and_sloc(lines: list[str]) -> tuple[int, int]:
        comments = 0
        sloc = 0
        in_block_comment = False
        for line in lines:
            s = line.strip()
            if not s:
                continue
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
        return comments, sloc

    @classmethod
    def _count_file_lines(cls, lines: list[str], suffix: str) -> tuple[int, int]:
        if suffix in {".c", ".cpp", ".h", ".hpp", ".ino"}:
            return cls._count_c_comments_and_sloc(lines)
        comments = 0
        sloc = 0
        for line in lines:
            s = line.strip()
            if not s:
                continue
            if (suffix == ".py" and (s.startswith("#") or '"""' in s or "'''" in s)) or (
                suffix == ".md" and s.startswith("<!--")
            ):
                comments += 1
            else:
                sloc += 1
        return comments, sloc

    def _stat_for_path(self, path: Path, target_suffixes: frozenset[str]) -> FileLineStats | None:
        if self._is_excluded(path) or not path.is_file():
            return None
        suffix = path.suffix.lower()
        if suffix not in target_suffixes:
            return None
        rel_path = str(path.relative_to(self.repo_root)).replace("\\", "/")
        cat = self._classify_rel_path(rel_path)
        try:
            file_size = path.stat().st_size
        except OSError:
            return None

        if cat == "BAKED_ROMS" and file_size > 262144:
            try:
                total = path.read_bytes().count(b"\n") + 1
            except Exception:
                return None
            return FileLineStats(path, rel_path, cat, total, total, 0, 0, 100.0)

        try:
            content = path.read_text(encoding="utf-8", errors="ignore")
        except Exception:
            return None

        lines = content.splitlines()
        total = len(lines)
        blanks = sum(1 for raw in lines if not raw.strip())
        comments, sloc = self._count_file_lines(lines, suffix)
        density = (sloc / total * 100.0) if total > 0 else 0.0
        return FileLineStats(path, rel_path, cat, total, sloc, comments, blanks, density)

    def get_repo_line_stats(self) -> list[FileLineStats]:
        """Calculates line statistics across all tracked source and documentation files in the repository."""
        target_suffixes = frozenset({".c", ".cpp", ".h", ".hpp", ".ino", ".py", ".md", ".json", ".csv"})
        tracked = subprocess.run(
            ["git", "ls-files", "-z"],
            cwd=self.repo_root,
            check=True,
            capture_output=True,
        ).stdout
        stats: list[FileLineStats] = []
        for item in tracked.split(b"\0"):
            if not item:
                continue
            if entry := self._stat_for_path(self.repo_root / os.fsdecode(item), target_suffixes):
                stats.append(entry)
        return stats

    def lint_all(self) -> list[LintFinding]:
        findings: list[LintFinding] = []

        for path in sorted(self.firmware_dir.rglob("*")):
            if not path.is_file():
                continue
            suffix = path.suffix.lower()
            if suffix not in {".c", ".cpp", ".h", ".hpp", ".ino"}:
                continue
            # Skip large optional local baked ROM headers during AST lint
            if path.name == "local_baked_rom.h" and path.stat().st_size > 262144:
                continue
            findings.extend(self.lint_file(path))

        return findings

    def _check_local_stack_arrays(self, path: Path, rel_path_str: str, lines: list[str]) -> list[LintFinding]:
        norm_rel = f"/{rel_path_str.strip('/')}"
        if "/src/third_party/" in norm_rel or "/third_party/" in norm_rel:
            return []
        local_array_re = re.compile(
            r"^\s*(?:(?:static|const|volatile|constexpr)\s+)*(?P<type>uint8_t|int8_t|char|bool|uint16_t|int16_t|short|uint32_t|int32_t|int|long|float|uint64_t|int64_t|double)\s+"
            r"\w+\s*(?P<dims>(?:\[\s*\d+\s*\])+)"
        )
        type_sizes = {
            "uint8_t": 1, "int8_t": 1, "char": 1, "bool": 1,
            "uint16_t": 2, "int16_t": 2, "short": 2,
            "uint32_t": 4, "int32_t": 4, "int": 4, "long": 4, "float": 4,
            "uint64_t": 8, "int64_t": 8, "double": 8,
        }
        function_open_re = re.compile(r"\b[\w:~]+\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?\{")
        findings: list[LintFinding] = []
        brace_depth = 0
        function_depths: list[int] = []
        for idx, line in enumerate(lines, 1):
            clean = line.split("//", 1)[0].strip()
            if clean.startswith(("/*", "*", "#")):
                continue
            match = local_array_re.match(clean)
            if match and function_depths and brace_depth >= function_depths[-1] and "static" not in clean.split("[", 1)[0]:
                dimensions = [int(value) for value in re.findall(r"\[\s*(\d+)\s*\]", match.group("dims"))]
                element_count = 1
                for dimension in dimensions:
                    element_count *= dimension
                byte_count = element_count * type_sizes[match.group("type")]
                if byte_count > 1024:
                    findings.append(
                        LintFinding(
                            rule_id="LARGE_LOCAL_STACK_ARRAY",
                            severity="CRITICAL",
                            file_path=path,
                            line_number=idx,
                            line_content=clean,
                            message=f"Local array occupies {byte_count} bytes on the task stack (limit is 1,024 B).",
                            suggested_fix="Move the buffer to static storage or allocate it from an explicitly budgeted heap.",
                        )
                    )
            opened_function = bool(function_open_re.search(clean))
            brace_depth += clean.count("{") - clean.count("}")
            if opened_function and "{" in clean:
                function_depths.append(brace_depth)
            function_depths = [depth for depth in function_depths if depth <= brace_depth]
        return findings

    def _check_core_wrapper_rules(self, path: Path, rel_path_str: str, lines: list[str]) -> list[LintFinding]:
        findings: list[LintFinding] = []
        if "src/cores/" in rel_path_str:
            for idx, line in enumerate(lines, 1):
                clean = line.strip()
                if not clean.startswith(("//", "/*")) and re.search(r"\bSerial\.(print|println|printf|write)\b", clean):
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
        if "src/cores/emu_" in rel_path_str and rel_path_str.endswith(".cpp"):
            first_non_comment = next(
                (s for raw in lines if (s := raw.strip()) and not s.startswith(("//", "/*"))), ""
            )
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
        return findings

    def _check_malloc_and_sd_rules(self, path: Path, rel_path_str: str, lines: list[str]) -> list[LintFinding]:
        findings: list[LintFinding] = []
        in_host_fallback = False
        skip_path = any(seg in rel_path_str for seg in ("host_test", "m_argv", "m_misc"))
        for idx, line in enumerate(lines, 1):
            clean = line.strip()
            if clean.startswith(("//", "/*", "#define")):
                continue
            if "#else" in clean and idx > 2 and ("Doom_MallocPSRAM" in lines[idx - 2] or "heap_caps_malloc" in lines[idx - 2]):
                in_host_fallback = True
            elif "#endif" in clean:
                in_host_fallback = False
            if (
                not in_host_fallback
                and not skip_path
                and re.search(r"\bmalloc\s*\([^)]+\)", clean)
                and "heap_caps_malloc" not in clean
                and "Doom_MallocPSRAM" not in clean
            ):
                severity = "CRITICAL" if any(k in clean.lower() for k in ("screen", "framebuffer", "ctx", "zone", "ram", "buffer")) else "WARNING"
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

        if rel_path_str.endswith(("sd_card.cpp", "sd_mount.cpp")):
            findings.extend(
                LintFinding(
                    rule_id="SD_SPI_SPEED_SUBOPTIMAL",
                    severity="CRITICAL",
                    file_path=path,
                    line_number=idx,
                    line_content=line.strip(),
                    message="SD card mounted at 4 MHz instead of 25 MHz standard (5-6x slower ROM load).",
                    suggested_fix="Increase SD.begin clock to 25000000 (25 MHz).",
                )
                for idx, line in enumerate(lines, 1)
                if "SD.begin" in line and "4000000" in line
            )
        return findings

    def _check_app_and_hardware_rules(self, path: Path, rel_path_str: str, lines: list[str]) -> list[LintFinding]:
        findings: list[LintFinding] = []
        if rel_path_str.endswith(("BmoGameboy.ino", "bmo_app.cpp")) or "src/app/" in rel_path_str:
            loop_idx = (
                next((i for i, src_line in enumerate(lines, 1) if "void loop()" in src_line), 9999)
                if rel_path_str.endswith("BmoGameboy.ino")
                else 0
            )
            for idx, line in enumerate(lines, 1):
                clean = line.strip()
                if clean.startswith(("//", "/*")):
                    continue
                if idx > loop_idx and "rebuildVisibleGames()" in clean and not clean.startswith("void "):
                    prev_lines = " ".join(lines[max(0, idx - 3) : idx]).lower()
                    if "visiblegamesdirty" not in prev_lines and "visiblegamesdirty" not in clean.lower():
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
                            message="Direct digitalRead() in app/sketch bypasses atomic REG_READ(GPIO_IN_REG) button polling.",
                            suggested_fix="Use Buttons::isPressed(), Buttons::justPressed(), or Buttons::gb_joypad_state.",
                        )
                    )

        if rel_path_str.endswith(("config.h", "feature_flags.h")):
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

        if rel_path_str.endswith(("config.h", "pin_map.h")):
            for idx, line in enumerate(lines, 1):
                clean = line.strip()
                if (m_pin := re.match(r"^#define\s+(BTN_[A-Z0-9_]+|TFT_[A-Z0-9_]+|SD_[A-Z0-9_]+)\s+(\d+)\b", clean)):
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
                                suggested_fix="Assign to a safe GPIO per docs/hardware.md and AGENTS.md.",
                            )
                        )
        return findings

    def _check_spi_arbiter_functions(self, path: Path, rel_path_str: str, lines: list[str]) -> list[LintFinding]:
        if not (
            any(seg in rel_path_str for seg in ("src/app/", "src/hal/", "src/storage/", "src/ui/"))
            and rel_path_str.endswith(".cpp")
            and not rel_path_str.endswith("spi_arbiter.cpp")
        ):
            return []
        fn_open_re = re.compile(r"^\s*(?:[\w:<>*&]+\s+)+(?P<name>[\w:~]+)\s*\([^;{}]*\)\s*(?:const\s*)?\{")
        io_re = re.compile(
            r"\b(?:SD\.(?:open|mkdir|remove|exists|rename|rmdir)|"
            r"SPI\.(?:writeBytes|transfer|beginTransaction)|"
            r"tft\.(?:startWrite|setAddrWindow|writePixels|fillRect|drawRect|fillScreen|print))\b"
        )
        raw_mid_tx_funcs = {
            "DisplayEmu::streamPixelRow",
            "DisplayEmu::pushPixelsRaw",
            "DisplayEmu::writeWindowBytes",
            "streamPixelRow",
            "pushPixelsRaw",
            "writeWindowBytes",
        }
        findings: list[LintFinding] = []
        cur_fn: str | None = None
        fn_start = 0
        fn_buf: list[str] = []
        fn_depth = 0
        for idx, raw_line in enumerate(lines, 1):
            clean = raw_line.split("//", 1)[0]
            if cur_fn is None:
                m_fn = fn_open_re.match(clean)
                if m_fn and not clean.lstrip().startswith(("if", "for", "while", "switch", "else", "namespace", "struct", "class")):
                    cur_fn = m_fn.group("name")
                    fn_start = idx
                    fn_buf = [clean]
                    fn_depth = clean.count("{") - clean.count("}")
                    if fn_depth <= 0:
                        cur_fn = None
                continue
            fn_buf.append(clean)
            fn_depth += clean.count("{") - clean.count("}")
            if fn_depth <= 0:
                body = "\n".join(fn_buf)
                if (
                    cur_fn not in raw_mid_tx_funcs
                    and io_re.search(body)
                    and "SpiArbiter::ScopedLock" not in body
                    and "SpiArbiter::lock()" not in body
                ):
                    findings.append(
                        LintFinding(
                            rule_id="UNARBITRATED_SD_IO_IN_FUNC",
                            severity="CRITICAL" if "::" in cur_fn else "WARNING",
                            file_path=path,
                            line_number=fn_start,
                            line_content=f"{cur_fn}() touches SD/SPI/TFT without SpiArbiter",
                            message=f"{cur_fn}() executes SPI/SD/TFT I/O without acquiring SpiArbiter::ScopedLock inside the function.",
                            suggested_fix="Add `SpiArbiter::ScopedLock spiLock;` inside the function body.",
                        )
                    )
                cur_fn = None
        return findings

    def _check_misc_safety_rules(self, path: Path, rel_path_str: str, lines: list[str]) -> list[LintFinding]:
        findings: list[LintFinding] = []
        scanline_re = re.compile(r"uint16_t\s+[a-zA-Z0-9_]+\[\s*(240|256|320|480)\s*\]")
        unaligned_re = re.compile(r"\*\s*\(\s*uint16_t\s*\*\s*\)\s*&?[a-zA-Z0-9_]+\[")
        is_core_or_vendor = "src/cores/" in rel_path_str or "src/third_party/" in rel_path_str
        is_hot_cpp = any(seg in rel_path_str for seg in ("src/app/", "src/ui/", "src/cores/")) and rel_path_str.endswith(".cpp")
        alloc_err_re = re.compile(
            r"(malloc|calloc|realloc|Doom_MallocPSRAM|heap_caps_malloc|default_ram\s*<\s*min_ram|newlumpinfo\s*==\s*NULL|new_open_wadfiles\s*==\s*NULL|rover\s*==\s*start|Unable to allocate|Z_Malloc:\s*failed|Couldn't realloc)"
        )
        for idx, raw_line in enumerate(lines, 1):
            clean = raw_line.strip()
            if scanline_re.search(clean) and raw_line.startswith(("  ", "\t")) and "static" not in clean:
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
            if clean.startswith(("//", "/*", "*")):
                continue
            if is_core_or_vendor and unaligned_re.search(clean) and ("gb_rom_read" in clean or "rom" in clean.lower()):
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
            stripped_code = raw_line.split("//", 1)[0].strip()
            if is_hot_cpp and stripped_code and not stripped_code.startswith(("/*", "*", "#")):
                if re.search(r"\b(?:delay|ets_delay_us)\s*\(", stripped_code):
                    findings.append(
                        LintFinding(
                            rule_id="BLOCKING_DELAY_IN_FIRMWARE",
                            severity="WARNING",
                            file_path=path,
                            line_number=idx,
                            line_content=stripped_code,
                            message="Blocking delay() / ets_delay_us() stalls Core 1 / RTOS responsiveness.",
                            suggested_fix="Replace with non-blocking deadline timers or vTaskDelayUntil.",
                        )
                    )
                if "src/cores/" in rel_path_str and re.search(r"\bstd::(?:string|vector)\b", stripped_code):
                    findings.append(
                        LintFinding(
                            rule_id="INTERNAL_DRAM_STL_IN_CORE",
                            severity="WARNING",
                            file_path=path,
                            line_number=idx,
                            line_content=stripped_code,
                            message="std::string / std::vector allocates from internal DRAM instead of fixed/PSRAM pools.",
                            suggested_fix="Use fixed char/struct buffers or explicit bmo_psram_malloc buffers.",
                        )
                    )
            if re.search(r"\bexit\s*\(", clean):
                findings.append(
                    LintFinding(
                        rule_id="EXIT_OR_ERROR_ON_ALLOC_FAILURE",
                        severity="CRITICAL",
                        file_path=path,
                        line_number=idx,
                        line_content=clean,
                        message="Reachable exit() call in embedded firmware terminates RTOS execution.",
                        suggested_fix="Return false or unwind via Doom_Bailout / setjmp recovery.",
                    )
                )
            if re.search(r"\bI_Error\s*\(", clean):
                window = " ".join(src_line.strip() for src_line in lines[max(0, idx - 5) : idx])
                if alloc_err_re.search(window + " " + clean):
                    findings.append(
                        LintFinding(
                            rule_id="EXIT_OR_ERROR_ON_ALLOC_FAILURE",
                            severity="CRITICAL",
                            file_path=path,
                            line_number=idx,
                            line_content=clean,
                            message="Reachable I_Error() on memory allocation failure instead of non-fatal recovery.",
                            suggested_fix="Return NULL/false or unwind via Doom_Bailout so begin() returns false cleanly.",
                        )
                    )
        return findings

    def lint_file(self, path: Path) -> list[LintFinding]:
        try:
            content = path.read_text(encoding="utf-8", errors="ignore")
        except Exception:
            return []
        lines = content.splitlines()
        try:
            rel_path = path.relative_to(self.firmware_dir)
        except ValueError:
            rel_path = path
        rel_path_str = str(rel_path).replace("\\", "/")

        findings: list[LintFinding] = []
        findings.extend(self._check_local_stack_arrays(path, rel_path_str, lines))
        findings.extend(self._check_core_wrapper_rules(path, rel_path_str, lines))
        findings.extend(self._check_malloc_and_sd_rules(path, rel_path_str, lines))
        findings.extend(self._check_app_and_hardware_rules(path, rel_path_str, lines))
        findings.extend(self._check_spi_arbiter_functions(path, rel_path_str, lines))
        findings.extend(self._check_misc_safety_rules(path, rel_path_str, lines))
        return findings
