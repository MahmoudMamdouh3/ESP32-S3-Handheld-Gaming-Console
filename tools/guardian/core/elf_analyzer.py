# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""
tools.guardian.core.elf_analyzer — Xtensa ESP32-S3 ELF Binary Symbol & Section Introspection
Parses compiled firmware ELF binaries to extract symbol sizes, memory section breakdowns,
IRAM vs DRAM vs Flash placement, and identify memory consumers.
"""

import os
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path


@dataclass
class SymbolEntry:
    address: int
    size: int
    sym_type: str  # T (text), D (data), B (bss), R (rodata), etc.
    name: str
    section: str


@dataclass
class SectionEntry:
    name: str
    size: int
    vma: int
    memory_type: str  # IRAM, DRAM, FLASH_TEXT, FLASH_RODATA, PSRAM, RTC, DUMMY


@dataclass
class ElfAnalysisResult:
    elf_path: Path | None
    sections: list[SectionEntry]
    symbols_by_size: list[SymbolEntry]
    iram_used_bytes: int
    iram_total_bytes: int
    dram_used_bytes: int
    dram_total_bytes: int
    flash_used_bytes: int
    flash_total_bytes: int
    psram_static_bytes: int
    largest_functions: list[SymbolEntry]
    largest_data_objects: list[SymbolEntry]
    toolchain_found: bool


class ElfAnalyzer:
    """Introspects Xtensa ESP32-S3 ELF binaries using GCC toolchain binaries."""

    def __init__(self, repo_root: Path):
        self.repo_root = repo_root
        self.toolchain_bin = self._find_toolchain_dir()

    def _find_toolchain_dir(self) -> Path | None:
        data_dirs = []
        configured_data_dir = os.environ.get("ARDUINO_DIRECTORIES_DATA")
        if configured_data_dir:
            data_dirs.append(Path(configured_data_dir))

        local_appdata = os.environ.get("LOCALAPPDATA")
        if local_appdata:
            data_dirs.append(Path(local_appdata) / "Arduino15")
        user_profile = os.environ.get("USERPROFILE")
        if user_profile:
            data_dirs.append(Path(user_profile) / ".arduino15")
        data_dirs.append(Path.home() / ".arduino15")

        candidates = []
        for data_dir in dict.fromkeys(data_dirs):
            esp_x32_root = data_dir / "packages" / "esp32" / "tools" / "esp-x32"
            if esp_x32_root.exists():
                version_bins = sorted(
                    (p / "bin" for p in esp_x32_root.iterdir() if p.is_dir()),
                    reverse=True,
                )
                candidates.extend(version_bins)
            candidates.append(esp_x32_root / "2601" / "bin")
        for c in candidates:
            if c.exists() and (c / "xtensa-esp32s3-elf-size.exe").exists():
                return c
            if c.exists() and (c / "xtensa-esp32s3-elf-size").exists():
                return c
        return None

    def get_tool(self, name: str) -> str | None:
        if self.toolchain_bin:
            tool_exe = self.toolchain_bin / f"xtensa-esp32s3-elf-{name}.exe"
            if tool_exe.exists():
                return str(tool_exe)
            tool_no_exe = self.toolchain_bin / f"xtensa-esp32s3-elf-{name}"
            if tool_no_exe.exists():
                return str(tool_no_exe)
        found = shutil.which(f"xtensa-esp32s3-elf-{name}")
        if found:
            return found
        return None

    def find_latest_elf(self) -> Path | None:
        search_dirs = [
            self.repo_root / "build",
            self.repo_root / "firmware" / "BmoGameboy" / "build",
            Path(os.environ.get("LOCALAPPDATA", "")) / "arduino" / "sketches",
            Path(os.environ.get("TEMP", "")) / "arduino",
            Path(os.environ.get("TEMP", "")) / "arduino-sketch",
        ]
        best_elf: Path | None = None
        best_mtime: float = 0.0

        for s_dir in search_dirs:
            if not s_dir.exists():
                continue
            try:
                for elf_file in s_dir.rglob("*.elf"):
                    if "BmoGameboy" in elf_file.name:
                        mtime = elf_file.stat().st_mtime
                        if mtime > best_mtime:
                            best_mtime = mtime
                            best_elf = elf_file
            except Exception:
                continue

        return best_elf

    def _classify_section(self, name: str) -> str:
        if "dummy" in name:
            return "DUMMY"
        if ".iram0" in name:
            return "IRAM"
        if ".dram0" in name or name == ".noinit":
            return "DRAM"
        if ".flash.text" in name:
            return "FLASH_TEXT"
        if ".flash.rodata" in name or ".flash.appdesc" in name or name == ".eh_frame":
            return "FLASH_RODATA"
        if ".ext_ram" in name:
            return "PSRAM"
        if ".rtc" in name:
            return "RTC"
        return "UNKNOWN"

    def _parse_sections(self, size_tool: str, target_elf: Path) -> list[SectionEntry]:
        sections: list[SectionEntry] = []
        try:
            out = subprocess.check_output([size_tool, "-A", str(target_elf)], text=True, errors="ignore")
        except Exception:
            return sections
        for line in out.splitlines():
            parts = line.split()
            if len(parts) >= 3 and parts[1].isdigit():
                s_name = parts[0]
                s_size = int(parts[1])
                s_vma = int(parts[2]) if parts[2].isdigit() else 0
                sections.append(SectionEntry(name=s_name, size=s_size, vma=s_vma, memory_type=self._classify_section(s_name)))
        return sections

    def _parse_symbols(self, nm_tool: str, target_elf: Path) -> list[SymbolEntry]:
        symbols: list[SymbolEntry] = []
        try:
            out = subprocess.check_output([nm_tool, "-S", "-C", "--size-sort", str(target_elf)], text=True, errors="ignore")
        except Exception:
            return symbols
        sec_map = {"T": "text", "W": "text", "D": "data_or_bss", "B": "data_or_bss", "R": "rodata"}
        for line in out.splitlines():
            parts = line.split(maxsplit=3)
            if len(parts) >= 4:
                sym_t = parts[2]
                symbols.append(
                    SymbolEntry(
                        address=int(parts[0], 16),
                        size=int(parts[1], 16),
                        sym_type=sym_t,
                        name=parts[3],
                        section=sec_map.get(sym_t.upper(), "unknown"),
                    )
                )
        return symbols

    def analyze(self, elf_path: Path | None = None) -> ElfAnalysisResult:
        target_elf = elf_path or self.find_latest_elf()
        size_tool = self.get_tool("size")
        toolchain_found = size_tool is not None

        if not target_elf or not target_elf.exists():
            raise FileNotFoundError(
                f"Compiled ESP32-S3 ELF binary not found ({target_elf or 'no BmoGameboy*.elf discovered under build/'}). "
                "Compile firmware first (e.g. `python tasks.py build --profile default`)."
            )
        if not size_tool:
            raise RuntimeError(
                "xtensa-esp32s3-elf-size toolchain binary not found in PATH or Arduino15 esp32 tools."
            )

        sections = self._parse_sections(size_tool, target_elf)
        nm_tool = self.get_tool("nm")
        symbols = self._parse_symbols(nm_tool, target_elf) if nm_tool else []

        symbols_sorted = sorted(symbols, key=lambda s: s.size, reverse=True)
        largest_fn = [s for s in symbols_sorted if s.section == "text"][:20]
        largest_data = [s for s in symbols_sorted if s.section in {"data_or_bss", "rodata"}][:20]

        iram_used = sum(s.size for s in sections if s.memory_type == "IRAM")
        dram_used = sum(s.size for s in sections if s.memory_type == "DRAM")
        dram_data = sum(s.size for s in sections if s.name == ".dram0.data")
        flash_used = (
            sum(s.size for s in sections if s.memory_type in {"FLASH_TEXT", "FLASH_RODATA"})
            + iram_used
            + dram_data
        )
        psram_used = sum(s.size for s in sections if s.memory_type == "PSRAM")

        return ElfAnalysisResult(
            elf_path=target_elf,
            sections=sections,
            symbols_by_size=symbols_sorted,
            iram_used_bytes=iram_used,
            iram_total_bytes=327680,
            dram_used_bytes=dram_used,
            dram_total_bytes=327680,
            flash_used_bytes=flash_used,
            flash_total_bytes=16777216,
            psram_static_bytes=psram_used,
            largest_functions=largest_fn,
            largest_data_objects=largest_data,
            toolchain_found=toolchain_found,
        )
