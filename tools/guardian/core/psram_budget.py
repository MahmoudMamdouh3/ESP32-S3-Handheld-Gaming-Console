# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""
tools.guardian.core.psram_budget — Source-Derived PSRAM Allocation & DOOM Zone Budget Analyzer
Extracts persistent and transient PSRAM allocations directly from firmware C/C++ source files
and verifies that DOOM zone sizing fits within the 8 MiB Octal PSRAM ceiling across all catalog sizes.
"""

import re
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

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
    persistent_sites: list[PsramAllocationSite]
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

    def _locate_symbol_site(self, pattern: str, fallback_rel: str) -> tuple[str, int]:
        """Dynamically locate an allocation symbol across first-party firmware sources."""
        src_root = self.fw_dir / "src"
        compiled = re.compile(pattern)
        for sub in ("storage", "app", "ui", "hal", "cores"):
            sub_dir = src_root / sub
            if not sub_dir.is_dir():
                continue
            for candidate in sorted(sub_dir.glob("*.cpp")) + sorted(sub_dir.glob("*.h")):
                lines = candidate.read_text(encoding="utf-8", errors="ignore").splitlines()
                for idx, line in enumerate(lines, 1):
                    if compiled.search(line):
                        return candidate.relative_to(self.repo_root).as_posix(), idx
        return fallback_rel, 1

    def _extract_constants(self) -> dict[str, int]:
        sd_h = (self.fw_dir / "src" / "storage" / "sd_card.h").read_text(encoding="utf-8", errors="ignore")
        zone_h = (self.fw_dir / "src" / "cores" / "psram_zone_budget.h").read_text(encoding="utf-8", errors="ignore")
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
            match = re.fullmatch(
                r"((?:const\s+)?[A-Za-z_][A-Za-z_0-9]*(?:\s*\*+)?)\s+[A-Za-z_][A-Za-z_0-9]*(?:\s*\[\s*(\d+)\s*\])?\s*;",
                line,
            )
            if not match:
                raise ValueError(f"Unsupported RomFile field declaration: {line}")
            field_type, count_text = match.groups()
            normalized_type = " ".join(field_type.split())
            if normalized_type.endswith("*"):
                unit_size, alignment = (4, 4)
            else:
                unit_size, alignment = sizes.get(normalized_type, (0, 0))
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

    def _collect_persistent_sites(self, consts: dict[str, int], pool_2k: int) -> list[PsramAllocationSite]:
        min_pool = consts["min_pool"]
        max_pool = consts["max_pool"]
        rom_sz = consts["rom_file_sizeof"]
        specs: tuple[tuple[str, str, str, int, str, int, int, int, str], ...] = (
            ("SDCard::romList", r"heap_caps_malloc\(sizeof\(RomFile\)", "firmware/BmoGameboy/src/storage/sd_mount.cpp",
             rom_sz, "ceil(catalog * 1.25), clamped to [256..36000]", min_pool * rom_sz, pool_2k * rom_sz, max_pool * rom_sz,
             "Persistent (dynamic pool)"),
            ("SDCard::s_pathArena", r"s_pathArena\s*=\s*static_cast", "firmware/BmoGameboy/src/storage/sd_mount.cpp",
             1, "Pow2 path arena (16 KiB..4 MiB) or exact stringTableBytes", 16384, 131072, 4194304,
             "Persistent (dynamic pool)"),
            ("s_visibleRomIndexes", r"psramIndexes\s*=", "firmware/BmoGameboy/src/app/app_state.cpp",
             4, "poolCap * sizeof(int) (ESP32-S3: 4 B)", min_pool * 4, pool_2k * 4, max_pool * 4,
             "Persistent (dynamic pool)"),
            ("s_visibleGames", r"psramGames\s*=", "firmware/BmoGameboy/src/app/app_state.cpp",
             4, "poolCap * sizeof(RomFile*) (ESP32-S3: 4 B)", min_pool * 4, pool_2k * 4, max_pool * 4,
             "Persistent (dynamic pool)"),
            ("s_asyncEmuBuf[0..1]", r"s_asyncEmuBuf\[0\]\s*=", "firmware/BmoGameboy/src/ui/display_pipeline.cpp",
             320 * 240 * 2, "2 * (320 * 240 * 2)", 2 * 320 * 240 * 2, 2 * 320 * 240 * 2, 2 * 320 * 240 * 2,
             "Persistent (boot)"),
            ("s_menuCanvasArr[0..1]", r"s_menuCanvasArr\[0\]\s*=", "firmware/BmoGameboy/src/ui/display_pipeline.cpp",
             320 * 240 * 2, "2 * (320 * 240 * 2)", 2 * 320 * 240 * 2, 2 * 320 * 240 * 2, 2 * 320 * 240 * 2,
             "Persistent (cached UI)"),
            ("s_psramFaceBuf", r"(?:s_psramFaceBuf|psramFaceBuf)\s*=", "firmware/BmoGameboy/src/ui/bmo_face_anim.cpp",
             320 * 240 * 2, "1 * (320 * 240 * 2)", 320 * 240 * 2, 320 * 240 * 2, 320 * 240 * 2,
             "Persistent (boot)"),
            ("s_boxArtBuf", r"s_boxArtBuf\s*=", "firmware/BmoGameboy/src/storage/box_art.cpp",
             64 * 64 * 2, "1 * (64 * 64 * 2)", 64 * 64 * 2, 64 * 64 * 2, 64 * 64 * 2,
             "Persistent (first art load)"),
        )
        sites: list[PsramAllocationSite] = []
        for sym, pat, rel_file, unit_b, formula, b_floor, b_2k, b_max, pers in specs:
            f_path, line_num = self._locate_symbol_site(pat, rel_file)
            sites.append(
                PsramAllocationSite(
                    symbol=sym, file_path=f_path, line_number=line_num, unit_bytes=unit_b,
                    count_formula=formula, bytes_at_floor=b_floor, bytes_at_2k=b_2k, bytes_at_max=b_max, persistence=pers,
                )
            )
        return sites

    def analyze(self) -> PsramBudgetReport:
        consts = self._extract_constants()
        min_pool = consts["min_pool"]
        max_pool = consts["max_pool"]
        pool_2k = self.compute_pool_capacity(2000, min_pool, max_pool)
        sites = self._collect_persistent_sites(consts, pool_2k)

        tot_floor = sum(s.bytes_at_floor for s in sites)
        tot_2k = sum(s.bytes_at_2k for s in sites)
        tot_max = sum(s.bytes_at_max for s in sites)

        free_floor = PSRAM_TOTAL_BYTES - tot_floor
        free_2k = PSRAM_TOTAL_BYTES - tot_2k
        free_max = PSRAM_TOTAL_BYTES - tot_max

        return PsramBudgetReport(
            total_psram_bytes=PSRAM_TOTAL_BYTES,
            min_rom_pool=min_pool,
            max_rom_pool=max_pool,
            rom_file_sizeof=consts["rom_file_sizeof"],
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
            doom_zone_floor_catalog=self.compute_doom_zone_bytes(
                free_floor, consts["doom_margin"], consts["doom_floor"], consts["doom_cap"]
            ),
            doom_zone_2k_catalog=self.compute_doom_zone_bytes(
                free_2k, consts["doom_margin"], consts["doom_floor"], consts["doom_cap"]
            ),
            doom_zone_max_catalog=self.compute_doom_zone_bytes(
                free_max, consts["doom_margin"], consts["doom_floor"], consts["doom_cap"]
            ),
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

    def to_dict(self) -> dict[str, Any]:
        return asdict(self.analyze())
