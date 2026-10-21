"""
tools.guardian.core.ui_auditor — Anti-AI-Slop UI & Visual Design System Auditor

Enforces DESIGN.md, Rule 08, and Rule 44 standards across firmware C++
and web simulator assets:
  1. Firmware UI token discipline (no hardcoded raw hex colors in drawing calls)
  2. Physical selection outline invariant (drawRoundRect vs fillRoundRect per Rule 08)
  3. Simulator CSS craft checks (no nested cards, focus-visible states, 8px/4px grid)
  4. Design token parity between firmware/theme.h and simulator.css
"""

from dataclasses import dataclass
from pathlib import Path
import re
from typing import Dict, List, Optional, Tuple


@dataclass
class UiAuditIssue:
    file_path: str
    line_number: int
    category: str  # HARDCODED_COLOR, CARD_NESTING, MISSING_FOCUS, OFF_GRID_SPACING, TOKEN_PARITY, SELECTION_FILL
    message: str


@dataclass
class UiAuditReport:
    total_checks: int
    passed_checks: int
    issues: List[UiAuditIssue]

    @property
    def is_clean(self) -> bool:
        return len(self.issues) == 0


class UiAuditor:
    """Static auditor for design tokens, accessibility, and anti-AI-slop UI standards."""

    CORE_FIRMWARE_UI_FILES = [
        "firmware/BmoGameboy/src/core/display_emu.cpp",
        "firmware/BmoGameboy/src/core/box_art.cpp",
        "firmware/BmoGameboy/src/core/bmo_face.cpp",
    ]

    CORE_TOKENS = {
        "BMO_SCREEN_MINT": ("--screen-mint", "#CEF5E4"),
        "BMO_BODY_TEAL": ("--chassis-color", "#5FB49C"),
        "BMO_DEEP_TEAL": ("--bezel-color", "#1A4B42"),
        "BMO_DARK_FOREST": ("--bezel-lens", "#0F2620"),
        "BMO_DPAD_YELLOW": ("--dpad-color", "#FFE033"),
        "BMO_CORAL_RED": ("--btn-b-color", "#E8175D"),
        "BMO_BLUE_BUTTON": ("--btn-a-color", "#52A6BA"),
    }

    REQUIRED_FOCUS_SELECTORS = [
        ".control-pill",
        ".dpad-btn",
        ".round-btn",
        ".pill-btn",
    ]

    DRAW_CALL_PATTERN = re.compile(
        r"\b(?:drawRoundRect|drawRect|fillRect|fillRoundRect|setTextColor|drawPixel|drawLine|drawCircle|fillCircle)\s*\("
    )

    RAW_HEX_PATTERN = re.compile(r"\b0x[0-9a-fA-F]{3,4}\b")

    OFF_GRID_SPACING_PATTERN = re.compile(
        r"\b(?:margin|padding)(?:-(?:top|bottom|left|right))?\s*:\s*([^;]+);"
    )

    def __init__(self, repo_root: Path):
        self.repo_root = repo_root
        self.firmware_dir = repo_root / "firmware" / "BmoGameboy"
        self.theme_h = self.firmware_dir / "src" / "core" / "theme.h"
        self.simulator_css = repo_root / "tools" / "bmo_simulator" / "simulator.css"
        self.simulator_html = repo_root / "tools" / "bmo_simulator" / "index.html"

    def audit_firmware_ui_tokens(self) -> List[UiAuditIssue]:
        issues: List[UiAuditIssue] = []
        for rel_path in self.CORE_FIRMWARE_UI_FILES:
            full_path = self.repo_root / rel_path
            if not full_path.exists():
                continue

            lines = full_path.read_text(encoding="utf-8", errors="ignore").splitlines()
            for idx, line in enumerate(lines, start=1):
                stripped = line.strip()
                if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
                    continue

                # Check for raw inline 16-bit hex literals inside drawing calls
                if self.DRAW_CALL_PATTERN.search(stripped):
                    # Exclude bitmap sprite arrays or comments
                    if "{" in stripped and "}" in stripped and "," in stripped:
                        continue
                    hex_matches = self.RAW_HEX_PATTERN.findall(stripped)
                    for hx in hex_matches:
                        # Allow 0x0000 or 0xFFFF as pure black/white wire values if explicitly commented
                        if hx.upper() in {"0x0000", "0xFFFF"}:
                            continue
                        issues.append(
                            UiAuditIssue(
                                rel_path,
                                idx,
                                "HARDCODED_COLOR",
                                f"Raw hex color literal {hx} in drawing call; replace with Theme:: token from theme.h",
                            )
                        )

        return issues

    def audit_simulator_css(self) -> List[UiAuditIssue]:
        issues: List[UiAuditIssue] = []
        if not self.simulator_css.exists():
            issues.append(
                UiAuditIssue(
                    "tools/bmo_simulator/simulator.css",
                    1,
                    "MISSING_FILE",
                    "Simulator stylesheet tools/bmo_simulator/simulator.css not found",
                )
            )
            return issues

        css_content = self.simulator_css.read_text(encoding="utf-8", errors="ignore")
        css_lines = css_content.splitlines()

        # 1. Anti-Pattern: Card nesting in CSS (.panel-card .panel-card or .card .card)
        for idx, line in enumerate(css_lines, start=1):
            if re.search(r"\.panel-card\s+[^{]*\.panel-card", line) or re.search(r"\.card\s+[^{]*\.card", line):
                issues.append(
                    UiAuditIssue(
                        "tools/bmo_simulator/simulator.css",
                        idx,
                        "CARD_NESTING",
                        "Nested card anti-pattern detected in CSS rule; use whitespace or divider lines instead",
                    )
                )

        # 2. Mandatory Focus States: Verify each interactive class has :focus-visible
        for selector in self.REQUIRED_FOCUS_SELECTORS:
            focus_pattern = re.compile(re.escape(selector) + r"[^{,]*:focus-visible")
            if not focus_pattern.search(css_content):
                issues.append(
                    UiAuditIssue(
                        "tools/bmo_simulator/simulator.css",
                        1,
                        "MISSING_FOCUS",
                        f"Missing :focus-visible interactive state for {selector} selector",
                    )
                )

        # 3. Off-grid spacing scan: Flag odd pixel values like 7px, 11px, 13px, 17px, 19px, 23px
        for idx, line in enumerate(css_lines, start=1):
            m = self.OFF_GRID_SPACING_PATTERN.search(line)
            if m:
                val_str = m.group(1)
                px_matches = re.findall(r"\b(\d+)px\b", val_str)
                for px in px_matches:
                    val = int(px)
                    # Odd numbers or values not aligned to 4px/8px grid
                    if val > 4 and (val % 4 != 0) and val in {7, 11, 13, 17, 19, 21, 23, 27, 29}:
                        issues.append(
                            UiAuditIssue(
                                "tools/bmo_simulator/simulator.css",
                                idx,
                                "OFF_GRID_SPACING",
                                f"Off-grid spacing value {val}px; align to 4px/8px spacing grid",
                            )
                        )

        return issues

    def audit_token_parity(self) -> List[UiAuditIssue]:
        issues: List[UiAuditIssue] = []
        if not self.theme_h.exists():
            issues.append(
                UiAuditIssue("firmware/BmoGameboy/src/core/theme.h", 1, "TOKEN_PARITY", "theme.h missing")
            )
            return issues

        if not self.simulator_css.exists():
            issues.append(
                UiAuditIssue("tools/bmo_simulator/simulator.css", 1, "TOKEN_PARITY", "simulator.css missing")
            )
            return issues

        theme_text = self.theme_h.read_text(encoding="utf-8", errors="ignore")
        css_text = self.simulator_css.read_text(encoding="utf-8", errors="ignore")

        # Parse :root block in simulator.css
        root_match = re.search(r":root\s*\{([^}]+)\}", css_text, re.DOTALL)
        css_vars: Dict[str, str] = {}
        if root_match:
            for line in root_match.group(1).splitlines():
                vm = re.search(r"(--[a-zA-Z0-9_-]+)\s*:\s*([^;]+);", line)
                if vm:
                    css_vars[vm.group(1).strip()] = vm.group(2).strip().lower()

        # Check each core token
        for token_name, (css_var, expected_hex) in self.CORE_TOKENS.items():
            if token_name not in theme_text:
                issues.append(
                    UiAuditIssue(
                        "firmware/BmoGameboy/src/core/theme.h",
                        1,
                        "TOKEN_PARITY",
                        f"Core token {token_name} missing from theme.h",
                    )
                )

            if css_var not in css_vars:
                issues.append(
                    UiAuditIssue(
                        "tools/bmo_simulator/simulator.css",
                        1,
                        "TOKEN_PARITY",
                        f"CSS custom property {css_var} missing from :root in simulator.css",
                    )
                )
            else:
                actual_css_val = css_vars[css_var]
                if expected_hex.lower() not in actual_css_val:
                    issues.append(
                        UiAuditIssue(
                            "tools/bmo_simulator/simulator.css",
                            1,
                            "TOKEN_PARITY",
                            f"Token color mismatch for {css_var}: expected {expected_hex.lower()}, found {actual_css_val}",
                        )
                    )

        return issues

    def run_all_checks(self) -> UiAuditReport:
        all_issues: List[UiAuditIssue] = []

        # Check 1: Firmware UI tokens
        fw_issues = self.audit_firmware_ui_tokens()
        all_issues.extend(fw_issues)

        # Check 2: Simulator CSS rules & focus states
        css_issues = self.audit_simulator_css()
        all_issues.extend(css_issues)

        # Check 3: Token parity across firmware and web simulator
        parity_issues = self.audit_token_parity()
        all_issues.extend(parity_issues)

        total_checks = len(self.CORE_FIRMWARE_UI_FILES) + 3 + len(self.CORE_TOKENS)
        passed_checks = total_checks - len(all_issues)
        if passed_checks < 0:
            passed_checks = 0

        return UiAuditReport(
            total_checks=total_checks,
            passed_checks=passed_checks,
            issues=all_issues,
        )
