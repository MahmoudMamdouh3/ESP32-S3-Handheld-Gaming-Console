# BMO Enclosure Model and 3D Printing

This page describes the parametric enclosure design and its current verification scope. All physical dimensions in this guide are given in **centimetres (`cm`)** (only the `2.4"` display diagonal uses inches; CAD scripts internally operate in millimetres).

The numbers below are CAD design inputs (`NOT_TESTED` on physical prints), not caliper measurements of the soldered handheld. `physical_fit_status` and `perfboard.survey_status` in [`generate_bmo_enclosure.py`](../hardware/enclosure/generate_bmo_enclosure.py) remain `WAITING` until the board and modules are measured. See [Hardware Reference](hardware.md), [`hardware/layout.json`](../hardware/layout.json), [`docs/wiring/perfboard-layout.md`](wiring/perfboard-layout.md), and [`hardware_measurements.md`](hardware_measurements.md).

---

## 1. Reference Layout vs. Compact CAD Shell

Two enclosure envelopes are tracked while awaiting the owner's caliper survey and decision (`docs/owner-decisions.md` Decision #3):

| Parameter | Owner Reference Layout ([`hardware/layout.json`](../hardware/layout.json)) | Compact CAD Generator ([`generate_bmo_enclosure.py`](../hardware/enclosure/generate_bmo_enclosure.py)) |
| :--- | :--- | :--- |
| **Outer Envelope (`W × H × D`)** | **`8.80 × 12.40 × 3.20 cm`** (`88 × 124 × 32 mm`) | `8.20 × 12.40 × 2.60 cm` (`82 × 124 × 26 mm`) |
| **Perfboard Envelope** | **`7.87 × 11.68 × 0.16 cm`** (`31 × 46` holes at `0.254 cm` pitch) | `5.00 × 7.00 × 0.16 cm` (`50 × 70 mm` partial board) |
| **Display Bezel Window** | `7.20 × 6.40 cm` (`x = 0.80..8.00 cm`, `y = 0.70..7.10 cm`) | `6.00 × 4.60 cm` centered window |
| **2.4" Active Display Opening** | `4.90 × 3.67 cm` (`2.4"` diagonal) | `5.00 × 3.75 cm` (`2.4"` nominal profile) |
| **D-Pad / ABXY Centers** | `(2.20, 9.20) cm` / `(6.60, 9.20) cm` | Matched relative button cluster |
| **Battery Bay (`W × H × D`)** | **`5.80 × 3.20 × 1.65 cm`** (2×AA NiMH in Lower Y Zone `y = 8.10..11.30 cm`) | `5.40 × 4.70 × 1.60 cm` (note: `5.40 cm` is shorter than a `5.80 cm` AA holder) |
| **Z-Stackup Verification** | **Verified (`3.20 cm` total, `0` collisions)** — see [`layout_side_section.svg`](assets/layout_side_section.svg) | `2.60 cm` total (`2.18 cm` internal cavity) |

- **Top-View Diagram (`1:1 cm` scale):** [`docs/assets/layout_top_view.svg`](assets/layout_top_view.svg)
- **Side-Section Z-Stackup Diagram:** [`docs/assets/layout_side_section.svg`](assets/layout_side_section.svg)
- **31×46 Perfboard Placement Map:** [`docs/wiring/perfboard-layout.md`](wiring/perfboard-layout.md)
- **Point-to-Point Soldering Wire List:** [`docs/wiring/wire_list.csv`](wiring/wire_list.csv)

---

## 2. Generated Printable Meshes

The parametric CAD generator builds seven closed, separate printable meshes:

| Output | Features |
| :--- | :--- |
| `bmo_front_shell.stl` | Flush display rebate, button apertures, keyed guides, shoulder openings, USB-C and MicroSD access cuts, debossed side logos, seven-hole side grilles, board standoffs, and case screw receivers |
| `bmo_back_shell.stl` | Split-shell rear, five vent slots, matching side cuts and logos, battery-bay opening, side rails, board mounts, and case/door screw posts |
| `bmo_battery_door.stl` | Removable cover plate with four screw tabs and clearance holes |
| `bmo_dpad.stl` | Rounded cross, neck, retaining plate, and central pivot |
| `bmo_buttons_xyab.stl` | Four caps with retaining flanges and anti-rotation keys |
| `bmo_buttons_start_select.stl` | Two pill caps with guide stems and retaining flanges |
| `bmo_buttons_lr.stl` | Two top-edge actuators with broad caps, stems, and retaining flanges |

