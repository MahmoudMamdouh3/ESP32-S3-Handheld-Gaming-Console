#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Generate hardware layout SVGs, 31x46 perfboard placement guide, and wire list CSV from hardware/layout.json."""

from __future__ import annotations

import csv
import json
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
LAYOUT_PATH = REPO_ROOT / "hardware" / "layout.json"
TOP_SVG_PATH = REPO_ROOT / "docs" / "assets" / "layout_top_view.svg"
SIDE_SVG_PATH = REPO_ROOT / "docs" / "assets" / "layout_side_section.svg"
PERFBOARD_DOC_PATH = REPO_ROOT / "docs" / "wiring" / "perfboard-layout.md"
WIRE_CSV_PATH = REPO_ROOT / "docs" / "wiring" / "wire_list.csv"


def load_layout() -> dict:
    return json.loads(LAYOUT_PATH.read_text(encoding="utf-8"))


def generate_top_view_svg(layout: dict) -> str:
    # Scale: 50 px per cm, with 60 px margin
    scale = 50.0
    ox, oy = 60.0, 60.0
    env = layout["enclosure"]["owner_reference_cm"]
    w_cm = env["width_cm"]
    h_cm = env["height_cm"]
    svg_w = int(w_cm * scale + 140)
    svg_h = int(h_cm * scale + 130)

    def px(cm: float) -> float:
        return round(ox + cm * scale, 2)

    def py(cm: float) -> float:
        return round(oy + cm * scale, 2)

    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {svg_w} {svg_h}" width="{svg_w}" height="{svg_h}" font-family="monospace">',
        "  <!-- GENERATED FROM hardware/layout.json - DO NOT EDIT BY HAND -->",
        f'  <rect x="0" y="0" width="{svg_w}" height="{svg_h}" fill="#0F2620"/>',
        f'  <text x="{ox}" y="28" fill="#F8FDFA" font-size="14" font-weight="bold">BMO Handheld Top View (8.80 x 12.40 cm, 31x46 Perfboard Grid)</text>',
        f'  <text x="{ox}" y="46" fill="#CEF5E4" font-size="11">Origin (0,0) top-left | Grid = 0.254 cm (1 hole) | Evidence: NOT_TESTED</text>',
    ]

    # 1.0 cm ruler ticks
    for x_i in range(int(w_cm) + 1):
        x_p = px(float(x_i))
        lines.append(f'  <line x1="{x_p}" y1="{oy - 6}" x2="{x_p}" y2="{oy}" stroke="#CEF5E4" stroke-width="1"/>')
        lines.append(f'  <text x="{x_p - 8}" y="{oy - 9}" fill="#CEF5E4" font-size="9">{x_i}cm</text>')
    for y_i in range(int(h_cm) + 1):
        y_p = py(float(y_i))
        lines.append(f'  <line x1="{ox - 6}" y1="{y_p}" x2="{ox}" y2="{y_p}" stroke="#CEF5E4" stroke-width="1"/>')
        lines.append(f'  <text x="{ox - 36}" y="{y_p + 3}" fill="#CEF5E4" font-size="9">{y_i}cm</text>')

    # Outer enclosure shell
    r_px = round(env["corner_radius_cm"] * scale, 2)
    lines.append(
        f'  <rect x="{px(0)}" y="{py(0)}" width="{round(w_cm * scale, 2)}" height="{round(h_cm * scale, 2)}" '
        f'rx="{r_px}" ry="{r_px}" fill="#5FB49C" fill-opacity="0.22" stroke="#5FB49C" stroke-width="2.5"/>'
    )

    # Perfboard outline
    pb = layout["perfboard"]
    pb_x, pb_y = pb["board_top_left_cm"]
    lines.append(
        f'  <rect x="{px(pb_x)}" y="{py(pb_y)}" width="{round(pb["width_cm"] * scale, 2)}" height="{round(pb["height_cm"] * scale, 2)}" '
        f'fill="#1A4B42" fill-opacity="0.55" stroke="#FFE033" stroke-width="1.2" stroke-dasharray="4,2"/>'
    )

    # Perfboard 31x46 holes
    c1x, r1y = pb["hole_c01_r01_cm"]
    pitch = pb["pitch_cm"]
    for col in range(pb["columns"]):
        for row in range(pb["rows"]):
            hx = px(c1x + col * pitch)
            hy = py(r1y + row * pitch)
            lines.append(f'  <circle cx="{hx}" cy="{hy}" r="1.2" fill="#8AD5C3" fill-opacity="0.45"/>')

    # Rear zones (dashed)
    upper_comp = layout["z_stackup"]["rear_zones"]["upper_zone"]["components"]
    for comp in upper_comp:
        x0, x1 = comp["x_range_cm"]
        y0, y1 = comp["y_range_cm"]
        lines.append(
            f'  <rect x="{px(x0)}" y="{py(y0)}" width="{round((x1 - x0) * scale, 2)}" height="{round((y1 - y0) * scale, 2)}" '
            f'fill="none" stroke="#FF91A5" stroke-width="1.2" stroke-dasharray="3,3"/>'
        )
    aa_comp = layout["z_stackup"]["rear_zones"]["lower_zone"]["components"][0]
    ax0, ax1 = aa_comp["x_range_cm"]
    ay0, ay1 = aa_comp["y_range_cm"]
    lines.append(
        f'  <rect x="{px(ax0)}" y="{py(ay0)}" width="{round((ax1 - ax0) * scale, 2)}" height="{round((ay1 - ay0) * scale, 2)}" '
        f'fill="none" stroke="#FFE033" stroke-width="1.2" stroke-dasharray="5,3"/>'
    )
    lines.append(
        f'  <text x="{px(ax0 + 0.3)}" y="{py(ay0 + 0.4)}" fill="#FFE033" font-size="9">Rear Lower Zone: 2xAA Holder (5.80 x 3.20 cm)</text>'
    )

    # Screen bezel & active display area
    bez = layout["display"]["bezel_rect_cm"]
    act = layout["display"]["active_area_landscape_cm"]
    lines.append(
        f'  <rect x="{px(bez["x_min_cm"])}" y="{py(bez["y_min_cm"])}" '
        f'width="{round(bez["width_cm"] * scale, 2)}" height="{round(bez["height_cm"] * scale, 2)}" '
        f'rx="8" ry="8" fill="#1A4B42" fill-opacity="0.7" stroke="#F8FDFA" stroke-width="1.5"/>'
    )
    lines.append(
        f'  <rect x="{px(act["x_min_cm"])}" y="{py(act["y_min_cm"])}" '
        f'width="{round(act["width_cm"] * scale, 2)}" height="{round(act["height_cm"] * scale, 2)}" '
        f'fill="#CEF5E4" fill-opacity="0.25" stroke="#CEF5E4" stroke-width="1.5"/>'
    )
    lines.append(
        f'  <text x="{px(act["x_min_cm"] + 0.3)}" y="{py(act["y_min_cm"] + 0.5)}" fill="#CEF5E4" font-size="10">2.4" ST7789 Active Area (4.90 x 3.67 cm)</text>'
    )
    lines.append(
        f'  <text x="{px(1.2)}" y="{py(7.0)}" fill="#FF91A5" font-size="9">Rear Upper: DevKitC-1 (2.54x6.33cm) + MicroSD + Boost</text>'
    )

    # Buttons (show snapped hole position + 0.60 cm recommended body and 1.20 cm OMRON ghost outline)
    for btn in layout["controls"]["buttons"]:
        bx = px(btn["snapped_x_cm"])
        by = py(btn["snapped_y_cm"])
        color = "#FFE033" if btn["soldered"] else "#FF91A5"
        # 1.20 cm OMRON ghost box
        s12 = round(1.20 * scale, 2)
        s06 = round(0.60 * scale, 2)
        lines.append(
            f'  <rect x="{round(bx - s12 / 2, 2)}" y="{round(by - s12 / 2, 2)}" width="{s12}" height="{s12}" '
            f'fill="none" stroke="{color}" stroke-opacity="0.4" stroke-width="1" stroke-dasharray="2,2"/>'
        )
        # 0.60 cm recommended tactile switch box
        lines.append(
            f'  <rect x="{round(bx - s06 / 2, 2)}" y="{round(by - s06 / 2, 2)}" width="{s06}" height="{s06}" '
            f'fill="{color}" fill-opacity="0.35" stroke="{color}" stroke-width="1.5"/>'
        )
        lines.append(
            f'  <text x="{round(bx - 14, 2)}" y="{round(by + 3, 2)}" fill="#F8FDFA" font-size="8.5" font-weight="bold">{btn["label"]}(G{btn["gpio"]})</text>'
        )

    lines.append(
        f'  <text x="{ox}" y="{svg_h - 16}" fill="#F8FDFA" font-size="10">'
        f'Solid boxes = 0.60x0.60 cm switches (0.81 cm ABXY gap) | Dotted boxes = 1.20x1.20 cm OMRON (0.21 cm ABXY gap)</text>'
    )
    lines.append("</svg>\n")
    return "\n".join(lines)


