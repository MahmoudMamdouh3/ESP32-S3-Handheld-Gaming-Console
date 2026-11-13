# BMO Enclosure Model and 3D Printing

This page describes the parametric enclosure design and its current verification scope. The numbers below are CAD design inputs, not measurements of the soldered handheld. `physical_fit_status` and `perfboard.survey_status` in [`generate_bmo_enclosure.py`](../tools/enclosure/generate_bmo_enclosure.py) remain `WAITING` until the board and modules are measured.

## Model layout

The starting envelope is **82 mm wide × 124 mm high × 26 mm thick**, with a 9 mm plan-view corner radius and 2.1 mm nominal shell walls. The shell splits at the center depth plane into front and rear halves. The nominal cavity depth is 21.8 mm before bosses, display parts, wiring, and internal hardware are included. These values sit within the requested handheld envelope; pocket comfort and thermal performance have not been measured.

The generator builds seven closed, separate printable meshes:

| Output | Features |
| :--- | :--- |
| `bmo_front_shell.stl` | Flush display rebate, button apertures, keyed guides, shoulder openings, USB-C and MicroSD access cuts, debossed side logos, seven-hole side grilles, board standoffs, and case screw receivers |
| `bmo_back_shell.stl` | Split-shell rear, five vent slots, matching side cuts and logos, battery-bay opening, side rails, board mounts, and case/door screw posts |
| `bmo_battery_door.stl` | Removable cover plate with four screw tabs and clearance holes |
| `bmo_dpad.stl` | Rounded cross, neck, retaining plate, and central pivot |
| `bmo_buttons_xyab.stl` | Four caps with retaining flanges and anti-rotation keys |
| `bmo_buttons_start_select.stl` | Two pill caps with guide stems and retaining flanges |
| `bmo_buttons_lr.stl` | Two top-edge actuators with broad caps, stems, and retaining flanges |

The left and right sides use block `B`, `M`, and `O` cuts at 22 mm, 6 mm, and −10 mm from the body center. Each side has a seven-hole cluster centered 43 mm from the body center. These are matching design coordinates in the WebGL view (`tools/bmo_simulator/bmo_enclosure_meshes.js`) and the printable shell. The MicroSD opening sits on the left wall at `y = −34 mm` below the `BMO` lettering, and the USB-C opening sits on the bottom edge (`x = 0, y = −62 mm`) aligned with the vertical ESP32-S3-DevKitC-1 board envelope. The button coordinates and 2.4-inch display window also match the simulator's centimetre coordinates. Expansion buttons X, Y, L, and R are supported in firmware but are not soldered to the present board.

### Display window and glass

`display.active_window_profiles` contains nominal visible openings for 2.0-inch (42 × 31.5 mm), 2.4-inch (50 × 37.5 mm), and 2.8-inch (58 × 43.5 mm) panels. These are window profiles, not measured module-package sizes. The generator cuts the active window through the shell, then cuts a 1 mm deep rebate around it. The preview lens is 1 mm thick and has a 1 mm border around the active window; a 0.18 mm radial fit gap leaves the outer lens surface flush with the 13 mm front datum. Test a short fit coupon with the chosen material before printing a full shell.

The GLB includes a dark lens preview. It does not include an ST7789 module, PCB, cable, gasket, or adhesive. Select the window profile only after measuring the panel glass, active area, flex, connector, and mounting points.

## Build and export

Use Python with the pinned mesh dependencies:

```powershell
python -m pip install -r tools/enclosure/requirements.txt
python tools/enclosure/generate_bmo_enclosure.py
python scripts/export_enclosure_web_meshes.py
```

The CAD script writes seven STL files and `bmo_enclosure_assembly.glb` under `tools/enclosure/build/`. That directory is ignored by Git. STL coordinates and dimensions are millimetres. The GLB scene uses metres and names each printable part plus `bmo_display_lens_preview`; vertex colors are embedded for inspection in Blender and glTF-compatible Unity importers. The GLB is an assembled, static preview, while `scripts/export_enclosure_web_meshes.py` updates `tools/bmo_simulator/bmo_enclosure_meshes.js` so the interactive Three.js console and disassembly internals mode (`bmo_internals_3d.js`) render the same CSG shells in centimetres.

The generator's configuration block is the only place to change enclosure dimensions, mounting envelopes, clearances, and profiles. Regenerate both the CAD files and the simulator web-mesh bundle after any value changes. Do not edit generated STL, GLB, or `bmo_enclosure_meshes.js` files by hand.

## Geometry checks

`tests/test_enclosure_generator.py` checks all seven STL source meshes for closed surfaces, consistent winding, positive volume, finite coordinates, and successful import into Manifold3D. It checks pairwise intersections between the seven assembled parts, repeated export geometry, metre-scale GLB bounds, and clearance between the display lens and shell rebate. The displayed screen lens is preview geometry and is not one of the seven STL parts.

These host checks establish mesh closure and fit between the configured parts. They do not establish print accuracy, screw pull-out strength, button travel, glass retention under impact, electrical safety, or fit around the soldered board.

## Print setup suggestions

The settings here are starting points for a slicer, not validated material or process settings:

