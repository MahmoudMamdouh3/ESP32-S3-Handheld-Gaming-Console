# Archived changelog (history archived 2026-11-07)

## [Four Extra Buttons and 3D Console Simulator] - 2026-11-04
### Added
- **Button pins (`firmware/BmoGameboy/src/core/config.h`)**: `BTN_X=14`, `BTN_Y=42`, `BTN_L=47`, `BTN_R=48`, all configured as `INPUT_PULLUP`. Avoiding boot strapping, PSRAM (33-37), display (8-12), and SD (13, 15) pins.
- **Button driver (`buttons.h`, `buttons.cpp`)**: `Buttons::Index` expanded to 12 buttons (`UP, DOWN, LEFT, RIGHT, A, B, START, SELECT, X, Y, L, R`). `Buttons::update()` reads both `GPIO_IN_REG` (<32) and `GPIO_IN1_REG` (>=32).
- **Core mappings**: SNES emulator pad registers widened to 16-bit (`JOY1L`/`JOY1H` returning X, Y, L, R). Genesis maps X to C. DOOM maps L/R to strafing and X/Y to weapon cycling.
- **Diagnostics screen (`display_emu.cpp`)**: Live hardware tester matrix updated to display LT, RT, Y, X.
- **Wokwi circuit (`diagram.json`)**: Four pushbuttons wired to GPIO 14, 42, 47, 48 and GND.
- **3D Interactive WebGL Console Model (`tools/bmo_simulator/bmo_3d_console.js`, `three.min.js`, `OrbitControls.js`)**: Real-scale Three.js 3D handheld enclosure built to exact centimeter hardware reference dimensions:
  - Chassis: 8.8 W × 12.4 H × 3.2 D cm, 0.6 cm corner radius with 1.5 cm asymmetric bottom-right corner.
  - Screen window: 5.0 × 3.8 cm (scaled to active 4.88 × 3.66 cm 4:3 ST7789 display active area). Dynamic `THREE.CanvasTexture` projects native 320×240 companion and emulator rendering in real time.
  - Square-grid ABXY diamond aligned to 2.54 mm perfboard holes: X at (6.6, 7.4), Y at (5.6, 8.4), A at (7.6, 8.4), B at (6.6, 9.4) cm.
  - D-Pad: 2.8 × 2.8 cm centered at (2.2, 8.4) cm with directional arm switches.
  - Select / Start pills: 1.4 × 0.5 cm centered at y=11.0 cm.
  - Top edge L/R shoulder triggers: 2.2 × 0.6 cm bumpers with tactile spring depression.
  - Cutouts: Top USB-C (1.0 × 0.45 cm), left MicroSD slot (1.3 × 0.25 cm), right power slide switch (1.0 × 0.4 cm), rear 4×AA battery compartment door (3.4 × 6.2 cm).
  - Raycaster pointer interaction: Clicking 3D buttons depresses them and routes input to WASM/firmware state.
  - Camera presets: Front, 3D Iso, Top (Shoulders & USB-C), Left (SD Slot), Right (Power), Back (4×AA Door), and Reset.
  - Theme switching: Real-time shell material updates across BMO Teal, DMG Gray, Atomic Purple, and OLED Black.
  - View mode toggle between 3D console model and flat layout.