The left and right sides use block `B`, `M`, and `O` cuts at `2.20 cm`, `0.60 cm`, and `-1.00 cm` from the body center. Each side has a seven-hole cluster centered `4.30 cm` from the body center. These are matching design coordinates in the WebGL view (`simulator/bmo_enclosure_meshes.js`) and the printable shell. The MicroSD opening sits on the left wall at `y = -3.40 cm` below the `BMO` lettering, and the USB-C opening sits on the bottom edge (`x = 0.00 cm, y = -6.20 cm`) aligned with the vertical `ESP32-S3-DevKitC-1-N16R8` board envelope. Expansion buttons `X`, `Y`, `L`, and `R` are supported in firmware (`config.h`) but are not soldered to the present 8-button board.

### Display Window and Glass

`display.active_window_profiles` contains nominal visible openings for `2.0"` (`4.20 × 3.15 cm`), `2.4"` (`5.00 × 3.75 cm`), and `2.8"` (`5.80 × 4.35 cm`) panels. These are window profiles, not measured module-package sizes. The generator cuts the active window through the shell, then cuts a `0.10 cm` (`1.0 mm`) deep rebate around it. The preview lens is `0.10 cm` thick and has a `0.10 cm` border around the active window; a `0.018 cm` (`0.18 mm`) radial fit gap leaves the outer lens surface flush with the front datum. Test a short fit coupon with the chosen material before printing a full shell.

---

## 3. Build and Export Commands

Use Python with the repository's development dependencies:

```powershell
python -m pip install -r requirements-dev.txt
python scripts/generate_hardware_layout_artifacts.py
python hardware/enclosure/generate_bmo_enclosure.py
python scripts/export_enclosure_web_meshes.py
```

The CAD script writes seven STL files and `bmo_enclosure_assembly.glb` under `hardware/enclosure/build/` (ignored by Git). STL internal units are millimetres (`10 mm = 1.00 cm`); the GLB scene uses metres (`0.01 m = 1.00 cm`) and names each printable part plus `bmo_display_lens_preview`. Meanwhile, `scripts/export_enclosure_web_meshes.py` updates `simulator/bmo_enclosure_meshes.js` so the interactive Three.js console and disassembly internals mode (`bmo_internals_3d.js`) render the same CSG shells in centimetres.

Do not edit generated STL, GLB, SVG, or `bmo_enclosure_meshes.js` files by hand.

---

## 4. Geometry and Layout Verification (`HOST_TESTED`)

- `tests/test_enclosure_generator.py` checks all seven STL source meshes for closed surfaces, consistent winding, positive volume, finite coordinates, and successful import into Manifold3D. It checks pairwise intersections between the seven assembled parts, repeated export geometry, metre-scale GLB bounds, and clearance between the display lens and shell rebate.
- `tests/test_hardware_layout.py` and `tests/test_pin_snapshot.py` verify that [`hardware/layout.json`](../hardware/layout.json), [`firmware/BmoGameboy/src/hal/config.h`](../firmware/BmoGameboy/src/hal/config.h), [`firmware/BmoGameboy/src/hal/pins_check.h`](../firmware/BmoGameboy/src/hal/pins_check.h), and [`docs/hardware.md`](hardware.md) agree on every pin, envelope dimension, grid-snapped switch hole, and Z-stackup clearance.

These host checks establish geometric closure and layout consistency (`HOST_TESTED`). They do not establish physical print accuracy, screw pull-out strength, button travel, or fit around the hand-soldered board (`NOT_TESTED` physically).

---

## 5. Print Setup Suggestions (`NOT_TESTED`)

| Item | Starting Point |
| :--- | :--- |
| Material | PETG for impact tolerance; PLA+ for a first fit check; ABS or resin only with a printer/process suitable for that material |
| Layer Height | `0.012–0.016 cm` (`0.12–0.16 mm`) for the side lettering and shell curves |
| Perimeters | At least 4 |
| Infill | About 25% gyroid for shell regions that need infill |
| Fit Coupon | Print samples for `0.018–0.020 cm` (`0.18–0.20 mm`) display and button gaps before selecting final clearances |

