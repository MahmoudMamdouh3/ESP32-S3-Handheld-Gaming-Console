"""Verify firmware, simulator, and design-document color values."""

from pathlib import Path
import re
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))

from tools.guardian.core.ui_auditor import UiAuditor


FACE_COLOR = re.compile(
    r"static\s+const\s+uint16_t\s+(COL_[A-Z0-9_]+)\s*=\s*packBGR565\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\).*?//\s*#([0-9A-Fa-f]{6})"
)


def verify_face_palette(repo_root: Path) -> list[str]:
    source = (repo_root / "firmware/BmoGameboy/src/core/bmo_face.cpp").read_text(encoding="utf-8")
    design = (repo_root / "DESIGN.md").read_text(encoding="utf-8")
    source_colors = {}
    for name, red, green, blue, comment_hex in FACE_COLOR.findall(source):
        rgb = (int(red), int(green), int(blue))
        expected_hex = "#%02X%02X%02X" % rgb
        if expected_hex.lower() != f"#{comment_hex}".lower():
            source_colors[name] = (expected_hex, "source comment mismatch")
        else:
            source_colors[name] = (expected_hex, None)

    registered = {}
    for line in design.splitlines():
        match = re.search(r"\|\s*`(COL_[A-Z0-9_]+)`\s*\|\s*`?(#[0-9A-Fa-f]{6})`?\s*\|", line)
        if match:
            registered[match.group(1)] = match.group(2).upper()

    issues = []
    for name, (expected_hex, source_error) in source_colors.items():
        if source_error:
            issues.append(f"{name}: {source_error}; expected {expected_hex}")
        if registered.get(name) != expected_hex:
            issues.append(f"DESIGN.md face palette mismatch for {name}: expected {expected_hex}, found {registered.get(name)}")
    for name in sorted(set(registered) - set(source_colors)):
        issues.append(f"DESIGN.md registers missing source color {name}")
    if not source_colors:
        issues.append("No face-renderer COL_* colors were parsed")
    return issues


def verify_recomputed_color_math(auditor: UiAuditor) -> list[str]:
    theme_text = auditor.theme_h.read_text(encoding="utf-8")
    colors = auditor._parse_theme_colors(theme_text)
    issues = []
    for token, (rgb_hex, ui_word, direct_word, _) in colors.items():
        rgb = tuple(int(rgb_hex[index:index + 2], 16) for index in (1, 3, 5))
        red, green, blue = rgb
        calculated_direct = ((blue & 0xF8) << 8) | ((green & 0xFC) << 3) | (red >> 3)
        calculated_ui = ((calculated_direct & 0xFF) << 8) | (calculated_direct >> 8)
        if (direct_word, ui_word) != (calculated_direct, calculated_ui):
            issues.append(f"{token}: parsed BGR565 values do not match the recomputed RGB values")

    def luminance(hex_color: str) -> float:
        values = [int(hex_color[index:index + 2], 16) / 255.0 for index in (1, 3, 5)]
        linear = [value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4 for value in values]
        return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]

    def contrast(first: str, second: str) -> float:
        high, low = sorted((luminance(first), luminance(second)), reverse=True)
        return (high + 0.05) / (low + 0.05)

    coral = colors["BMO_CORAL_RED"][0]
    deep_teal = colors["BMO_DEEP_TEAL"][0]
    dark_forest = colors["BMO_DARK_FOREST"][0]
    ratios = (round(contrast(coral, deep_teal), 1), round(contrast(dark_forest, coral), 1))
    if ratios != (4.6, 7.5):
        issues.append(f"Coral contrast recomputed as {ratios[0]}:1 and {ratios[1]}:1, expected 4.6:1 and 7.5:1")
    return issues


def main() -> int:
    auditor = UiAuditor(REPO_ROOT)
    theme_text = auditor.theme_h.read_text(encoding="utf-8")
    colors = auditor._parse_theme_colors(theme_text)
    for token in auditor.CORE_TOKENS:
        source_hex, canvas_word, direct_word, _ = colors[token]
        print(f"{token}: RGB={source_hex} canvas=0x{canvas_word:04X} direct=0x{direct_word:04X}")

    issues = auditor.audit_token_parity()
    issues.extend(verify_face_palette(REPO_ROOT))
    issues.extend(verify_recomputed_color_math(auditor))
    if issues:
        for issue in issues:
            if hasattr(issue, "file_path"):
                print(f"[FAIL] {issue.file_path}:{issue.line_number} {issue.message}")
            else:
                print(f"[FAIL] {issue}")
        return 1
    print("[PASS] Theme RGB565, SDD/design rows, contrast, and registered face-renderer colors agree.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
