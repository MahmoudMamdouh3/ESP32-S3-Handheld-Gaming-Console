# Archived known-issue history (source archived 2026-11-07)

# 6. Known Issues, Technical Debt & Hardware Limitations Registry

> **System Target:** ESP32-S3-N16R8 (240MHz Xtensa LX7 dual-core, 16MB Quad SPI Flash, 8MB Octal PSRAM).
> **Physical Peripherals:** ST7789VW 240×320 IPS TFT (80MHz FSPI), MicroSD Slot (25MHz SPI), 8× GPIO Tactile Buttons.
> **Repository Scope:** The latest generated manifest records 372 first-party source/rule/test files (excluding `.venv` and external `zig` toolchain), 1,503 indexed symbols across all 14 production emulator engines (`0` stubs remaining), Flash 33.9% (`5,687,672` bytes), and SRAM 66.8% (`218,784` bytes) as of 2026-10-21.
> **Verification Status:** Firmware compile (`FlashMode=qio, PSRAM=opi`), repository validator (`validate_repo.py`), all 113 unit + Guardian tests (`guardian verify`), `guardian doctor`, and full-source `guardian cppcheck` (`0 errors`) all passed with exit code 0 in this session.

---

## Latest Agent Handoff - 2026-11-07 (DOOM allocation guard fix)

- **Status: VERIFIED_HOST** — `python scripts/validate_repo.py` passed all phases cleanly after the DOOM allocation guard fix. `python -m tools.guardian diff-audit` reported the prior fatal-allocation findings resolved in the ESP32 port and the quoted output was clean.
- **Scope:** `firmware/BmoGameboy/src/vendor/doom/src/i_system.c`, `m_misc.c`.
- **What changed:** replaced bare heap allocation paths with PSRAM-aware/zeroed allocation and removed fatal `I_Error()` calls on string-allocation failures so low-memory conditions fall back without aborting the engine.

## Latest Agent Handoff - 2026-11-07 (battery status is truthful when no battery is fitted)

- **Status: VERIFIED_HOST** — `python scripts/validate_repo.py` passed after the battery-status fix. `python -m tools.guardian validate` and the repo validation gate both stayed clean.
- **Scope:** `firmware/BmoGameboy/src/core/battery.h`, `battery.cpp`, `display_emu.cpp`.
- **What changed:** when `FEATURE_BATTERY_MONITOR` is `0`, the battery API now reports `0` instead of invented values and the diagnostics/UI show `BATTERY: NOT FITTED (USB power only)` with a USB/N-A glyph instead of a fake charge bar. The code never calls `analogRead(1)` in that disabled path.

## Latest Agent Handoff - 2026-11-07 (Software Design Document v5.0 Source-Verified Repository Specification)

- Rewrote `docs/software-design-document.md` into a 17-section source-verified repository specification (v5.0) cross-checked line-by-line against `BmoGameboy.ino`, `src/core/*.h/.cpp`, `src/emulators/*.h/.cpp`, `src/vendor/*/*.h`, `tools/bmo_simulator/`, `diagram.json`, and `tools/guardian/`:
  - Fixed all prior documentation discrepancies: corrected save state extensions from `.st0`–`.st4` to `/saves/<base>.s01`–`.s05` (`SaveStateHeader` magic `0x31535342` `"BSS1"` v1, 60 B), corrected `/.bmo_index` binary layout (`BMOIDX01` v1, 36 B `BmoIndexHeader`, 72 B `BmoIndexEntry`), documented all 18 real `BmoFace::BmoExpression` enum states and 13 `HarmonicSpring` variables, corrected FSM button routing (`START` in carousel opens `STATE_DIAGNOSTICS`; `START` in `STATE_GAME_MENU` toggles Top Picks filter; `UP`/`DOWN` steps ±1 game; `LEFT`/`RIGHT` jumps A–Z), and corrected PICO-8 (`src/emulators/emu_pico.*`, `src/vendor/pico/`) and Walnut-CGB (`src/engine/walnut_cgb/walnut_cgb.h`) paths.
  - Added complete per-core C++ wrapper and vendor C struct layouts (`gb_s`, `agnes_t`, `sms_context_t`, `pce_context_t`, `atari_context_t`, `pico_context_t`, `genesis_t`, `snes_t`, `wswan_t`, `ngp_t`, `lynx_t`, `colem_t`), joypad bitmasks, `.sav` and `.s01`–`.s05` multi-buffer persistence mechanics, 80 MHz FSPI viewport coordinates and wire timings, 25-point performance registry (`PERF-01`..`PERF-25`), and full public symbol reference.
