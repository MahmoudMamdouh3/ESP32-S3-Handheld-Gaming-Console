"""Verify firmware, simulator CSS, DESIGN.md, and face-renderer color tokens."""

import json
from pathlib import Path
import re

REPO_ROOT = Path(__file__).resolve().parents[1]

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

FACE_COLOR = re.compile(
    r"static\s+const\s+uint16_t\s+(COL_[A-Z0-9_]+)\s*=\s*packBGR565\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\).*?//\s*#([0-9A-Fa-f]{6})"
)


def parse_theme_colors(theme_text: str) -> dict[str, tuple[str, int, int, int]]:
    parsed: dict[str, tuple[str, int, int, int]] = {}
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


def verify_token_parity(repo_root: Path, theme_colors: dict[str, tuple[str, int, int, int]]) -> list[str]:
    issues: list[str] = []
    css_path = repo_root / "simulator" / "simulator.css"
    if not css_path.exists():
        return [f"simulator.css not found under {repo_root}"]

    css_text = css_path.read_text(encoding="utf-8", errors="ignore")
    root_match = re.search(r":root\s*\{([^}]+)\}", css_text, re.DOTALL)
    css_vars: dict[str, str] = {}
    if root_match:
        for line in root_match.group(1).splitlines():
            vm = re.search(r"(--[a-zA-Z0-9_-]+)\s*:\s*([^;]+);", line)
            if vm:
                css_vars[vm.group(1).strip()] = vm.group(2).strip().lower()

    for token_name, (css_var, expected_hex) in CORE_TOKENS.items():
        parsed = theme_colors.get(token_name)
        if parsed is None:
            issues.append(f"theme.h: cannot derive RGB values for {token_name} from makeUiColor()")
        elif parsed[0].lower() != expected_hex.lower():
            issues.append(
                f"theme.h:{parsed[3]}: RGB for {token_name} produces {parsed[0]}, expected {expected_hex}"
            )
        if css_var not in css_vars:
            issues.append(f"{css_path.name}: CSS variable {css_var} missing from :root")
        elif css_vars[css_var] != expected_hex.lower():
            issues.append(
                f"{css_path.name}: token mismatch for {css_var}: expected {expected_hex.lower()}, found {css_vars[css_var]}"
            )

    design_path = repo_root / "DESIGN.md"
    if not design_path.exists():
        issues.append("DESIGN.md is missing")
        return issues

    seen_tokens = set()
    for line_number, line in enumerate(design_path.read_text(encoding="utf-8", errors="ignore").splitlines(), start=1):
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if not cells:
            continue
        token_match = re.search(r"`(BMO_[A-Z0-9_]+)`", cells[0])
        if not token_match or token_match.group(1) not in CORE_TOKENS:
            continue
        token = token_match.group(1)
        seen_tokens.add(token)
        expected = theme_colors.get(token)
        if expected is None:
            continue
        expected_hex, expected_ui, expected_direct, _ = expected
        expected_cells = {
            2: expected_hex,
            4: f"0x{expected_ui:04X}",
            5: f"0x{expected_direct:04X}",
            6: CORE_TOKENS[token][0],
        }
        for col, expected_val in expected_cells.items():
            if col >= len(cells):
                issues.append(f"DESIGN.md:{line_number}: row for {token} missing column {col + 1}")
                break
            actual = cells[col].replace("`", "").strip()
            if actual.lower() != expected_val.lower():
                issues.append(
                    f"DESIGN.md:{line_number}: {token} mismatch: expected {expected_val}, found {actual}"
                )
    for token in CORE_TOKENS:
        if token not in seen_tokens:
            issues.append(f"DESIGN.md: color table missing {token}")
    return issues


def verify_face_palette(repo_root: Path) -> list[str]:
    bmo_face = repo_root / "firmware" / "BmoGameboy" / "src" / "ui" / "bmo_face.cpp"
    source = bmo_face.read_text(encoding="utf-8")
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