### Changed
- `DocTruthVerifier` updated to match pin macros with word boundaries (`BTN_L` no longer false-positive matches `BTN_LEFT`).
- `docs/hardware-notes.md` updated: Flash Mode corrected to QIO 80MHz with OPI PSRAM; power notes updated to document AA/NiMH boost architecture vs LiPo TP4056 restrictions.
- `tools/bmo_simulator/bmo_3d_console.js` color & lighting architecture overhauled: Switched WebGL rendering pipeline to `LinearEncoding` and `NoToneMapping` to prevent double-gamma bleaching, guaranteeing 100% color-picker parity with 2D CSS tokens (`#5FB49C` chassis, `#FFE033` D-pad, `#4CAF50` X, `#F48FB1` Y, `#52A6BA` A, `#E8175D` B, `#1A4B42` bezel). Normalized studio lighting rig to 1.00 front-face irradiance (0.74 ambient + 0.32 key * 0.80 cos). Added bezel header/footer textures, top vent ridges, contact drop shadows, and engraved button sub-labels.
- **Adventure Time Canonical BMO Design & Tactile Fixes (`tools/bmo_simulator/`)**:
  - Fixed L/R shoulder trigger flickering: mounted bumpers completely on the top rim ($Y \ge 6.20\text{ cm}$, $Z = 0.0\text{ cm}$), completely eliminating coplanar mesh overlap and Z-fighting with the front chassis bevel. Triggers feature smooth rounded outer corners, crisp embossed front facet labels, and downward tactile travel.
  - Removed top vent ridge lines from front face and hid `.chassis-top-ridges` in `simulator.css`.
  - Removed front diagonal speaker grille and implemented canonical 7-hole acoustic speaker arrays on both left and right sides.
  - Added iconic bold vertical "B M O" typography with integrated circular arm socket port to both left and right sides.
  - Re-anchored Select and Start pill buttons to canonical Adventure Time layout: horizontally side-by-side (`[ - ] [ - ]`) directly under the yellow cross D-pad in the lower-left area.
  - Added iconic cassette/floppy drive slot and circular blue eject button directly beneath the screen bezel.
  - Overhauled rear chassis (`tools/bmo_simulator/bmo_3d_console.js`) to match canonical Adventure Time reference: 5 vertical cooling vent capsule slits (`| | | | |`) across the upper back, raised beveled access/battery plate with molded perimeter seam and 4 corner metal slotted screws across the middle back, and horizontal indented finger latch pocket across the lower back. Adjusted global drop shadow offset to eliminate rear occlusion.
- **3D Visual & Tactile Polish (`tools/bmo_simulator/bmo_3d_console.js`)**:
  - Removed dark outer stroke and reduced oversized shadow discs from action button textures, producing pure, borderless action button caps with subtle contact occlusion.
  - Repositioned L/R top shoulder bumper meshes flush with the front face ($Z = +1.60\text{ cm}$) and added an expanded invisible raycast hit box ($2.6 \times 1.1 \times 2.8\text{ cm}$) ensuring 100% click hit reliability from any camera angle. Implemented tactile 3D click animation (downward travel $Y - 0.22\text{ cm}$, housing sink $Z - 0.12\text{ cm}$, and microswitch tilt $X - 0.12\text{ rad}$).
  - Symmetrically aligned Select and Start labels: rotated text meshes to -25.2° matching the pill angle, offset along the perpendicular vector ($d = 0.65\text{ cm}$) with bilateral symmetry ($X = \pm 1.0\text{ cm}$) and generous clearance.
  - Upgraded pill buttons to extruded rounded stadium capsule geometry with smooth semicircular caps.
- **Studio Lighting & Tone Controls (`tools/bmo_simulator/`)**: Added interactive Light intensity (30-200%), Saturation (0-200%), and Temperature (Cool Blue to Warm Amber, -100 to +100) sliders with instant Reset in the simulator stage. Controls dynamically update Three.js 3D light intensities, Planckian Kelvin color tinting, and GPU compositor canvas saturation. Initialized calibration automatically on page load.
- Knowledge graph, AST index, and documentation regenerated.

### Limitation
- The four buttons are not soldered yet. Unwired pins read as released.
- Genesis uses a 3-button pad; the core has no 6-button (X/Y/Z/Mode) protocol.

## [SD Library Audit and Safer Deployment] - 2026-11-03
### Changed
- **FAT32 deployment (`scripts/deploy_to_sd.py`)**: Replaced size-only, concurrent in-place writes with an explicit-destination dry run, volume-label guard, content hashing, sequential staged copies, post-copy verification, and ROM-index invalidation. The tool deploys only supported root-level source files, does not delete destination files, and reports files that would be copied or replaced.
- **ROM menu PSRAM allocation (`firmware/BmoGameboy/BmoGameboy.ino`)**: Made the 36,000-entry index and pointer arrays an all-or-fallback allocation so a partial allocation cannot overrun the 32-entry fallback array.
- **Console and PICO-8 documentation (`README.md`)**: Corrected `.sg` to the ColecoVision / SG-1000 core and documented that the current PICO-8-style runtime does not execute Lua carts or `.p8.png` files.

