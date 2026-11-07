"""
tools.guardian.core.psram_budget — Source-Derived PSRAM Allocation & DOOM Zone Budget Analyzer
Extracts persistent and transient PSRAM allocations directly from firmware C/C++ source files
and verifies that DOOM zone sizing fits within the 8 MiB Octal PSRAM ceiling across all catalog sizes.
"""

import re
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Dict, List


PSRAM_TOTAL_BYTES = 8 * 1024 * 1024  # 8,388,608 B (ESP32-S3-WROOM-1-N16R8 Octal PSRAM)


@dataclass
class PsramAllocationSite:
    symbol: str
    file_path: str
    line_number: int
    unit_bytes: int
    count_formula: str
    bytes_at_floor: int
    bytes_at_2k: int
    bytes_at_max: int
    persistence: str


@dataclass
class PsramBudgetReport:
    total_psram_bytes: int
    min_rom_pool: int
    max_rom_pool: int
    rom_file_sizeof: int
    doom_margin_bytes: int
    doom_floor_bytes: int
    doom_cap_bytes: int
    persistent_sites: List[PsramAllocationSite]
    persistent_total_floor: int
    persistent_total_2k: int
    persistent_total_max: int
    free_after_boot_floor: int
    free_after_boot_2k: int
    free_after_boot_max: int
    doom_zone_floor_catalog: int
    doom_zone_2k_catalog: int
    doom_zone_max_catalog: int