- Updated `README.md` save state filenames (`.s01`–`.s05`), in-game `SELECT + ...` hotkeys, and 6-option Pause Menu controls; clarified soldered (8 base buttons) vs pending-solder (`X=14, Y=42, L=47, R=48`) status in `docs/wiring/01-display-and-buttons.md` and `docs/wiring/02-sd-card.md`.
- Expanded `DocTruthVerifier` (`tools/guardian/core/doc_verifier.py`) to scan `docs/software-design-document.md`, `docs/wiring/01-display-and-buttons.md`, and `docs/wiring/02-sd-card.md` for pin drift and obsolete symbol references.
- Re-bundled single-file consolidated ruleset v8 (`docs/consolidated-rules.md`) via `scripts/bundle_rules.py`.
- `VERIFIED_HOST`: `python scripts/validate_repo.py` passed all 9 phases (Phase 0–8) cleanly. `python -m tools.guardian doctor`, `ui-audit`, `diff-audit`, and `verify` passed with 0 blocking issues.

## Previous Agent Handoff - 2026-11-06 (Comprehensive Documentation Synchronization)

- Synchronized wiring guides (`docs/wiring/01-display-and-buttons.md`, `docs/wiring/02-sd-card.md`) to the complete 12-button console input specification (`BTN_UP=4`, `BTN_DOWN=5`, `BTN_LEFT=6`, `BTN_RIGHT=7`, `BTN_A=16`, `BTN_B=17`, `BTN_START=18`, `BTN_SELECT=21`, `BTN_X=14`, `BTN_Y=42`, `BTN_L=47`, `BTN_R=48`).
- Documented dual-register polling architecture: GPIO 0–31 via `GPIO_IN_REG` and GPIO 32–48 via `GPIO_IN1_REG`.
- Documented physical 2.54 mm perfboard geometry, square-grid ABXY diamond coordinates, lower-left Select/Start pill placement, and top-edge right-angle shoulder switch mounting.
- Synchronized `docs/hardware-notes.md` to all 14 active production emulator engines, and added Section 12 detailing the canonical Adventure Time BMO enclosure reference dimensions, side acoustic speaker arrays, flank B-M-O arm sockets, and rear cooling vents/battery access plate.
- Updated `docs/software-design-document.md` Hardware Capability Matrix, topology diagram, and core engine list.
- Regenerated `docs/consolidated-rules.md` via `scripts/bundle_rules.py`.
- `VERIFIED_HOST`: `python scripts/validate_repo.py` passed all 9 phases (Phase 0–8) cleanly. `python -m tools.guardian doctor`, `ui-audit`, and `diff-audit` passed with 0 blocking issues.

## Previous Agent Handoff - 2026-11-04 (Extra Buttons X, Y, L, R and 3D Console Simulator)