### Verification
- `F:\` is FAT32 and Windows reports the volume healthy; all scanned files were readable, and installed box art passed firmware size/header checks and matched corresponding local-source hashes.
- The card contains 11,184 of 31,326 local game files; 20,142 are not present. Two same-name GBC files differ in size from the source. 12,659 box-art files from the current `roms/boxart` source are absent on the card.
- The root box-art source contains 14,168 exact-size raw images and 14,168 BMPs with 64×64, 24-bit BI_RGB headers; no invalid files were found. `sdcard/boxart` contains 28,224 filenames, all present in the root source, with 112 source assets not in staging.
- A basic ROM-header audit flagged 26 files for manual/core-level validation; its heuristics are not proof that those files are corrupt. A privileged filesystem checker could not be run in this environment.
- No ROM or cover-art binaries were copied, committed, or pushed.

## [Firmware-Backed Browser BMO Companion] - 2026-11-03
### Added
- **Actual firmware BMO face renderer in WebAssembly (`tools/bmo_simulator/bmo_face_firmware.cpp`, `bmo_face_wasm.cpp`)**: Compiles `firmware/BmoGameboy/src/core/bmo_face.cpp` and calls the real 320×240 SDF rendering, expression, gaze, blink, speech, petting, and dance APIs.
- **BMO companion controls (`tools/bmo_simulator/simulator.js`)**: UP from the carousel opens the companion; D-pad gaze, A wink, B blush/hold-to-exit, SELECT dialogue, UP dance, START return, and the 30-second idle timeout follow the firmware companion flow.
- **Firmware API regression coverage (`tests/test_native_cores_execution.py`)**: Checks rendered face colors, quote reveal, wink state, pet counter, dance toggle, and framebuffer output.

### Changed
- The browser's previous hand-drawn static BMO face was replaced with the firmware SDF output; the firmware-derived interaction and enablement are labeled separately from the browser-rendered menu shell.

### Limitation
- The BMO face/expression engine is the firmware source, but the browser still ports the surrounding device state machine and overlays. It does not run the ESP32 scheduler or reproduce physical ST7789/SPI/SD behavior; a literal 1:1 hardware replica is not claimed.

## [Playable Browser ColecoVision Core] - 2026-11-03
### Added
- **ColecoVision WebAssembly gameplay (`tools/bmo_simulator/colem_wasm.cpp`, `scripts/build_wasm_cores.py`)**: Compiles the same vendored ColecoVision core used by firmware and executes uploaded `.col` ROMs with the firmware joystick mapping and centered 256×192 viewport.
- **Generated ColecoVision regression ROM (`tests/test_native_cores_execution.py`)**: Exercises frame progression, framebuffer output, controller mapping, session save/restore, and coexistence with the existing Game Boy, NES, and Atari browser cores.

### Changed
- The browser accepts `.gb`, `.nes`, `.a26`, and `.col` ROMs. System carousel support labels and documentation identify remaining firmware-only systems.

### Known limitation
- Browser gameplay supports only the firmware Peanut-GB, Agnes, Stella Atari 2600, and ColecoVision cores. It does not execute the ESP32 firmware, the other 10 console cores, physical SPI/SD timing, or reproduce the physical enclosure electrically. Wokwi still uses an ILI9341 display substitute rather than the physical ST7789.

### Verification
- `python -m unittest tests.test_native_cores_execution.TestNativeCoresExecutionAndPhaseD`: `Ran 10 tests in 7.500s`; `OK`.
- `python scripts/validate_repo.py`: `Flash: 5,698,412 / 16,777,216 bytes (34.0%)`; `SRAM: 218,904 / 327,680 bytes (66.8%)`; `VALIDATION PASSED in 26729 ms`.
- Browser smoke test: uploaded a generated 8 KiB ColecoVision ROM, observed the core running beyond 400 frames at 256×192, and confirmed a ROM checksum was reported.
