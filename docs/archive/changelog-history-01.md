# Archived changelog (history archived 2026-11-07)

# Changelog
All notable changes to the ESP32-S3-Handheld-Gaming-Console project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Battery status truthfulness fix] - 2026-11-07
### Changed
- **Battery API + diagnostics (`firmware/BmoGameboy/src/core/battery.h`, `battery.cpp`, `display_emu.cpp`)**:
  - When `FEATURE_BATTERY_MONITOR` is `0`, the firmware now reports a truthful no-battery state instead of inventing a 100% charge and a 3300 mV value.
  - The diagnostics screen shows `BATTERY: NOT FITTED (USB power only)` and the top-right header uses a USB/N-A indicator instead of a fake battery bar.
  - Verified with `python scripts/validate_repo.py` and `python -m tools.guardian diff-audit` in this session.

## [DOOM allocation guard fix] - 2026-11-07
### Changed
- **DOOM vendor allocation guard (`firmware/BmoGameboy/src/vendor/doom/src/i_system.c`, `m_misc.c`)**:
  - Replaced bare heap allocations with PSRAM-aware or zeroed allocation paths for the ESP32 port.
  - Removed fatal `I_Error()` calls from string-allocation failures so the engine returns a soft failure instead of aborting on low-memory paths.
  - Verified with `python scripts/validate_repo.py` and `python -m tools.guardian diff-audit` in this session.

---

## [Software Design Document v5.0 Source-Verified Repository Specification] - 2026-11-07
### Changed
- **Software Design Document (`docs/software-design-document.md`)**:
  - Rewrote and expanded into a 17-section source-verified repository specification (v5.0) cross-checked against all firmware headers, vendor C cores, simulator modules, and verification tools for external multi-agent review.
  - Documented the Environment Truth Matrix (Physical ESP32-S3 Board vs. Wokwi vs. Browser 3D/WASM Simulator vs. Native x64 C Harness), physical hardware and electrical ground truth (Quad-SPI Flash vs. Octal-SPI PSRAM, ST7789 `MADCTL = 0xA8` BGR565 wire format, MicroSD 5V LDO & MISO 5V back-powering physics, 8.8×12.4×3.2 cm enclosure and 2.54 mm perfboard coordinates), complete GPIO pin matrix and dual-register (`GPIO_IN_REG` / `GPIO_IN1_REG`) polling, repository file encyclopedia, 10-step boot sequence, dual-core FreeRTOS display pipeline, and complete 8-state FSM with exact button transitions and `SELECT + ...` hotkeys.
  - Verified and documented exact binary structures (`BmoIndexHeader` 36 B / `BmoIndexEntry` 72 B with `BMOIDX01` v1 magic; `SaveStateHeader` 60 B with `0x31535342` `"BSS1"` v1 magic and `.s01`–`.s05` slot paths), all 18 `BmoFace::BmoExpression` states and 13 `HarmonicSpring` variables, all 14 emulator wrappers and vendor C struct definitions (`gb_s`, `agnes_t`, `sms_context_t`, `pce_context_t`, `atari_context_t`, `pico_context_t`, `genesis_t`, `snes_t`, `wswan_t`, `ngp_t`, `lynx_t`, `colem_t`), public C/C++ symbol catalog, 25-point performance registry (`PERF-01`..`PERF-25`), and step-by-step core extensibility playbook.
- **README, Wiring Guides & Doc Verifier (`README.md`, `docs/wiring/*.md`, `tools/guardian/core/doc_verifier.py`)**:
  - Fixed save-state extension reference in `README.md` from `.st0`–`.st4` to `.s01`–`.s05` and listed all in-game `SELECT + ...` hotkeys and 6 Pause Menu actions.
  - Clarified in `docs/wiring/01-display-and-buttons.md` and `docs/wiring/02-sd-card.md` that the 8 base buttons are physically soldered now while `X (14), Y (42), L (47), R (48)` are supported in firmware/Wokwi/simulator and pending physical soldering.
  - Added `docs/software-design-document.md` and `docs/wiring/*.md` to `DocTruthVerifier` target files.
- **Rules & Knowledge Index**:
  - Re-bundled single-file consolidated ruleset v8 (`docs/consolidated-rules.md`).
  - Synchronized AI Knowledge Graph (`AGENT_KNOWLEDGE_GRAPH.json`) and Context Index.
### Changed
- **Wiring Guides (`docs/wiring/01-display-and-buttons.md`, `docs/wiring/02-sd-card.md`)**:
  - Updated to full 12-button console input specification (`BTN_UP=4`, `BTN_DOWN=5`, `BTN_LEFT=6`, `BTN_RIGHT=7`, `BTN_A=16`, `BTN_B=17`, `BTN_START=18`, `BTN_SELECT=21`, `BTN_X=14`, `BTN_Y=42`, `BTN_L=47`, `BTN_R=48`).
  - Documented dual-register polling architecture (`GPIO_IN_REG` for GPIO 0–31, `GPIO_IN1_REG` for GPIO 32–48).
  - Detailed physical 2.54 mm perfboard geometry, square-grid ABXY diamond coordinates, Select/Start pill placement, and top-edge right-angle shoulder switch mounting.
  - Documented shared FSPI bus wiring, explicit pin parameter initialization contract, and MISO 5V back-powering protection.
- **Hardware Notes (`docs/hardware-notes.md`)**:
  - Synchronized engine list to all 14 active production emulator engines (Peanut-GB, Walnut-CGB, Agnes, doomgeneric, SMSPlus, PCE, Stella Atari 2600, PICO-8 runtime, Genesis, SNES, WonderSwan, Neo Geo Pocket, Lynx, ColecoVision).
  - Added Section 12 detailing the canonical Adventure Time BMO chassis reference specifications (8.8 × 12.4 × 3.2 cm, 5.0 × 3.8 cm display window, side 7-hole acoustic speakers and B-M-O arm sockets, rear 5 cooling vents and 4-screw access panel, and internal stackup).
- **Software Design Document (`docs/software-design-document.md`)**:
  - Updated Hardware Capability Matrix and hardware topology diagram to reflect all 12 console buttons.
  - Documented all 14 emulator cores and dual-register polling in Section 7 and Section 8.
- **Repository Documentation (`README.md`, `docs/consolidated-rules.md`)**:
  - Synchronized display specification to 2.4" ST7789 IPS TFT across all documents.
  - Regenerated consolidated ruleset v8 via `scripts/bundle_rules.py`.
