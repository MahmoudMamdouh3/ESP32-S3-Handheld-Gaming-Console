"""
tools.guardian.core.doc_verifier — Cross-Document Hardware, Symbol, and Architecture Truth Verifier
Ensures that Markdown documentation and AI rules never drift out of sync with C/C++ firmware source truth.
"""

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List


@dataclass
class DocDriftIssue:
    file_path: str
    line_number: int
    category: str  # PIN_DRIFT, STALE_PATH, STALE_ARCHITECTURE
    message: str


class DocTruthVerifier:
    """Verifies that README.md, docs/, and .agents/rules/ match live firmware source truth."""

    PIN_MACROS = {
        "TFT_SCK", "TFT_MOSI", "TFT_RST", "TFT_DC", "TFT_CS",
        "SD_MISO", "SD_CS",
        "BTN_UP", "BTN_DOWN", "BTN_LEFT", "BTN_RIGHT",
        "BTN_A", "BTN_B", "BTN_START", "BTN_SELECT",
        "BATTERY_ADC_PIN", "I2S_BCLK", "I2S_LRC", "I2S_DIN",
        "I2C_SDA", "I2C_SCL",
        "FEATURE_SD_CARD", "FEATURE_AUDIO", "FEATURE_BATTERY_MONITOR",
    }

    FORBIDDEN_STALE_REFERENCES = [
        ("IMPLEMENTATION_PLAN.md", "Deleted legacy root plan; use .agents/rules/42_quality_of_life_and_roadmap_ledger.md"),
        ("report_guardian.md", "Deleted root report; use docs/performance_report.md"),
        ("tools/convert.py", "Deleted duplicate script; use scripts/convert_roms.py"),
        ("zig-windows-x86_64", "Removed Zig binary dependency; native host tests use MSVC cl.exe"),
    ]

    def __init__(self, repo_root: Path):
        self.repo_root = repo_root
        self.firmware_dir = repo_root / "firmware" / "BmoGameboy"
        self.config_path = self.firmware_dir / "src" / "core" / "config.h"

    def parse_config_defines(self) -> Dict[str, int]:
        defines: Dict[str, int] = {}
        if not self.config_path.exists():
            return defines
        for line in self.config_path.read_text(encoding="utf-8", errors="ignore").splitlines():
            m = re.match(r"^\s*#define\s+([A-Z0-9_]+)\s+(\d+)\b", line)
            if m:
                defines[m.group(1)] = int(m.group(2))
        return defines

    def verify_all(self) -> List[DocDriftIssue]:
        issues: List[DocDriftIssue] = []
        cfg = self.parse_config_defines()
        if not cfg:
            issues.append(DocDriftIssue("firmware/BmoGameboy/src/core/config.h", 1, "PIN_DRIFT", "config.h missing or empty"))
            return issues

        # 1. Verify 01_hardware.md documents every pin and flag in config.h accurately
        hw_rule = self.repo_root / ".agents" / "rules" / "01_hardware.md"
        if hw_rule.exists():
            hw_text = hw_rule.read_text(encoding="utf-8", errors="ignore")
            for macro in sorted(self.PIN_MACROS):
                if macro not in cfg:
                    continue
                expected_val = cfg[macro]
                if macro not in hw_text:
                    issues.append(
                        DocDriftIssue(
                            ".agents/rules/01_hardware.md",
                            1,
                            "PIN_DRIFT",
                            f"Macro `{macro}` ({expected_val}) from config.h is missing in 01_hardware.md",
                        )
                    )

        # 2. Scan key markdown files for explicit macro-to-GPIO mismatches and stale references
        target_docs = [
            self.repo_root / "README.md",
            self.repo_root / "AGENTS.md",
            self.repo_root / "docs" / "hardware-notes.md",
            self.repo_root / ".agents" / "rules" / "01_hardware.md",
            self.repo_root / ".agents" / "rules" / "02_architecture.md",
            self.repo_root / ".agents" / "rules" / "09_testing_infrastructure.md",
            self.repo_root / ".agents" / "rules" / "26_emulator_exit_contract.md",
            self.repo_root / ".agents" / "rules" / "27_codebase_map.md",
            self.repo_root / ".agents" / "rules" / "31_quick_start_primer.md",
        ]

        for doc_path in target_docs:
            if not doc_path.exists():
                continue
            rel_str = str(doc_path.relative_to(self.repo_root)).replace("\\", "/")
            lines = doc_path.read_text(encoding="utf-8", errors="ignore").splitlines()

            for idx, line in enumerate(lines, 1):
                # Check for stale file/tool references
                for stale_token, reason in self.FORBIDDEN_STALE_REFERENCES:
                    if stale_token in line:
                        issues.append(
                            DocDriftIssue(
                                rel_str,
                                idx,
                                "STALE_PATH",
                                f"References obsolete `{stale_token}` ({reason})",
                            )
                        )

                # Check for explicit `MACRO = N`, `| N | MACRO |`, or `MACRO (GPIO N)` mismatches
                for macro in self.PIN_MACROS:
                    if macro not in line or macro not in cfg:
                        continue
                    expected = cfg[macro]
                    # Pattern A: `| **13** | `SD_CS` |`
                    m_table = re.search(rf"\|\s*\*\*(\d+)\*\*\s*\|\s*`?{macro}`?", line)
                    if m_table and int(m_table.group(1)) != expected:
                        issues.append(
                            DocDriftIssue(
                                rel_str,
                                idx,
                                "PIN_DRIFT",
                                f"{macro} listed as GPIO {m_table.group(1)}, but config.h defines {expected}",
                            )
                        )
                    # Pattern B: `MACRO = N` or `MACRO: GPIO N` or `MACRO | GPIO N`
                    m_inline = re.search(rf"\b{macro}\b(?:\s*[:=|]\s*(?:GPIO\s*)?|\s+\(GPIO\s*)(\d+)\b", line)
                    if m_inline and int(m_inline.group(1)) != expected:
                        issues.append(
                            DocDriftIssue(
                                rel_str,
                                idx,
                                "PIN_DRIFT",
                                f"{macro} listed as {m_inline.group(1)}, but config.h defines {expected}",
                            )
                        )

        # 3. Verify 27_codebase_map.md lists all 14 emulator wrappers
        map_rule = self.repo_root / ".agents" / "rules" / "27_codebase_map.md"
        if map_rule.exists():
            map_text = map_rule.read_text(encoding="utf-8", errors="ignore")
            expected_wrappers = [
                "emu_peanut", "emu_walnut", "emu_nes", "emu_doom",
                "emu_sms", "emu_pce", "emu_atari", "emu_pico",
                "emu_genesis", "emu_snes", "emu_wswan", "emu_ngp",
                "emu_lynx", "emu_colem",
            ]
            for w in expected_wrappers:
                if w not in map_text:
                    issues.append(
                        DocDriftIssue(
                            ".agents/rules/27_codebase_map.md",
                            1,
                            "STALE_ARCHITECTURE",
                            f"Emulator wrapper `{w}` is missing from 27_codebase_map.md",
                        )
                    )

        return issues