class PsramBudgetAnalyzer:
    """Parses firmware declarations to calculate the persistent PSRAM ledger."""

    def __init__(self, repo_root: Path):
        self.repo_root = repo_root
        self.fw_dir = repo_root / "firmware" / "BmoGameboy"

    @staticmethod
    def compute_pool_capacity(catalog_count: int, min_pool: int = 256, max_pool: int = 36000) -> int:
        if catalog_count < 0:
            catalog_count = 0
        if catalog_count >= max_pool:
            return max_pool
        cap = catalog_count + (catalog_count // 4) + (1 if catalog_count % 4 else 0)
        if cap < min_pool:
            cap = min_pool
        if cap > max_pool:
            cap = max_pool
        return cap

    @staticmethod
    def compute_doom_zone_bytes(
        largest_free_block: int,
        margin_bytes: int = 512 * 1024,
        floor_bytes: int = 2560 * 1024,
        cap_bytes: int = 6144 * 1024,
    ) -> int:
        if largest_free_block < floor_bytes + margin_bytes:
            return 0
        candidate = largest_free_block - margin_bytes
        candidate &= ~65535
        if candidate < floor_bytes:
            return 0
        if candidate > cap_bytes:
            candidate = cap_bytes
        return candidate

    def _find_line(self, rel_path: str, pattern: str) -> int:
        p = self.repo_root / rel_path
        if not p.exists():
            return 1
        for idx, line in enumerate(p.read_text(encoding="utf-8", errors="ignore").splitlines(), 1):
            if re.search(pattern, line):
                return idx
        return 1

    def _extract_constants(self) -> Dict[str, int]:
        sd_h = (self.fw_dir / "src" / "core" / "sd_card.h").read_text(encoding="utf-8", errors="ignore")
        zone_h = (self.fw_dir / "src" / "emulators" / "psram_zone_budget.h").read_text(encoding="utf-8", errors="ignore")
        rom_file_sizeof = self._extract_rom_file_sizeof(sd_h)

        m_min = re.search(r"MIN_ROM_POOL\s*=\s*(\d+)", sd_h)
        m_max = re.search(r"MAX_ROM_POOL\s*=\s*(\d+)", sd_h)

        min_pool = int(m_min.group(1)) if m_min else 256
        max_pool = int(m_max.group(1)) if m_max else 36000

        def _parse_kb_expr(text: str, name: str, default_kb: int) -> int:
            m = re.search(rf"{name}\s*=\s*(\d+)\s*\*\s*1024", text)
            return int(m.group(1)) * 1024 if m else default_kb * 1024

        margin_b = _parse_kb_expr(zone_h, "DOOM_ZONE_MARGIN_BYTES", 512)
        floor_b = _parse_kb_expr(zone_h, "DOOM_ZONE_FLOOR_BYTES", 2560)
        cap_b = _parse_kb_expr(zone_h, "DOOM_ZONE_CAP_BYTES", 6144)

        return {
            "min_pool": min_pool,
            "max_pool": max_pool,
            "rom_file_sizeof": rom_file_sizeof,
            "doom_margin": margin_b,
            "doom_floor": floor_b,
            "doom_cap": cap_b,
        }

    @staticmethod
    def _extract_rom_file_sizeof(sd_header: str) -> int:
        """Compute RomFile's ESP32 ABI size from its declared fields.

        Unsupported field types fail closed so the report cannot silently keep
        an old size after the firmware layout changes.
        """
        enum_match = re.search(r"\benum\s+RomType(?:\s*:\s*(\w+))?\s*\{", sd_header)
        struct_match = re.search(r"\bstruct\s+RomFile\s*\{([^}]+)\}", sd_header, re.S)
        if not enum_match or not struct_match:
            raise ValueError("Could not locate RomType and RomFile declarations in sd_card.h")

        enum_type = enum_match.group(1) or "int"
        sizes = {"char": (1, 1), "bool": (1, 1), "uint8_t": (1, 1),
                 "int8_t": (1, 1), "uint16_t": (2, 2), "int16_t": (2, 2),
                 "uint32_t": (4, 4), "int32_t": (4, 4), "int": (4, 4)}
        sizes["RomType"] = sizes.get(enum_type, (0, 0))
        offset = 0
        max_alignment = 1
        fields = 0
        for raw_line in struct_match.group(1).splitlines():
            line = raw_line.split("//", 1)[0].strip()
            if not line:
                continue
            match = re.fullmatch(r"([A-Za-z_][A-Za-z_0-9]*)\s+[A-Za-z_][A-Za-z_0-9]*(?:\s*\[\s*(\d+)\s*\])?\s*;", line)
            if not match:
                raise ValueError(f"Unsupported RomFile field declaration: {line}")
            field_type, count_text = match.groups()
            unit_size, alignment = sizes.get(field_type, (0, 0))
            if not unit_size:
                raise ValueError(f"Unsupported RomFile field type: {field_type}")
            count = int(count_text) if count_text else 1
            offset = (offset + alignment - 1) // alignment * alignment
            offset += unit_size * count
            max_alignment = max(max_alignment, alignment)
            fields += 1
        if fields == 0:
            raise ValueError("RomFile declaration contains no supported fields")
        return (offset + max_alignment - 1) // max_alignment * max_alignment

    def analyze(self) -> PsramBudgetReport:
        consts = self._extract_constants()
        min_pool = consts["min_pool"]
        max_pool = consts["max_pool"]
        pool_2k = self.compute_pool_capacity(2000, min_pool, max_pool)
        rom_sz = consts["rom_file_sizeof"]

        sites: List[PsramAllocationSite] = [
            PsramAllocationSite(
                symbol="SDCard::romList",
                file_path="firmware/BmoGameboy/src/core/sd_card.cpp",
                line_number=self._find_line("firmware/BmoGameboy/src/core/sd_card.cpp", r"heap_caps_malloc\(sizeof\(RomFile\)"),
                unit_bytes=rom_sz,
                count_formula="ceil(catalog * 1.25), clamped to [256..36000]",
                bytes_at_floor=min_pool * rom_sz,
                bytes_at_2k=pool_2k * rom_sz,
                bytes_at_max=max_pool * rom_sz,
                persistence="Persistent (dynamic pool)",
            ),
            PsramAllocationSite(
                symbol="visibleRomIndexes",
                file_path="firmware/BmoGameboy/BmoGameboy.ino",
                line_number=self._find_line("firmware/BmoGameboy/BmoGameboy.ino", r"psramIndexes\s*=.*heap_caps_malloc"),
                unit_bytes=4,
                count_formula="poolCap * sizeof(int) (ESP32-S3: 4 B)",
                bytes_at_floor=min_pool * 4,
                bytes_at_2k=pool_2k * 4,
                bytes_at_max=max_pool * 4,
                persistence="Persistent (dynamic pool)",
            ),
            PsramAllocationSite(
                symbol="visibleGames",
                file_path="firmware/BmoGameboy/BmoGameboy.ino",
                line_number=self._find_line("firmware/BmoGameboy/BmoGameboy.ino", r"psramGames\s*=.*heap_caps_malloc"),
                unit_bytes=4,
                count_formula="poolCap * sizeof(RomFile*) (ESP32-S3: 4 B)",
                bytes_at_floor=min_pool * 4,
                bytes_at_2k=pool_2k * 4,
                bytes_at_max=max_pool * 4,
                persistence="Persistent (dynamic pool)",
            ),
            PsramAllocationSite(
                symbol="s_asyncEmuBuf[0..1]",
                file_path="firmware/BmoGameboy/src/core/display_emu.cpp",
                line_number=self._find_line("firmware/BmoGameboy/src/core/display_emu.cpp", r"s_asyncEmuBuf\[0\]\s*="),
                unit_bytes=320 * 240 * 2,
                count_formula="2 * (320 * 240 * 2)",
                bytes_at_floor=2 * 320 * 240 * 2,
                bytes_at_2k=2 * 320 * 240 * 2,
                bytes_at_max=2 * 320 * 240 * 2,
                persistence="Persistent (boot)",
            ),
            PsramAllocationSite(
                symbol="menuCanvasArr[0..1]",
                file_path="firmware/BmoGameboy/src/core/display_emu.cpp",
                line_number=self._find_line("firmware/BmoGameboy/src/core/display_emu.cpp", r"menuCanvasArr\[0\]\s*="),
                unit_bytes=320 * 240 * 2,
                count_formula="2 * (320 * 240 * 2)",
                bytes_at_floor=2 * 320 * 240 * 2,
                bytes_at_2k=2 * 320 * 240 * 2,
                bytes_at_max=2 * 320 * 240 * 2,
                persistence="Persistent (cached UI)",
            ),
            PsramAllocationSite(
                symbol="s_psramFaceBuf",
                file_path="firmware/BmoGameboy/src/core/bmo_face.cpp",
                line_number=self._find_line("firmware/BmoGameboy/src/core/bmo_face.cpp", r"s_psramFaceBuf\s*="),
                unit_bytes=320 * 240 * 2,
                count_formula="1 * (320 * 240 * 2)",
                bytes_at_floor=320 * 240 * 2,
                bytes_at_2k=320 * 240 * 2,
                bytes_at_max=320 * 240 * 2,
                persistence="Persistent (boot)",
            ),
            PsramAllocationSite(
                symbol="s_boxArtBuf",
                file_path="firmware/BmoGameboy/src/core/box_art.cpp",
                line_number=self._find_line("firmware/BmoGameboy/src/core/box_art.cpp", r"s_boxArtBuf\s*="),
                unit_bytes=64 * 64 * 2,
                count_formula="1 * (64 * 64 * 2)",
                bytes_at_floor=64 * 64 * 2,
                bytes_at_2k=64 * 64 * 2,
                bytes_at_max=64 * 64 * 2,
                persistence="Persistent (first art load)",
            ),
        ]

        tot_floor = sum(s.bytes_at_floor for s in sites)
        tot_2k = sum(s.bytes_at_2k for s in sites)
        tot_max = sum(s.bytes_at_max for s in sites)

        free_floor = PSRAM_TOTAL_BYTES - tot_floor
        free_2k = PSRAM_TOTAL_BYTES - tot_2k
        free_max = PSRAM_TOTAL_BYTES - tot_max

        dz_floor = self.compute_doom_zone_bytes(
            free_floor, consts["doom_margin"], consts["doom_floor"], consts["doom_cap"]
        )
        dz_2k = self.compute_doom_zone_bytes(
            free_2k, consts["doom_margin"], consts["doom_floor"], consts["doom_cap"]
        )
        dz_max = self.compute_doom_zone_bytes(
            free_max, consts["doom_margin"], consts["doom_floor"], consts["doom_cap"]
        )

        return PsramBudgetReport(
            total_psram_bytes=PSRAM_TOTAL_BYTES,
            min_rom_pool=min_pool,
            max_rom_pool=max_pool,
            rom_file_sizeof=rom_sz,
            doom_margin_bytes=consts["doom_margin"],
            doom_floor_bytes=consts["doom_floor"],
            doom_cap_bytes=consts["doom_cap"],
            persistent_sites=sites,
            persistent_total_floor=tot_floor,
            persistent_total_2k=tot_2k,
            persistent_total_max=tot_max,
            free_after_boot_floor=free_floor,
            free_after_boot_2k=free_2k,
            free_after_boot_max=free_max,
            doom_zone_floor_catalog=dz_floor,
            doom_zone_2k_catalog=dz_2k,
            doom_zone_max_catalog=dz_max,
        )

    def format_markdown(self) -> str:
        rep = self.analyze()
        lines = [
            "<!-- GENERATED:BEGIN psram-budget -->",
            "| Symbol | Source Location | Formula | Floor (256 slots) | Typical (2,500 slots) | Max Cap (36,000 slots) |",
            "| :--- | :--- | :--- | ---: | ---: | ---: |",
        ]
        for s in rep.persistent_sites:
            loc = f"`{s.file_path}:{s.line_number}`"
            lines.append(
                f"| `{s.symbol}` | {loc} | `{s.count_formula}` | {s.bytes_at_floor:,} B | {s.bytes_at_2k:,} B | {s.bytes_at_max:,} B |"
            )
        lines.extend(
            [
                f"| **Persistent Total** | — | — | **{rep.persistent_total_floor:,} B** | **{rep.persistent_total_2k:,} B** | **{rep.persistent_total_max:,} B** |",
                f"| **Free PSRAM (`8,388,608 B` total)** | — | `8,388,608 - Persistent` | **{rep.free_after_boot_floor:,} B** | **{rep.free_after_boot_2k:,} B** | **{rep.free_after_boot_max:,} B** |",
                f"| **DOOM zone upper bound** | `psram_zone_budget.h` | `clamp(total-free - {rep.doom_margin_bytes:,} B, {rep.doom_floor_bytes:,}..{rep.doom_cap_bytes:,} B)` | **{rep.doom_zone_floor_catalog:,} B** | **{rep.doom_zone_2k_catalog:,} B** | **{rep.doom_zone_max_catalog:,} B** |",
                "| **Runtime allocation** | `heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)` | Uses largest contiguous block; source ledger cannot predict fragmentation | Checked at launch | Checked at launch | Checked at launch |",
                "Actual DOOM zone size is computed from the measured largest free block at launch. The table's zone row is only an upper bound based on aggregate free PSRAM.",
                "<!-- GENERATED:END psram-budget -->",
            ]
        )
        return "\n".join(lines)

    def update_rule_file(self) -> None:
        rule_path = self.repo_root / ".agents" / "rules" / "15_performance_budgets.md"
        text = rule_path.read_text(encoding="utf-8")
        begin = "<!-- GENERATED:BEGIN psram-budget -->"
        end = "<!-- GENERATED:END psram-budget -->"
        generated = self.format_markdown()
        if begin in text and end in text:
            before, remainder = text.split(begin, 1)
            _, after = remainder.split(end, 1)
            updated = before + generated + after
        else:
            marker = "### PSRAM Allocation Ledger (8MB Total Octal PSRAM)"
            if marker not in text:
                raise ValueError(f"Could not locate PSRAM ledger heading in {rule_path}")
            before, after = text.split(marker, 1)
            next_section = "\n- **DRAM budget:**"
            if next_section not in after:
                raise ValueError(f"Could not locate the section following the PSRAM ledger in {rule_path}")
            _, tail = after.split(next_section, 1)
            updated = before + marker + "\n" + generated + next_section + tail
        rule_path.write_text(updated, encoding="utf-8", newline="\n")

    def to_dict(self) -> dict:
        return asdict(self.analyze())
