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
        "firmware/BmoGameboy/BmoGameboy.ino",
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
        "BMO_CORAL_RED": ("--btn-b-color", "#FF91A5"),
        "BMO_BLUE_BUTTON": ("--btn-a-color", "#52A6BA"),
        "BMO_WHITE": ("--text-main", "#F8FDFA"),
        "BMO_MOUTH_GREEN": ("--btn-x-color", "#4CAF50"),
        "BMO_TONGUE_PINK": ("--btn-y-color", "#F48FB1"),
    }

    REQUIRED_FOCUS_SELECTORS = [
        ".control-pill",
        ".dpad-btn",
        ".round-btn",
        ".pill-btn",
    ]

    DRAW_CALL_PATTERN = re.compile(
        r"\b(?:drawRoundRect|drawRect|fillRect|fillRoundRect|setTextColor|drawPixel|drawLine|drawCircle|fillCircle|showToast|drawCentered|drawFittedCentered)\s*\("
    )

    STRING_LITERAL_PATTERN = re.compile(r'"((?:\\.|[^"\\])*)"')

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

                # Check for non-ASCII characters inside C/C++ string literals in UI files
                # (Adafruit GFX CP437 5x7 and FreeSans 7-bit ASCII fonts render multibyte UTF-8 as garbage)
                for str_match in self.STRING_LITERAL_PATTERN.finditer(stripped):
                    literal_text = str_match.group(1)
                    if any(ord(ch) > 127 for ch in literal_text):
                        issues.append(
                            UiAuditIssue(
                                rel_path,
                                idx,
                                "NON_ASCII_UI_TEXT",
                                "Multibyte UTF-8 character in firmware UI string literal renders as CP437 garbage on ST7789; use 7-bit ASCII",
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
        theme_colors = self._parse_theme_colors(theme_text)

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
            parsed = theme_colors.get(token_name)
            if parsed is None:
                issues.append(
                    UiAuditIssue(
                        "firmware/BmoGameboy/src/core/theme.h",
                        1,
                        "TOKEN_PARITY",
                        f"Cannot derive RGB values for {token_name} from makeUiColor()",
                    )
                )
            elif parsed[0].lower() != expected_hex.lower():
                issues.append(UiAuditIssue(
                    "firmware/BmoGameboy/src/core/theme.h", parsed[3], "TOKEN_PARITY",
                    f"RGB arguments for {token_name} produce {parsed[0]}, expected {expected_hex}",
                ))

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
                if actual_css_val.lower() != expected_hex.lower():
                    issues.append(
                        UiAuditIssue(
                            "tools/bmo_simulator/simulator.css",
                            1,
                            "TOKEN_PARITY",
                            f"Token color mismatch for {css_var}: expected {expected_hex.lower()}, found {actual_css_val}",
                        )
                    )

        issues.extend(self.audit_document_color_values(theme_colors))

        return issues

    @staticmethod
    def _parse_theme_colors(theme_text: str) -> Dict[str, Tuple[str, int, int, int]]:
        parsed: Dict[str, Tuple[str, int, int, int]] = {}
        pattern = re.compile(
            r"constexpr\s+uint16_t\s+(BMO_[A-Z0-9_]+)\s*=\s*makeUiColor\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)"
        )
        for line_number, line in enumerate(theme_text.splitlines(), start=1):
            match = pattern.search(line)
            if not match:
                continue
            name = match.group(1)
            r, g, b = (int(value) for value in match.groups()[1:])
            direct = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3)
            swapped = ((direct & 0xFF) << 8) | (direct >> 8)
            parsed[name] = (f"#{r:02X}{g:02X}{b:02X}", swapped, direct, line_number)
        return parsed

    def audit_document_color_values(
        self, theme_colors: Dict[str, Tuple[str, int, int, int]]
    ) -> List[UiAuditIssue]:
        issues: List[UiAuditIssue] = []
        design = self.repo_root / "DESIGN.md"
        sdd = self.repo_root / "docs" / "software-design-document" / "section-07.md"
        tables = [
            (design, "design", 2, 4, 5, 6),
            (sdd, "sdd", 1, 2, 3, 4),
        ]
        for path, table_kind, hex_col, ui_col, direct_col, css_col in tables:
            rel_path = str(path.relative_to(self.repo_root)).replace("\\", "/")
            if not path.exists():
                issues.append(UiAuditIssue(rel_path, 1, "TOKEN_PARITY", "Color token table is missing"))
                continue
            seen_tokens = set()
            for line_number, line in enumerate(path.read_text(encoding="utf-8", errors="ignore").splitlines(), start=1):
                cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
                if not cells:
                    continue
                token_match = re.search(r"`(BMO_[A-Z0-9_]+)`", cells[0])
                if not token_match:
                    continue
                token = token_match.group(1)
                if token not in self.CORE_TOKENS:
                    continue
                seen_tokens.add(token)
                expected = theme_colors.get(token)
                if expected is None:
                    continue
                expected_hex, expected_ui, expected_direct, _ = expected
                expected_cells = {
                    hex_col: expected_hex,
                    ui_col: f"0x{expected_ui:04X}",
                    direct_col: f"0x{expected_direct:04X}",
                    css_col: self.CORE_TOKENS[token][0],
                }
                for column, expected_value in expected_cells.items():
                    if column >= len(cells):
                        issues.append(UiAuditIssue(
                            rel_path, line_number, "TOKEN_PARITY",
                            f"{table_kind} color row for {token} is missing column {column + 1}",
                        ))
                        break
                    actual = cells[column].replace("`", "").strip()
                    if actual.lower() != expected_value.lower():
                        issues.append(UiAuditIssue(
                            rel_path, line_number, "TOKEN_PARITY",
                            f"{table_kind} {token} value mismatch: expected {expected_value}, found {actual}",
                        ))
            for token in self.CORE_TOKENS:
                if token not in seen_tokens:
                    issues.append(UiAuditIssue(
                        rel_path, 1, "TOKEN_PARITY",
                        f"{table_kind} color table is missing {token}",
                    ))
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

        total_checks = len(self.CORE_FIRMWARE_UI_FILES) + 3 + len(self.CORE_TOKENS) * 4
        passed_checks = total_checks - len(all_issues)
        if passed_checks < 0:
            passed_checks = 0

        return UiAuditReport(
            total_checks=total_checks,
            passed_checks=passed_checks,
            issues=all_issues,
        )