def generate_side_section_svg(layout: dict) -> str:
    # Horizontal axis = Y (0.00 to 12.40 cm), Vertical axis = Z (0.00 to 3.20 cm)
    scale_y = 55.0
    scale_z = 75.0
    ox, oy = 70.0, 65.0
    h_cm = layout["enclosure"]["owner_reference_cm"]["height_cm"]
    z_cm = layout["z_stackup"]["total_thickness_cm"]
    svg_w = int(h_cm * scale_y + 130)
    svg_h = int(z_cm * scale_z + 150)

    def py_axis(y_val: float) -> float:
        return round(ox + y_val * scale_y, 2)

    def pz_axis(z_val: float) -> float:
        return round(oy + z_val * scale_z, 2)

    layer_colors = [
        "#5FB49C",
        "#52A6BA",
        "#FFE033",
        "#FF91A5",
        "#1A4B42",
        "#5FB49C",
    ]
    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {svg_w} {svg_h}" width="{svg_w}" height="{svg_h}" font-family="monospace">',
        "  <!-- GENERATED FROM hardware/layout.json - DO NOT EDIT BY HAND -->",
        f'  <rect x="0" y="0" width="{svg_w}" height="{svg_h}" fill="#0F2620"/>',
        f'  <text x="{ox}" y="26" fill="#F8FDFA" font-size="14" font-weight="bold">BMO Handheld Side-Section Z-Stackup (Total = 3.20 cm across Y = 0.00..12.40 cm)</text>',
        f'  <text x="{ox}" y="44" fill="#CEF5E4" font-size="11">Upper Y Zone (DevKitC-1: 1.35 cm) and Lower Y Zone (2xAA Holder: 1.65 cm) do NOT overlap in Z</text>',
    ]

    for idx, layer in enumerate(layout["z_stackup"]["layers"]):
        z0 = layer["z_start_cm"]
        z1 = layer["z_end_cm"]
        col = layer_colors[idx % len(layer_colors)]
        lines.append(
            f'  <rect x="{py_axis(0)}" y="{pz_axis(z0)}" width="{round(h_cm * scale_y, 2)}" '
            f'height="{round((z1 - z0) * scale_z, 2)}" fill="{col}" fill-opacity="0.22" stroke="{col}" stroke-width="1.2"/>'
        )
        lines.append(
            f'  <text x="8" y="{round((pz_axis(z0) + pz_axis(z1)) / 2 + 3, 2)}" fill="#F8FDFA" font-size="9.5">'
            f'z={z0:.2f}..{z1:.2f}cm</text>'
        )

    # Highlight Upper Y Zone (DevKitC-1: y=1.247..7.577, z=1.26..2.61)
    lines.append(
        f'  <rect x="{py_axis(1.25)}" y="{pz_axis(1.26)}" width="{round((7.58 - 1.25) * scale_y, 2)}" '
        f'height="{round(1.35 * scale_z, 2)}" fill="#52A6BA" fill-opacity="0.55" stroke="#F8FDFA" stroke-width="1.5"/>'
    )
    lines.append(
        f'  <text x="{py_axis(1.45)}" y="{pz_axis(1.90)}" fill="#F8FDFA" font-size="10" font-weight="bold">'
        f'Upper Y (1.25..7.58 cm): ESP32-S3-DevKitC-1 (1.35 cm deep -> 0.39 cm spare)</text>'
    )

    # Highlight Lower Y Zone (2xAA holder: y=8.10..11.30, z=1.26..2.91)
    lines.append(
        f'  <rect x="{py_axis(8.10)}" y="{pz_axis(1.26)}" width="{round((11.30 - 8.10) * scale_y, 2)}" '
        f'height="{round(1.65 * scale_z, 2)}" fill="#FFE033" fill-opacity="0.45" stroke="#FFE033" stroke-width="1.5"/>'
    )
    lines.append(
        f'  <text x="{py_axis(8.20)}" y="{pz_axis(2.10)}" fill="#F8FDFA" font-size="10" font-weight="bold">'
        f'Lower Y (8.10..11.30 cm): 2xAA (1.65 cm -> 0.09 cm spare)</text>'
    )

    # Notes at bottom
    lines.append(
        f'  <text x="{ox}" y="{svg_h - 32}" fill="#CEF5E4" font-size="10.5">'
        f'Layer stack: Front wall (0.20 cm) + Switches/Display (0.70 cm) + Perfboard (0.16 cm) + Solder/Wire (0.20 cm) + Rear Bay (1.74 cm) + Back wall (0.20 cm) = 3.20 cm</text>'
    )
    lines.append(
        f'  <text x="{ox}" y="{svg_h - 14}" fill="#FF91A5" font-size="10.5" font-weight="bold">'
        f'COLLISION RULE: Never stack the AA holder over the DevKitC-1 in Z (would require 4.46 cm vs 3.20 cm budget).</text>'
    )
    lines.append("</svg>\n")
    return "\n".join(lines)