| Item | Starting point |
| :--- | :--- |
| Material | PETG for impact tolerance; PLA+ for a first fit check; ABS or resin only with a printer/process suitable for that material |
| Layer height | 0.12–0.16 mm for the side lettering and shell curves |
| Perimeters | At least 4 |
| Infill | About 25% gyroid for shell regions that need infill |
| Fit coupon | Print samples for 0.18–0.20 mm display and button gaps before selecting final clearances |

Print both shell halves with the split rim on the build plate and the exterior face upward. This keeps support marks off the front and rear faces; inspect the display rebate, port tunnels, and internal posts for bridges. Print the battery door with its exterior face upward and support the four screw tabs only if the selected printer cannot bridge them cleanly. Print the D-pad with its visible cross face upward and support the hidden pivot as needed. Print the round and pill caps with their broad face on a smooth build surface; print the L/R caps with the labeled face upward and supports limited to the hidden stem side. Confirm dimensions with a small coupon before a full assembly.

The side `BMO` marks are recessed 0.5 mm into the shell. A multicolor printer can pause for a contrasting fill after the body color. For a single-color print, use a small brush or paint-fill, wipe the raised surface, and keep paint out of the speaker bores. Verify that the selected paint does not soften the printed material.

## Fastener and mechanical starting list

| Part | Current design provision | Selection still required |
| :--- | :--- | :--- |
| Front/rear shell | Four 2.2 mm rear clearance passages, 1.6 mm front pilot bores, 6.2 mm bosses | Confirm screw type, length, engagement, and printed-pilot fit; M2 self-tapping screws are a starting option |
| Battery door | Four 2.2 mm cover holes, recessed tabs, and 1.6 mm internal pilot bores | Confirm screw length, head shape, tab clearance, and repeated-open wear |
| Perfboard | 50 × 70 mm (5×7 cm) envelope; four front posts + upper rear mounts at 43 × 56 mm spacing (`y = −24 mm`) clear of battery bay | Measure board holes, solder peaks, module heights, and fastener heads first |
| Buttons | Captive flanges; radial running gap is configured around the action caps | Measure the actual switch body, plunger height, actuator travel, and clearance to solder joints |
| Display | 1 mm lens, 0.18 mm adhesive value, and 0.2 mm rebate fit gap | Choose glass, gasket or adhesive and verify its compressed thickness and temperature range |

Do not add heat-set inserts until the insert's barrel, length, installation temperature, and surrounding wall thickness are known. The current 1.6 mm pilot holes are starting geometry for self-tapping fasteners, not a thread specification. The 6 × 6 mm switch footprint is not confirmed for this board.

## Pocket layout and thermal limits

The 26 mm depth provides a nominal 21.8 mm internal cavity. A 54 × 47 × 16 mm battery bay is reserved for the planned three-AA NiMH holder and 5 V boost converter. Those items are not installed: the handheld is USB-C powered, and no holder, converter, power-path circuit, or battery monitor is selected. The bay is only an envelope. It does not prove that the cell holder, board, display, wires, and connector can fit together.

Seven side bores and five rear slots are open in the CAD shell to preserve the requested side appearance and provide possible air paths. The speaker is not installed and audio remains disabled. No airflow, case temperature, battery temperature, or component thermal test has been run. The openings do not establish cooling capacity. Keep `FEATURE_BATTERY_MONITOR=0` and `FEATURE_AUDIO=0` as recorded in [hardware notes](hardware-notes.md) and [Rule 01](../.agents/rules/01_hardware.md).

## Caliper survey checklist

All physical fields below are `WAITING`. Record measurements using the evidence process in [`hardware_measurements.md`](hardware_measurements.md) before changing the matching generator parameters or ordering a full print.

| Measurement | Status | Capture |
| :--- | :--- | :--- |
| Perfboard width, height, thickness, pitch, mounting-hole centers, and edge clearances | WAITING | Include headers, components, and the widest solder joints |
| ESP32-S3 module length/width/height and antenna keep-out | WAITING | Measure the installed module and surrounding wire route |
| Tallest component and solder peak on both board faces | WAITING | Record each component's position relative to board edges |
| ST7789 glass outer size, active area, flex width/exit direction, connector, and mounting holes | WAITING | Measure the exact display module, not its advertised diagonal |
| Button switch body, plunger height, actuation travel, and button-center positions | WAITING | Include the fitted caps and underside solder joints |
| USB-C receptacle shell and mating cable clearance | WAITING | Measure the installed connector, including shell and board edge |
| MicroSD breakout, card insertion depth, ejection travel, and finger clearance | WAITING | Include the card at full insertion and removal positions |
| Three-AA holder envelope, cell direction, contacts, and lead exit | WAITING | Measure the exact holder and loaded cells; do not connect them during a dimensional survey |
| Boost converter, switch, fuse, wiring, and bend radius | WAITING | Identify the selected part numbers before allocating the battery bay |
| Screw heads, thread type, usable engagement, and printed boss fit | WAITING | Run a material-specific coupon before final assembly |

After recording these measurements, update the corresponding values in `CONFIG`, regenerate all exports, rerun the enclosure tests, and inspect the assembly in Blender or Unity before slicing.
