<!-- GENERATED FROM hardware/layout.json - DO NOT EDIT BY HAND -->
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
| Enclosure Outer Envelope | `8.80 × 12.40 × 3.20 cm` | Origin `(0.00, 0.00, 0.00)` at top-left of front face (`+X` right, `+Y` down, `+Z` front-to-back) |
| Perfboard Grid Size | `31 columns (C01..C31) × 46 rows (R01..R46)` | Standard `0.254 cm` (`2.54 mm`) through-hole pitch |
| Perfboard Physical Size | `7.874 × 11.684 × 0.16 cm` | Centered inside shell with board top-left at `(0.463, 0.358) cm` |
| Hole `(C01, R01)` Coordinate | `(0.590, 0.485) cm` | General formula: `x_cm = 0.590 + (col - 1) * 0.254`, `y_cm = 0.485 + (row - 1) * 0.254` |
| Corner Mounting Holes (`M2`) | `(C02, R02)`, `(C30, R02)`, `(C02, R45)`, `(C30, R45)` | Four corner standoffs clear of button clusters and display ribbon |
| Display Bezel Window | `x = 0.80..8.00 cm`, `y = 0.70..7.10 cm` (`7.20 × 6.40 cm`) | Surrounds the `2.4"` (`6.10 cm` diagonal, `4.90 × 3.67 cm` active area) ST7789 TFT |

---

## 2. Button Coordinates Snapped to the `0.254 cm` Perfboard Grid

Every front-panel button target coordinate is snapped to the nearest `0.254 cm` perfboard hole (`C01..C31`, `R01..R46`). For the 10 front-face buttons, the maximum snap offset is `0.129 cm` (`1.29 mm`), easily absorbed by standard 3D-printed button cap flanges. Shoulder buttons `L` and `R` sit on the top edge (`y = 0.00 cm`) and actuate right-angle tactile switches soldered in row `R01` (`y = 0.485 cm`).

| Button Macro | GPIO | Target `(x, y)` (`cm`) | Nearest Hole `(Col, Row)` | Snapped `(x, y)` (`cm`) | Grid Snap Offset | Physical Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `BTN_UP` (`UP`) | `GPIO 4` | `2.20, 8.10` | `C07, R31` | `2.114, 8.105` | `0.086 cm` | Soldered (`INPUT_PULLUP`) |
| `BTN_DOWN` (`DOWN`) | `GPIO 5` | `2.20, 10.30` | `C07, R40` | `2.114, 10.391` | `0.125 cm` | Soldered (`INPUT_PULLUP`) |
| `BTN_LEFT` (`LEFT`) | `GPIO 6` | `1.10, 9.20` | `C03, R35` | `1.098, 9.121` | `0.079 cm` | Soldered (`INPUT_PULLUP`) |
| `BTN_RIGHT` (`RIGHT`) | `GPIO 7` | `3.30, 9.20` | `C12, R35` | `3.384, 9.121` | `0.115 cm` | Soldered (`INPUT_PULLUP`) |
| `BTN_A` (`A`) | `GPIO 16` | `7.60, 9.20` | `C29, R35` | `7.702, 9.121` | `0.129 cm` | Soldered (`INPUT_PULLUP`) |
| `BTN_B` (`B`) | `GPIO 17` | `6.60, 10.20` | `C25, R39` | `6.686, 10.137` | `0.107 cm` | Soldered (`INPUT_PULLUP`) |
| `BTN_X` (`X`) | `GPIO 14` | `6.60, 8.20` | `C25, R31` | `6.686, 8.105` | `0.128 cm` | Expansion (Unsoldered) |
| `BTN_Y` (`Y`) | `GPIO 42` | `5.60, 9.20` | `C21, R35` | `5.670, 9.121` | `0.106 cm` | Expansion (Unsoldered) |
| `BTN_SELECT` (`SELECT`) | `GPIO 21` | `3.60, 11.30` | `C13, R44` | `3.638, 11.407` | `0.114 cm` | Soldered (`INPUT_PULLUP`) |
| `BTN_START` (`START`) | `GPIO 18` | `5.20, 11.30` | `C19, R44` | `5.162, 11.407` | `0.114 cm` | Soldered (`INPUT_PULLUP`) |
| `BTN_L` (`L`) | `GPIO 47` | `1.60, 0.00` | `C05, R01` | `1.606, 0.485` | `0.485 cm` | Expansion (Unsoldered) |
| `BTN_R` (`R`) | `GPIO 48` | `7.20, 0.00` | `C27, R01` | `7.194, 0.485` | `0.485 cm` | Expansion (Unsoldered) |

---

## 3. Tactile Switch Footprint Analysis (`1.20 × 1.20 cm` vs `0.60 × 0.60 cm`)

On the ABXY diamond (centered at `(6.60, 9.20) cm` with `±1.00 cm` axial offsets), the diagonal center-to-center distance between adjacent buttons (`A↔B`, `B↔Y`, `Y↔X`, `X↔A`) is:

$$d = \sqrt{1.00^2 + 1.00^2} = 1.414\text{ cm}$$

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
| ESP32-S3-DevKitC-1-N16R8 (44-pin carrier) | `C03..C13`, `R04..R29` | `2.54 × 6.33 × 1.35 cm` | `0.39 cm` |
| MicroSD Breakout Module (3.3V SPI) | `C16..C25`, `R05..R21` | `2.40 × 4.20 × 0.40 cm` | `1.34 cm` |
| Pololu 5V Step-Up Boost Module (U3V40F5 / S7V7F5) | `C18..C26`, `R22..R28` | `2.03 × 1.52 × 0.50 cm` | `1.24 cm` |
| **Lower Y (`7.80..11.80 cm`)** | 2xAA NiMH Holder (Recommended) or 3xAA Holder | Chassis bay (`x = 1.50..7.30 cm`, `y = 8.10..11.30 cm`) | `5.80 × 3.20 × 1.65 cm` | `0.09 cm` |