def generate_perfboard_markdown(layout: dict) -> str:
    pb = layout["perfboard"]
    env = layout["enclosure"]["owner_reference_cm"]
    buttons = layout["controls"]["buttons"]

    rows_md = []
    for b in buttons:
        state = "Soldered (`INPUT_PULLUP`)" if b["soldered"] else "Expansion (Unsoldered)"
        rows_md.append(
            f"| `{b['name']}` (`{b['label']}`) | `GPIO {b['gpio']}` | `{b['target_x_cm']:.2f}, {b['target_y_cm']:.2f}` | "
            f"`C{b['perfboard_col']:02d}, R{b['perfboard_row']:02d}` | `{b['snapped_x_cm']:.3f}, {b['snapped_y_cm']:.3f}` | "
            f"`{b['snap_offset_cm']:.3f} cm` | {state} |"
        )

    upper_md = []
    for comp in layout["z_stackup"]["rear_zones"]["upper_zone"]["components"]:
        c0, c1 = comp["perfboard_cols"]
        r0, r1 = comp["perfboard_rows"]
        upper_md.append(
            f"| {comp['name']} | `C{c0:02d}..C{c1:02d}`, `R{r0:02d}..R{r1:02d}` | "
            f"`{comp['width_cm']:.2f} × {comp['height_cm']:.2f} × {comp['depth_cm']:.2f} cm` | "
            f"`{comp['clearance_cm']:.2f} cm` |"
        )

    aa = layout["z_stackup"]["rear_zones"]["lower_zone"]["components"][0]

    return f"""<!-- GENERATED FROM hardware/layout.json - DO NOT EDIT BY HAND -->
# Perfboard Placement Map (`31 × 46` Holes) & Mechanical Stack-Up

- **Source of Truth:** [`hardware/layout.json`](../../hardware/layout.json)
- **Evidence Status:** `NOT_TESTED` (analytical layout and grid-snap check; physical caliper survey required before printing final shells)
- **Visual Diagrams:**
  - Top View SVG (`1:1 cm` grid): [`docs/assets/layout_top_view.svg`](../assets/layout_top_view.svg)
  - Side Section Z-Stackup SVG: [`docs/assets/layout_side_section.svg`](../assets/layout_side_section.svg)
  - Point-to-Point Soldering Table: [`docs/wiring/wire_list.csv`](wire_list.csv)

---

## 1. Envelope & Perfboard Coordinate System (All Dimensions in Centimetres)

| Parameter | Value (`cm`) | Notes |
| :--- | :--- | :--- |
| Enclosure Outer Envelope | `{env['width_cm']:.2f} × {env['height_cm']:.2f} × {env['thickness_cm']:.2f} cm` | Origin `(0.00, 0.00, 0.00)` at top-left of front face (`+X` right, `+Y` down, `+Z` front-to-back) |
| Perfboard Grid Size | `31 columns (C01..C31) × 46 rows (R01..R46)` | Standard `0.254 cm` (`2.54 mm`) through-hole pitch |
| Perfboard Physical Size | `{pb['width_cm']:.3f} × {pb['height_cm']:.3f} × {pb['thickness_cm']:.2f} cm` | Centered inside shell with board top-left at `({pb['board_top_left_cm'][0]:.3f}, {pb['board_top_left_cm'][1]:.3f}) cm` |
| Hole `(C01, R01)` Coordinate | `({pb['hole_c01_r01_cm'][0]:.3f}, {pb['hole_c01_r01_cm'][1]:.3f}) cm` | General formula: `x_cm = 0.590 + (col - 1) * 0.254`, `y_cm = 0.485 + (row - 1) * 0.254` |
| Corner Mounting Holes (`M2`) | `(C02, R02)`, `(C30, R02)`, `(C02, R45)`, `(C30, R45)` | Four corner standoffs clear of button clusters and display ribbon |
| Display Bezel Window | `x = 0.80..8.00 cm`, `y = 0.70..7.10 cm` (`7.20 × 6.40 cm`) | Surrounds the `2.4"` (`6.10 cm` diagonal, `4.90 × 3.67 cm` active area) ST7789 TFT |

---

## 2. Button Coordinates Snapped to the `0.254 cm` Perfboard Grid

Every front-panel button target coordinate is snapped to the nearest `0.254 cm` perfboard hole (`C01..C31`, `R01..R46`). For the 10 front-face buttons, the maximum snap offset is `0.129 cm` (`1.29 mm`), easily absorbed by standard 3D-printed button cap flanges. Shoulder buttons `L` and `R` sit on the top edge (`y = 0.00 cm`) and actuate right-angle tactile switches soldered in row `R01` (`y = 0.485 cm`).

| Button Macro | GPIO | Target `(x, y)` (`cm`) | Nearest Hole `(Col, Row)` | Snapped `(x, y)` (`cm`) | Grid Snap Offset | Physical Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
{chr(10).join(rows_md)}

---

## 3. Tactile Switch Footprint Analysis (`1.20 × 1.20 cm` vs `0.60 × 0.60 cm`)

On the ABXY diamond (centered at `(6.60, 9.20) cm` with `±1.00 cm` axial offsets), the diagonal center-to-center distance between adjacent buttons (`A↔B`, `B↔Y`, `Y↔X`, `X↔A`) is:

$$d = \\sqrt{{1.00^2 + 1.00^2}} = 1.414\\text{{ cm}}$$

| Switch Option | Switch Body Size (`cm`) | Adjacent ABXY Body Gap (`cm`) | Perfboard Leg Spacing Verdict |
| :--- | :--- | :--- | :--- |
| **Option A (Recommended):** `6 × 6 × 7 mm` Through-Hole Tactile Switch | `0.60 × 0.60 cm` | **`0.814 cm`** (`8.14 mm`) | **Spacious (`2–3` free holes between adjacent switches).** Easy for a first-time builder to hand-solder without solder bridges. |
| **Option B:** `OMRON B3F-4055` (`12 × 12 mm` Tactile Switch + B32 Cap) | `1.20 × 1.20 cm` | **`0.214 cm`** (`2.14 mm`) | **Tight (`1` hole = `0.254 cm` between adjacent switch legs).** Fits geometrically without overlapping, but leaves minimal soldering iron clearance on the ABXY diamond. |

---

## 4. Rear Component Placement & Z-Axis Stack-Up (`3.20 cm` Total Thickness)

### 4.1 Six-Layer Z-Axis Budget

| Layer # | Z Range (`cm`) | Thickness (`cm`) | Description |
| :--- | :--- | :--- | :--- |
| **1** | `0.00 .. 0.20 cm` | `0.20 cm` | Front enclosure wall and recessed display lens |
| **2** | `0.20 .. 0.90 cm` | `0.70 cm` | Tactile switch bodies + button caps (`0.70 cm`) and ST7789 panel (`0.50 cm`) on the front side of the perfboard |
| **3** | `0.90 .. 1.06 cm` | `0.16 cm` | FR-4 perfboard substrate (`31 × 46` holes) |
| **4** | `1.06 .. 1.26 cm` | `0.20 cm` | Rear through-hole solder joints and flat `28–30 AWG` point-to-point wiring |
| **5** | `1.26 .. 3.00 cm` | `1.74 cm` | Rear electronics & battery cavity (**split into Upper Y and Lower Y zones**) |
| **6** | `3.00 .. 3.20 cm` | `0.20 cm` | Back enclosure wall and removable battery door |

### 4.2 Rear Cavity XY Split (Preventing Z-Axis Collision)

> [!IMPORTANT]
> **Critical Z-Stackup Rule:** Stacking the `1.65 cm` tall AA battery holder directly on top of the `1.35 cm` tall `ESP32-S3-DevKitC-1-N16R8` in Z would require `0.20 + 0.70 + 0.16 + 0.20 + 1.35 + 1.65 + 0.20 = 4.46 cm` (`1.26 cm` thicker than the `3.20 cm` shell). Keeping the DevKitC-1 in the **Upper Y Zone** (`y = 1.00..7.60 cm`, behind the display) and the AA holder in the **Lower Y Zone** (`y = 7.80..11.80 cm`, behind the flat solder pins of the D-pad and ABXY buttons) eliminates the collision completely.

| Rear Zone (`y` Range) | Component | Perfboard Holes | Envelope (`W × H × D cm`) | Spare Z Clearance (vs `1.74 cm` cavity) |
| :--- | :--- | :--- | :--- | :--- |
{chr(10).join(upper_md)}
| **Lower Y (`7.80..11.80 cm`)** | {aa['name']} | Chassis bay (`x = 1.50..7.30 cm`, `y = 8.10..11.30 cm`) | `{aa['width_cm']:.2f} × {aa['height_cm']:.2f} × {aa['depth_cm']:.2f} cm` | `{aa['clearance_cm']:.2f} cm` |
"""