Print both shell halves with the split rim on the build plate and the exterior face upward. This keeps support marks off the front and rear faces; inspect the display rebate, port tunnels, and internal posts for bridges.

---

## 6. Fastener and Mechanical Starting List

| Part | Design Provision | Selection Still Required |
| :--- | :--- | :--- |
| Front/rear shell | Four `0.22 cm` (`2.2 mm`) rear clearance passages, `0.16 cm` (`1.6 mm`) front pilot bores, `0.62 cm` (`6.2 mm`) bosses | Confirm screw type, length, engagement, and printed-pilot fit; `M2` self-tapping screws are a starting option |
| Battery door | Four `0.22 cm` cover holes, recessed tabs, and `0.16 cm` internal pilot bores | Confirm screw length, head shape, tab clearance, and repeated-open wear |
| Perfboard | `7.87 × 11.68 cm` (`31 × 46` holes in reference layout) or `5.00 × 7.00 cm` (compact CAD shell) | Measure board holes, solder peaks (`<= 0.20 cm`), module heights, and fastener heads first |
| Buttons | Captive flanges; `0.60 × 0.60 cm` (`6 × 6 mm`) tactile switches recommended (`0.814 cm` ABXY gap vs `0.214 cm` with `1.20 × 1.20 cm` OMRON) | Measure the actual switch body, plunger height, actuator travel, and clearance to solder joints |
| Display | `0.10 cm` lens, `0.018 cm` adhesive value, and `0.020 cm` rebate fit gap | Choose glass, gasket, or adhesive and verify its compressed thickness |

---

## 7. Caliper Survey Checklist (`WAITING`)

All physical fields below are `WAITING`. Record measurements in centimetres (`cm`) using the evidence process in [`hardware_measurements.md`](hardware_measurements.md) before ordering or printing a final shell.

| Measurement | Status | Capture (in `cm`) |
| :--- | :--- | :--- |
| Perfboard width, height, thickness, hole pitch (`0.254 cm`), mounting-hole centers, and edge clearances | `WAITING` | Include headers, components, and the widest solder joints |
| `ESP32-S3-DevKitC-1-N16R8` carrier length (`~6.33 cm`), width (`~2.54 cm`), installed height (`~0.85–1.35 cm`), and antenna keep-out | `WAITING` | Measure the installed module and surrounding wire route |
| Tallest component and solder peak on both board faces | `WAITING` | Verify rear solder peaks stay `<= 0.20 cm` |
| `2.4"` ST7789 glass outer size, active area (`~4.90 × 3.67 cm`), PCB (`~4.28 × 7.72 cm`), flex exit direction, and header pins | `WAITING` | Measure the exact display module, not only its advertised `2.4"` diagonal |
| Button switch body (`0.60 cm` vs `1.20 cm`), plunger height, actuation travel, and soldered hole coordinates | `WAITING` | Include the fitted caps and underside solder joints |
| USB-C receptacle shell and mating cable overmold clearance | `WAITING` | Measure the installed connector, including shell and board edge |
| MicroSD breakout (`~2.40 × 4.20 × 0.40 cm`), card insertion depth, and finger clearance | `WAITING` | Include the card at full insertion and removal positions |
| `2×AA` (`~5.80 × 3.20 × 1.65 cm`) fits the current Lower Y zone; **`3×AA` (`~5.80 × 4.80 × 1.65 cm`) collides (`-0.19 cm` Y overlap / `1.26 cm` Z crash in [`hardware/layout_3xaa_variant.json`](../hardware/layout_3xaa_variant.json)) and needs a `~0.60–1.00 cm` taller shell** | `WAITING` | Measure your chosen holder with loaded Eneloop cells (disconnected) |
| Boost converter (`~2.90 × 2.30 × 0.75 cm` `Adafruit #2030` or `~2.03 × 1.52 × 0.50 cm` `Pololu U3V70F5`/`U3V40F5`), `>= 2 A` switch, `MF-R300`/`MF-R250` polyfuse, and wire bend radius | `WAITING` | Confirm Upper Y Zone placement beside DevKitC-1 |
| Screw heads, thread type (`M2`), usable engagement, and printed boss fit | `WAITING` | Run a material-specific coupon before final assembly |