- Added `BTN_X=14`, `BTN_Y=42`, `BTN_L=47`, `BTN_R=48` to firmware, Wokwi, and the browser simulator. See `CHANGELOG.md` for the file list.
- Replaced the 2D flat chassis representation in the browser simulator with a full 3D interactive WebGL console body (`tools/bmo_simulator/bmo_3d_console.js`, `three.min.js`, `OrbitControls.js`) built to exact physical centimeter hardware dimensions (8.8 × 12.4 × 3.2 cm, ST7789 2.4" display active window, 2.54 mm perfboard square-grid ABXY diamond, L/R top shoulder bumpers, USB-C cutout, SD slot, power switch, rear 4×AA battery door, raycasting button depression, camera presets, and theme materials).
- Overhauled 3D console rendering pipeline in `bmo_3d_console.js`: eliminated double-gamma bleaching by switching to `LinearEncoding` and `NoToneMapping`, normalizing studio lighting to 1.00 front-face irradiance, ensuring 100% color-picker parity with 2D tokens (`#5FB49C` chassis, `#FFE033` D-pad, `#4CAF50` X, `#F48FB1` Y, `#52A6BA` A, `#E8175D` B, `#1A4B42` bezel). Added bezel frame textures with top "BMO COLOR"/"USB-C POWER" and bottom "BMO GAMEBOY", contact drop shadows, top vent ridges, and engraved button sub-labels.
- Fixed 3D tactile polish and alignment in `bmo_3d_console.js`: eliminated dark outer stroke rings on action buttons, moved L/R shoulder bumpers flush with front face with expanded raycast hit boxes and tactile click depression, aligned Select/Start labels symmetrically to -25.2° pill tilt with perpendicular clearance, and upgraded pill buttons to extruded stadium capsules.
- Aligned 3D model with canonical Adventure Time BMO character design: resolved L/R trigger flickering by mounting triggers purely on the top rim ($Y \ge 6.20\text{ cm}$, $Z = 0.0\text{ cm}$) without front-face/bevel overlap; removed top accent ridge lines and front diagonal speaker slits; added canonical 7-hole acoustic speaker arrays and bold vertical "B M O" typography with arm socket ports to both left and right sides; moved Select and Start pill buttons to canonical lower-left placement horizontally side-by-side directly under the yellow cross D-pad; added front cassette/floppy slot and blue eject button beneath the screen bezel; overhauled rear chassis with 5 vertical cooling vent capsule slits (`| | | | |`), raised beveled access/battery plate with 4 corner metal slotted screws, and horizontal indented finger latch pocket.
- Added interactive Studio Lighting, Saturation, and Color Temperature (Cool Blue to Warm Amber) sliders with instant Reset in the simulator stage. Live sliders dynamically adjust Three.js light intensities, light color temperature, and GPU compositor canvas saturation. Initialized calibration on load.
- `VERIFIED_HOST`: `python scripts/validate_repo.py` Phase 0 compiled the firmware (Flash 5,700,300 bytes, SRAM 218,936 bytes). `python -m tools.guardian verify` ran 147 tests, all OK. `python -m tools.guardian ui-audit` and `diff-audit` passed clean.
- `UNVERIFIED_HARDWARE`: the buttons are not soldered or tested on the physical board. GPIO 42/47/48 are read from `GPIO_IN1_REG`; confirm each press on the Diagnostics screen after wiring. GPIO48 also drives the onboard RGB LED on many DevKitC-1 boards.
- Genesis maps X to C only; the core has no 6-button protocol. DOOM weapon cycling is bound in `DoomEmu::begin()` after `doomgeneric_Create()`.

## Latest Agent Handoff — 2026-11-03 (Firmware BMO Companion and Playable Browser Cores)

- The browser compiles the firmware `BmoFace` SDF renderer and the vendored Peanut-GB, Agnes, Stella, and ColecoVision cores to WebAssembly. The BMO companion uses the real face/expression/gaze/dialogue/pet/dance functions; the console cores accept `.gb`, `.nes`, `.a26`, and `.col` uploads with firmware button mappings and viewports.
- The browser still ports the state machine, menu artwork, and companion overlays; it does not execute the ESP32 firmware or reproduce the physical ST7789/SPI/SD hardware. The other 10 firmware cores remain firmware-only. Browser game save state is a single in-memory snapshot.
- The UI lists only user-loaded ROMs and supports no other browser ROM formats. No game ROM is bundled.
- `VERIFIED_SIMULATOR`: The firmware BMO renderer and interaction APIs pass generated-WASM regression checks. A generated 8 KiB ColecoVision ROM was uploaded in the local browser and ran past 400 frames in the 256×192 viewport. Generated Game Boy, iNES, and Atari 2600 test ROMs were also exercised in prior browser smoke tests.
- `VERIFIED_HOST`: `python -m unittest tests.test_native_cores_execution.TestNativeCoresExecutionAndPhaseD` reported `Ran 10 tests in 7.500s` and `OK`. `python scripts/validate_repo.py` reported Flash `5,698,412 / 16,777,216` bytes (34.0%), SRAM `218,904 / 327,680` bytes (66.8%), and `VALIDATION PASSED in 26729 ms`.
- `IN_PROGRESS`: The one-to-one request is not complete. Four of 14 game cores are browser-playable, the menus still are not firmware execution, physical SPI/SD behavior is not modeled in the browser, and Wokwi has no ST7789 model.
- The Wokwi diagram has the ESP32-S3, button GPIOs, shared TFT/SD SPI lines, and separate chip selects from `config.h`. Wokwi lacks an ST7789 model and does not expose the display reset/backlight pins; it uses an ILI9341 visual substitute. Wokwi CLI execution requires `WOKWI_CLI_TOKEN`.