def generate_wire_list_rows(layout: dict) -> list[dict]:
    return [
        {
            "wire_id": "W01",
            "signal": "3V3_DISPLAY",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "3V3",
            "from_hole": "C03,R05",
            "to_component": "ST7789_2.4_TFT",
            "to_pin": "VCC + BLK",
            "to_hole": "C15,R03",
            "wire_color": "Red",
            "length_cm": "4.5",
            "gauge_awg": "26",
            "status": "SOLDERED",
            "notes": "3.3V logic power and full-brightness backlight (+0.1 uF ceramic decoupling cap to GND at display header)",
        },
        {
            "wire_id": "W02",
            "signal": "GND_DISPLAY",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GND",
            "from_hole": "C13,R05",
            "to_component": "ST7789_2.4_TFT",
            "to_pin": "GND",
            "to_hole": "C14,R03",
            "wire_color": "Black",
            "length_cm": "4.0",
            "gauge_awg": "26",
            "status": "SOLDERED",
            "notes": "Run adjacent to or loosely twisted with W03 (TFT_SCK) to minimize 80 MHz SPI loop inductance",
        },
        {
            "wire_id": "W03",
            "signal": "TFT_SCK (FSPI_CLK)",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO12",
            "from_hole": "C03,R18",
            "to_component": "ST7789_2.4_TFT",
            "to_pin": "SCL/SCK",
            "to_hole": "C16,R03",
            "wire_color": "Yellow",
            "length_cm": "5.2",
            "gauge_awg": "28",
            "status": "SOLDERED",
            "notes": "Keep <= 6.0 cm; optional 22-47 Ohm series damping resistor at GPIO12 pin",
        },
        {
            "wire_id": "W04",
            "signal": "TFT_MOSI (FSPI_D)",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO11",
            "from_hole": "C03,R17",
            "to_component": "ST7789_2.4_TFT",
            "to_pin": "SDA/MOSI",
            "to_hole": "C17,R03",
            "wire_color": "Blue",
            "length_cm": "5.2",
            "gauge_awg": "28",
            "status": "SOLDERED",
            "notes": "Shared FSPI MOSI bus; keep <= 6.0 cm",
        },
        {
            "wire_id": "W05",
            "signal": "TFT_CS",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO10",
            "from_hole": "C03,R16",
            "to_component": "ST7789_2.4_TFT",
            "to_pin": "CS",
            "to_hole": "C20,R03",
            "wire_color": "Orange",
            "length_cm": "5.5",
            "gauge_awg": "28",
            "status": "SOLDERED",
            "notes": "Add 10 kOhm pull-up resistor from GPIO10 to 3V3 so display stays deselected during reset",
        },
        {
            "wire_id": "W06",
            "signal": "TFT_DC",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO8",
            "from_hole": "C03,R12",
            "to_component": "ST7789_2.4_TFT",
            "to_pin": "DC",
            "to_hole": "C19,R03",
            "wire_color": "Green",
            "length_cm": "5.0",
            "gauge_awg": "28",
            "status": "SOLDERED",
            "notes": "Data/Command select line",
        },
        {
            "wire_id": "W07",
            "signal": "TFT_RST",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO9",
            "from_hole": "C03,R15",
            "to_component": "ST7789_2.4_TFT",
            "to_pin": "RES/RST",
            "to_hole": "C18,R03",
            "wire_color": "White",
            "length_cm": "5.0",
            "gauge_awg": "28",
            "status": "SOLDERED",
            "notes": "Hardware panel reset line",
        },
        {
            "wire_id": "W08",
            "signal": "3V3_SD",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "3V3",
            "from_hole": "C03,R05",
            "to_component": "MicroSD_Breakout",
            "to_pin": "VCC (3.3V)",
            "to_hole": "C16,R06",
            "wire_color": "Red",
            "length_cm": "4.0",
            "gauge_awg": "26",
            "status": "SOLDERED",
            "notes": "Add 10 uF + 0.1 uF decoupling caps across VCC/GND at SD slot; verify MISO <= 3.3V per docs/wiring/02-sd-card.md",
        },
        {
            "wire_id": "W09",
            "signal": "GND_SD",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GND",
            "from_hole": "C13,R05",
            "to_component": "MicroSD_Breakout",
            "to_pin": "GND",
            "to_hole": "C16,R07",
            "wire_color": "Black",
            "length_cm": "3.5",
            "gauge_awg": "26",
            "status": "SOLDERED",
            "notes": "Common ground reference for SD SPI",
        },
        {
            "wire_id": "W10",
            "signal": "SD_SCK (FSPI_CLK)",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO12",
            "from_hole": "C03,R18",
            "to_component": "MicroSD_Breakout",
            "to_pin": "CLK/SCK",
            "to_hole": "C16,R08",
            "wire_color": "Yellow",
            "length_cm": "4.2",
            "gauge_awg": "28",
            "status": "SOLDERED",
            "notes": "Shared FSPI clock with ST7789 via SpiArbiter",
        },
        {
            "wire_id": "W11",
            "signal": "SD_MOSI (FSPI_D)",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO11",
            "from_hole": "C03,R17",
            "to_component": "MicroSD_Breakout",
            "to_pin": "MOSI/DI",
            "to_hole": "C16,R09",
            "wire_color": "Blue",
            "length_cm": "4.2",
            "gauge_awg": "28",
            "status": "SOLDERED",
            "notes": "Shared FSPI MOSI with ST7789 via SpiArbiter",
        },
        {
            "wire_id": "W12",
            "signal": "SD_MISO (FSPI_Q)",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO15",
            "from_hole": "C13,R10",
            "to_component": "MicroSD_Breakout",
            "to_pin": "MISO/DO",
            "to_hole": "C16,R10",
            "wire_color": "Purple",
            "length_cm": "3.0",
            "gauge_awg": "28",
            "status": "SOLDERED",
            "notes": "CRITICAL: Measure idle voltage <= 3.30V before connecting to GPIO15",
        },
        {
            "wire_id": "W13",
            "signal": "SD_CS",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO13",
            "from_hole": "C03,R19",
            "to_component": "MicroSD_Breakout",
            "to_pin": "CS",
            "to_hole": "C16,R11",
            "wire_color": "Grey",
            "length_cm": "4.5",
            "gauge_awg": "28",
            "status": "SOLDERED",
            "notes": "Add 10 kOhm pull-up resistor from GPIO13 to 3V3 so SD card stays deselected during reset",
        },
        {
            "wire_id": "W14",
            "signal": "BTN_UP",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO4",
            "from_hole": "C03,R08",
            "to_component": "SW_UP",
            "to_pin": "Terminal A",
            "to_hole": "C07,R31",
            "wire_color": "Green",
            "length_cm": "6.5",
            "gauge_awg": "30",
            "status": "SOLDERED",
            "notes": "Active-low INPUT_PULLUP; Terminal B connects to common button GND bus",
        },
        {
            "wire_id": "W15",
            "signal": "BTN_DOWN",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO5",
            "from_hole": "C03,R09",
            "to_component": "SW_DOWN",
            "to_pin": "Terminal A",
            "to_hole": "C07,R40",
            "wire_color": "Green",
            "length_cm": "8.5",
            "gauge_awg": "30",
            "status": "SOLDERED",
            "notes": "Active-low INPUT_PULLUP; Terminal B connects to common button GND bus",
        },
        {
            "wire_id": "W16",
            "signal": "BTN_LEFT",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO6",
            "from_hole": "C03,R10",
            "to_component": "SW_LEFT",
            "to_pin": "Terminal A",
            "to_hole": "C03,R35",
            "wire_color": "Green",
            "length_cm": "7.0",
            "gauge_awg": "30",
            "status": "SOLDERED",
            "notes": "Active-low INPUT_PULLUP; Terminal B connects to common button GND bus",
        },
        {
            "wire_id": "W17",
            "signal": "BTN_RIGHT",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO7",
            "from_hole": "C03,R11",
            "to_component": "SW_RIGHT",
            "to_pin": "Terminal A",
            "to_hole": "C12,R35",
            "wire_color": "Green",
            "length_cm": "7.5",
            "gauge_awg": "30",
            "status": "SOLDERED",
            "notes": "Active-low INPUT_PULLUP; Terminal B connects to common button GND bus",
        },
        {
            "wire_id": "W18",
            "signal": "BTN_A",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO16",
            "from_hole": "C13,R11",
            "to_component": "SW_A",
            "to_pin": "Terminal A",
            "to_hole": "C29,R35",
            "wire_color": "Blue",
            "length_cm": "8.0",
            "gauge_awg": "30",
            "status": "SOLDERED",
            "notes": "Active-low INPUT_PULLUP; Terminal B connects to common button GND bus",
        },
        {
            "wire_id": "W19",
            "signal": "BTN_B",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO17",
            "from_hole": "C13,R12",
            "to_component": "SW_B",
            "to_pin": "Terminal A",
            "to_hole": "C25,R39",
            "wire_color": "Blue",
            "length_cm": "8.5",
            "gauge_awg": "30",
            "status": "SOLDERED",
            "notes": "Active-low INPUT_PULLUP; Terminal B connects to common button GND bus",
        },
        {
            "wire_id": "W20",
            "signal": "BTN_START",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO18",
            "from_hole": "C13,R13",
            "to_component": "SW_START",
            "to_pin": "Terminal A",
            "to_hole": "C19,R44",
            "wire_color": "Yellow",
            "length_cm": "8.5",
            "gauge_awg": "30",
            "status": "SOLDERED",
            "notes": "Active-low INPUT_PULLUP; Terminal B connects to common button GND bus",
        },
        {
            "wire_id": "W21",
            "signal": "BTN_SELECT",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO21",
            "from_hole": "C13,R16",
            "to_component": "SW_SELECT",
            "to_pin": "Terminal A",
            "to_hole": "C13,R44",
            "wire_color": "Yellow",
            "length_cm": "7.5",
            "gauge_awg": "30",
            "status": "SOLDERED",
            "notes": "Active-low INPUT_PULLUP; Terminal B connects to common button GND bus",
        },
        {
            "wire_id": "W22",
            "signal": "BTN_GND_BUS",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GND",
            "from_hole": "C13,R28",
            "to_component": "SW_UP..SW_SELECT",
            "to_pin": "Terminal B (Daisy-Chain)",
            "to_hole": "C07,R32..C29,R36",
            "wire_color": "Black",
            "length_cm": "14.0",
            "gauge_awg": "28",
            "status": "SOLDERED",
            "notes": "Common ground return daisy-chained across all button Terminal B pins",
        },
        {
            "wire_id": "W23",
            "signal": "BTN_X (OPTIONAL)",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO14",
            "from_hole": "C03,R20",
            "to_component": "SW_X",
            "to_pin": "Terminal A",
            "to_hole": "C25,R31",
            "wire_color": "Orange",
            "length_cm": "7.0",
            "gauge_awg": "30",
            "status": "UNSOLDERED_EXPANSION",
            "notes": "Configured in config.h (INPUT_PULLUP); not soldered on current 8-button board",
        },
        {
            "wire_id": "W24",
            "signal": "BTN_Y (OPTIONAL)",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO42",
            "from_hole": "C13,R23",
            "to_component": "SW_Y",
            "to_pin": "Terminal A",
            "to_hole": "C21,R35",
            "wire_color": "Orange",
            "length_cm": "5.0",
            "gauge_awg": "30",
            "status": "UNSOLDERED_EXPANSION",
            "notes": "Configured in config.h (GPIO_IN1_REG); not soldered on current 8-button board",
        },
        {
            "wire_id": "W25",
            "signal": "BTN_L (OPTIONAL)",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO47",
            "from_hole": "C03,R24",
            "to_component": "SW_L",
            "to_pin": "Terminal A",
            "to_hole": "C05,R01",
            "wire_color": "White",
            "length_cm": "6.5",
            "gauge_awg": "30",
            "status": "UNSOLDERED_EXPANSION",
            "notes": "Configured in config.h (GPIO_IN1_REG); top-edge right-angle switch",
        },
        {
            "wire_id": "W26",
            "signal": "BTN_R (OPTIONAL)",
            "from_component": "ESP32-S3-DevKitC-1",
            "from_pin": "GPIO48",
            "from_hole": "C03,R25",
            "to_component": "SW_R",
            "to_pin": "Terminal A",
            "to_hole": "C27,R01",
            "wire_color": "White",
            "length_cm": "9.0",
            "gauge_awg": "30",
            "status": "UNSOLDERED_EXPANSION",
            "notes": "Configured in config.h (GPIO_IN1_REG; shares WS2812 LED on DevKitC-1 v1.0); top-edge right-angle switch",
        },
    ]