def verify_recomputed_color_math(theme_colors: dict[str, tuple[str, int, int, int]]) -> list[str]:
    issues = []
    for token, (rgb_hex, ui_word, direct_word, _) in theme_colors.items():
        rgb = tuple(int(rgb_hex[index:index + 2], 16) for index in (1, 3, 5))
        red, green, blue = rgb
        calculated_direct = ((blue & 0xF8) << 8) | ((green & 0xFC) << 3) | (red >> 3)
        calculated_ui = ((calculated_direct & 0xFF) << 8) | (calculated_direct >> 8)
        if (direct_word, ui_word) != (calculated_direct, calculated_ui):
            issues.append(f"{token}: parsed BGR565 values do not match recomputed RGB values")

    def luminance(hex_color: str) -> float:
        values = [int(hex_color[index:index + 2], 16) / 255.0 for index in (1, 3, 5)]
        linear = [value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4 for value in values]
        return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]

    def contrast(first: str, second: str) -> float:
        high, low = sorted((luminance(first), luminance(second)), reverse=True)
        return (high + 0.05) / (low + 0.05)

    coral = theme_colors["BMO_CORAL_RED"][0]
    deep_teal = theme_colors["BMO_DEEP_TEAL"][0]
    dark_forest = theme_colors["BMO_DARK_FOREST"][0]
    ratios = (round(contrast(coral, deep_teal), 1), round(contrast(dark_forest, coral), 1))
    if ratios != (4.6, 7.5):
        issues.append(f"Coral contrast recomputed as {ratios[0]}:1 and {ratios[1]}:1, expected 4.6:1 and 7.5:1")
    return issues


def verify_design_tokens_json(
    repo_root: Path, theme_colors: dict[str, tuple[str, int, int, int]]
) -> list[str]:
    tokens_path = repo_root / "design" / "tokens.json"
    if not tokens_path.is_file():
        return [f"design/tokens.json not found under {repo_root}"]

    data = json.loads(tokens_path.read_text(encoding="utf-8"))
    issues: list[str] = []

    core = data.get("core_tokens", {})
    for token_name, (css_var, expected_hex) in CORE_TOKENS.items():
        entry = core.get(token_name)
        if not isinstance(entry, dict):
            issues.append(f"design/tokens.json: missing core_tokens.{token_name}")
            continue
        if entry.get("css_var") != css_var:
            issues.append(f"design/tokens.json: {token_name}.css_var expected {css_var}, found {entry.get('css_var')}")
        if str(entry.get("hex", "")).lower() != expected_hex.lower():
            issues.append(f"design/tokens.json: {token_name}.hex expected {expected_hex}, found {entry.get('hex')}")
        parsed = theme_colors.get(token_name)
        if parsed is not None:
            _, ui_word, direct_word, _ = parsed
            if str(entry.get("canvas_bgr565_swapped", "")).lower() != f"0x{ui_word:04x}":
                issues.append(f"design/tokens.json: {token_name}.canvas_bgr565_swapped mismatch")
            if str(entry.get("direct_bgr565", "")).lower() != f"0x{direct_word:04x}":
                issues.append(f"design/tokens.json: {token_name}.direct_bgr565 mismatch")

    bmo_face_src = (repo_root / "firmware" / "BmoGameboy" / "src" / "ui" / "bmo_face.cpp").read_text(encoding="utf-8")
    face_json = data.get("face_palette", {})
    for name, red, green, blue, _ in FACE_COLOR.findall(bmo_face_src):
        expected_hex = "#%02X%02X%02X" % (int(red), int(green), int(blue))
        entry = face_json.get(name)
        if not isinstance(entry, dict) or str(entry.get("hex", "")).upper() != expected_hex:
            issues.append(f"design/tokens.json: face_palette.{name} expected {expected_hex}, found {entry}")

    return issues


def main() -> int:
    theme_h = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "ui" / "theme.h"
    theme_colors = parse_theme_colors(theme_h.read_text(encoding="utf-8"))
    for token in CORE_TOKENS:
        source_hex, canvas_word, direct_word, _ = theme_colors[token]
        print(f"{token}: RGB={source_hex} canvas=0x{canvas_word:04X} direct=0x{direct_word:04X}")

    issues = verify_token_parity(REPO_ROOT, theme_colors)
    issues.extend(verify_face_palette(REPO_ROOT))
    issues.extend(verify_recomputed_color_math(theme_colors))
    issues.extend(verify_design_tokens_json(REPO_ROOT, theme_colors))
    if issues:
        for issue in issues:
            print(f"[FAIL] {issue}")
        return 1
    print("[PASS] Theme RGB565, DESIGN.md rows, CSS variables, contrast, and face-renderer colors agree.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
