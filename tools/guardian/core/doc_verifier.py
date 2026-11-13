"""
tools.guardian.core.doc_verifier — Cross-Document Hardware, Symbol, and Architecture Truth Verifier
Ensures that Markdown documentation and AI rules never drift out of sync with C/C++ firmware source truth.
"""

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List
from urllib.parse import unquote, urlsplit


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
        "BTN_X", "BTN_Y", "BTN_L", "BTN_R",
        "BATTERY_ADC_PIN", "I2S_BCLK", "I2S_LRC", "I2S_DIN",
        "I2C_SDA", "I2C_SCL",
        "FEATURE_SD_CARD", "FEATURE_AUDIO", "FEATURE_BATTERY_MONITOR",
    }

    FORBIDDEN_STALE_REFERENCES = [
        ("IMPLEMENTATION_PLAN.md", "Deleted legacy root plan; use .agents/rules/42_quality_of_life_and_roadmap_ledger.md"),
        ("report_guardian.md", "Deleted root report; use docs/performance_report.md"),
        ("tools/convert.py", "Deleted duplicate script; use scripts/convert_roms.py"),
        ("zig-windows-x86_64", "Removed Zig binary dependency; native host tests use MSVC cl.exe"),
        ("teardownCurrentEmulator", "Obsolete function name; actual symbol in BmoGameboy.ino is destroyActiveEmulator()"),
        ("STATE_CONSOLE_SPECS", "Obsolete state name; actual enum in BmoGameboy.ino is STATE_CONSOLE_MUSEUM"),
        ("submitFrameAsync", "Non-existent DisplayEmu method; use streamGBFrame or streamRawFrameAsync"),
    ]

    BANNED_BUZZWORDS = (
        "apple/nintendo", "nintendo-grade", "apple-grade", "nintendo-style",
        "museum-grade", "reviewer-grade", "production-grade", "ultra-high-end",
        "state-of-the-art", "world-class", "god-tier", "next-gen",
        "industry-leading", "best-in-class", "living architectural specification",
        "buttery-smooth", "blazing-fast", "blazing fast", "lightning-fast",
        "ultra-optimized", "supercharge", "supercharged", "amazing", "masterclass",
        "overkill", "insane", "massive victory", "perfection", "miracle",
        "bulletproof", "unprecedented", "seamless", "flawless", "delve",
        "tapestry", "testament", "paradigm", "synergy", "holistic", "meticulously",
        "bespoke", "seamlessly", "flawlessly", "unparalleled", "unmatched",
        "groundbreaking", "revolutionary", "cutting-edge", "game-changing",
        "elevate", "elevated", "empower", "empowered", "living companion",
        "authoritative living", "glanceable",
    )
    BANNED_BUZZWORDS_REGEX = re.compile(
        r"\b(" + "|".join(re.escape(term) for term in BANNED_BUZZWORDS) + r")\b",
        re.IGNORECASE,
    )
    ALLOWED_PROPER_NOUNS = ("Amazing Spider-Man", "Alex Kidd in Miracle World")
    MARKDOWN_LINK_REGEX = re.compile(r"\[[^\]]*\]\(([^)\s]+)(?:\s+[^)]*)?\)")
    LINK_SCAN_IGNORED_DIRS = {
        ".git", ".tools", "build", "node_modules", "__pycache__", "venv",
        "games", "roms", "sdcard",
    }

    RULE_EXEMPT_FILES = {
        ".agents/rules/05_git_workflow.md",
        ".agents/rules/06_verification_standards.md",
        ".agents/rules/41_engineering_communication_and_critical_pushback.md",
        ".agents/rules/43_no_ai_jargon_and_buzzwords.md",
        ".agents/rules/44_anti_ai_ui_design_standards.md",
        "DESIGN.md",
        "docs/consolidated-rules.md",
        "tools/guardian/core/doc_verifier.py",
        "tools/guardian/tests/test_guardian.py",
        "scripts/install_git_hooks.py",
    }

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
            issues.extend(self.verify_markdown_links())
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

        # 2. Scan key markdown files and all active .agents/rules/*.md for macro-to-GPIO mismatches and stale references
        rules_dir = self.repo_root / ".agents" / "rules"
        historical_rule_logs = {"04_known_issues.md", "23_incident_postmortem_log.md"}
        rule_docs = sorted(
            p for p in rules_dir.glob("*.md")
            if p.name not in historical_rule_logs
        ) if rules_dir.exists() else []

        sdd_docs = sorted((self.repo_root / "docs" / "software-design-document").glob("*.md")) if (self.repo_root / "docs" / "software-design-document").exists() else []
        readme_docs = sorted((self.repo_root / "docs" / "readme").glob("*.md")) if (self.repo_root / "docs" / "readme").exists() else []
        wiring_docs = sorted((self.repo_root / "docs" / "wiring").glob("*.md")) if (self.repo_root / "docs" / "wiring").exists() else []
        top_docs = sorted(
            p for p in (self.repo_root / "docs").glob("*.md")
        ) if (self.repo_root / "docs").exists() else []
        viz_readme = [self.repo_root / "visualizations" / "README.md"]

        target_docs = [
            self.repo_root / "README.md",
            self.repo_root / "AGENTS.md",
            self.repo_root / "DESIGN.md",
        ] + top_docs + sdd_docs + readme_docs + wiring_docs + viz_readme + rule_docs

        for doc_path in target_docs:
            if not doc_path.exists():
                continue
            rel_str = str(doc_path.relative_to(self.repo_root)).replace("\\", "/")
            lines = doc_path.read_text(encoding="utf-8", errors="ignore").splitlines()

            for idx, line in enumerate(lines, 1):
                # Check for stale file/tool/symbol references
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

                # Check for unguarded FlashMode=opi command flags (unless warning against it)
                if "FlashMode=opi" in line:
                    low_line = line.lower()
                    if not any(w in low_line for w in ("never", "do not", "don't", "brick", "bootloop", "wrong", "forbidden", "hard-stop", "bad", "prevent", "setting")):
                        issues.append(
                            DocDriftIssue(
                                rel_str,
                                idx,
                                "PIN_DRIFT",
                                "Contains unguarded `FlashMode=opi` (brick hazard; board requires `FlashMode=qio,PSRAM=opi`)",
                            )
                        )

                # Check for explicit `MACRO = N`, `| N | MACRO |`, or `MACRO (GPIO N)` mismatches
                for macro in self.PIN_MACROS:
                    if macro not in line or macro not in cfg:
                        continue
                    if any(w in line.lower() for w in ("setting ", "never ", "do not ", "mistake")):
                        continue
                    expected = cfg[macro]
                    # Pattern A: `| **13** | `SD_CS` |`
                    m_table = re.search(rf"\|\s*\*\*(\d+)\*\*\s*\|\s*`?{macro}\b`?", line)
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

        # 4. Scan repo files for banned AI buzzwords and marketing filler (Rule 43)
        issues.extend(self.verify_no_buzzwords())
        issues.extend(self.verify_rule_file_sizes())
        issues.extend(self.verify_markdown_links())

        return issues

    def verify_markdown_links(self) -> List[DocDriftIssue]:
        """Reject file:// links and Markdown links whose local target is missing."""
        issues: List[DocDriftIssue] = []
        for path in self.repo_root.rglob("*.md"):
            relative = path.relative_to(self.repo_root)
            if any(part in self.LINK_SCAN_IGNORED_DIRS for part in relative.parts):
                continue
            rel_str = relative.as_posix()
            in_fence_char = ""
            in_fence_len = 0
            for line_number, line in enumerate(path.read_text(encoding="utf-8", errors="ignore").splitlines(), 1):
                stripped = line.lstrip(" ")
                if len(line) - len(stripped) <= 3:
                    if not in_fence_char:
                        m_open = re.match(r"^(`{3,}|~{3,})(.*)$", stripped)
                        if m_open:
                            fence = m_open.group(1)
                            info = m_open.group(2)
                            if not (fence[0] == "`" and "`" in info):
                                in_fence_char = fence[0]
                                in_fence_len = len(fence)
                                continue
                    elif re.match(rf"^{re.escape(in_fence_char)}{{{in_fence_len},}}[ \t]*$", stripped):
                        in_fence_char = ""
                        in_fence_len = 0
                        continue
                if in_fence_char:
                    continue
                for match in self.MARKDOWN_LINK_REGEX.finditer(line):
                    href = match.group(1).strip().strip("<>")
                    if href.lower().startswith("file:"):
                        issues.append(DocDriftIssue(
                            rel_str, line_number, "FILE_URI_LINK",
                            "Uses a file:// URI; replace it with a repository-relative link",
                        ))
                        continue
                    parsed = urlsplit(href)
                    if parsed.scheme or href.startswith("//") or not parsed.path:
                        continue
                    target_text = unquote(parsed.path).replace("\\", "/")
                    target = (path.parent / target_text).resolve()
                    try:
                        target.relative_to(self.repo_root.resolve())
                    except ValueError:
                        issues.append(DocDriftIssue(
                            rel_str, line_number, "UNRESOLVED_LINK",
                            f"Relative link `{href}` resolves outside the repository",
                        ))
                        continue
                    if not target.exists():
                        issues.append(DocDriftIssue(
                            rel_str, line_number, "UNRESOLVED_LINK",
                            f"Relative link target does not exist: `{href}`",
                        ))
        return issues

    def verify_rule_file_sizes(self) -> List[DocDriftIssue]:
        issues: List[DocDriftIssue] = []
        rules_dir = self.repo_root / ".agents" / "rules"
        if not rules_dir.exists():
            return issues
        for path in sorted(rules_dir.glob("*.md")):
            try:
                char_count = len(path.read_text(encoding="utf-8"))
            except OSError as exc:
                issues.append(DocDriftIssue(
                    str(path.relative_to(self.repo_root)).replace("\\", "/"),
                    1,
                    "RULE_FILE_TOO_LARGE",
                    f"Unable to read rule file for size check: {exc}",
                ))
                continue
            if char_count > 12_000:
                issues.append(DocDriftIssue(
                    str(path.relative_to(self.repo_root)).replace("\\", "/"),
                    1,
                    "RULE_FILE_TOO_LARGE",
                    f"Rule file has {char_count:,} characters; limit is 12,000",
                ))
        return issues

    @classmethod
    def buzzword_scan_text(cls, line: str) -> str:
        """Remove documented proper-name exceptions before language checks."""
        for proper_noun in cls.ALLOWED_PROPER_NOUNS:
            line = re.sub(re.escape(proper_noun), " ", line, flags=re.IGNORECASE)
        return line

    def verify_no_buzzwords(self) -> List[DocDriftIssue]:
        issues: List[DocDriftIssue] = []
        scan_roots = [
            self.repo_root / "README.md",
            self.repo_root / "AGENTS.md",
            self.repo_root / "CHANGELOG.md",
            self.repo_root / "docs",
            self.repo_root / ".agents" / "rules",
            self.repo_root / "firmware" / "BmoGameboy",
            self.repo_root / "tools",
            self.repo_root / "scripts",
            self.repo_root / "tests",
        ]
        valid_exts = {".md", ".c", ".cpp", ".h", ".ino", ".py", ".js"}

        files_to_scan: List[Path] = []
        for root in scan_roots:
            if not root.exists():
                continue
            if root.is_file():
                files_to_scan.append(root)
            else:
                for p in root.rglob("*"):
                    if not p.is_file() or p.suffix.lower() not in valid_exts:
                        continue
                    rel = str(p.relative_to(self.repo_root)).replace("\\", "/")
                    if (
                        "/assets/roms/" in rel
                        or "/vendor/doom/" in rel
                        or "__pycache__" in rel
                        or rel == "tools/bmo_simulator/bmo_playtest_cores.b64.js"
                        or rel == "tools/bmo_simulator/bmo_enclosure_meshes.js"
                    ):
                        continue
                    files_to_scan.append(p)

        for fpath in files_to_scan:
            rel_str = str(fpath.relative_to(self.repo_root)).replace("\\", "/")
            if rel_str in self.RULE_EXEMPT_FILES or rel_str.startswith("docs/consolidated-rules/"):
                continue
            if rel_str.startswith("docs/archive/"):
                continue  # Historical wording is preserved verbatim in the archive.
            lines = fpath.read_text(encoding="utf-8", errors="ignore").splitlines()
            for idx, line in enumerate(lines, 1):
                scan_line = self.buzzword_scan_text(line)
                for m in self.BANNED_BUZZWORDS_REGEX.finditer(scan_line):
                    issues.append(
                        DocDriftIssue(
                            rel_str,
                            idx,
                            "BUZZWORD_VIOLATION",
                            f"Banned AI buzzword/jargon `{m.group(1)}` found (Rule 43)",
                        )
                    )
        return issues