def main() -> int:
    layout = load_layout()
    TOP_SVG_PATH.parent.mkdir(parents=True, exist_ok=True)
    PERFBOARD_DOC_PATH.parent.mkdir(parents=True, exist_ok=True)

    TOP_SVG_PATH.write_text(generate_top_view_svg(layout), encoding="utf-8", newline="\n")
    SIDE_SVG_PATH.write_text(generate_side_section_svg(layout), encoding="utf-8", newline="\n")
    PERFBOARD_DOC_PATH.write_text(generate_perfboard_markdown(layout), encoding="utf-8", newline="\n")

    rows = generate_wire_list_rows(layout)
    fieldnames = [
        "wire_id",
        "signal",
        "from_component",
        "from_pin",
        "from_hole",
        "to_component",
        "to_pin",
        "to_hole",
        "wire_color",
        "length_cm",
        "gauge_awg",
        "status",
        "notes",
    ]
    with WIRE_CSV_PATH.open("w", encoding="utf-8", newline="\n") as fh:
        writer = csv.DictWriter(fh, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)

    print(f"Generated {TOP_SVG_PATH.relative_to(REPO_ROOT)}")
    print(f"Generated {SIDE_SVG_PATH.relative_to(REPO_ROOT)}")
    print(f"Generated {PERFBOARD_DOC_PATH.relative_to(REPO_ROOT)}")
    print(f"Generated {WIRE_CSV_PATH.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
