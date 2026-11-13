# Changelog

## [Interactive 3D Internals Teardown, CAD WebGL Fixes & Wiring Blueprint] - 2026-11-13

- Replaced primitive procedural WebGL box geometry in [`tools/bmo_simulator/bmo_3d_console.js`](tools/bmo_simulator/bmo_3d_console.js) with crease-split manifold CAD shell meshes exported by [`scripts/export_enclosure_web_meshes.py`](scripts/export_enclosure_web_meshes.py) to [`tools/bmo_simulator/bmo_enclosure_meshes.js`](tools/bmo_simulator/bmo_enclosure_meshes.js).
- Eliminated speaker-grille Z-fighting flicker using true through-punched shell bores (`matIdx = 1`) and recessed internal acoustic baffles, removed the stray top seam rectangle, relocated the MicroSD slot to `y = -34.0 mm` (below `"O"`, clearing `"B"`), moved the Dual USB-C cutout to the bottom edge (`x = 0.0`, `y = -62.0 mm`), and removed the stray counter ring inside `"O"` plus corrected `"M"` strokes in [`tools/enclosure/generate_bmo_enclosure.py`](tools/enclosure/generate_bmo_enclosure.py).
- Added [`tools/bmo_simulator/bmo_internals_3d.js`](tools/bmo_simulator/bmo_internals_3d.js) and UI controls in [`tools/bmo_simulator/index.html`](tools/bmo_simulator/index.html), [`tools/bmo_simulator/simulator.css`](tools/bmo_simulator/simulator.css), and [`tools/bmo_simulator/simulator.js`](tools/bmo_simulator/simulator.js): `Dissect / Internals` mode, `Bottom (USB-C)` / `Core PCB` presets, layer visibility checkboxes, `0-100%` explosion slider with animated M2 unscrewing and auto-play disassembly, 22 color-coded 3D jumper wires matching `firmware/BmoGameboy/src/core/config.h`, raycast Inspector, 7-step soldering sequence, and toggleable Planned Expansion overlay showing the `-9.3 mm` 3xAA rear Z conflict (`FEATURE_AUDIO=0`, `FEATURE_BATTERY_MONITOR=0`).
- Updated [`docs/enclosure-3d-printing.md`](docs/enclosure-3d-printing.md), [`docs/readme/details-01.md`](docs/readme/details-01.md), and regression tests in [`tests/test_enclosure_generator.py`](tests/test_enclosure_generator.py) and [`tests/test_native_cores_execution.py`](tests/test_native_cores_execution.py).

## [BMO Mode Mascot Test Bench in Browser Simulator] - 2026-11-13

### Added
- **BMO Mode WASM & Simulator Controls**:
  - Extended `tools/bmo_simulator/bmo_face_wasm.cpp` and `scripts/build_wasm_cores.py` exports with custom speech buffer calls (`bmo_face_speech_buffer`, `bmo_face_speech_capacity`, `bmo_face_say_buffer`), explicit dance tempo start/stop (`bmo_face_start_dance`, `bmo_face_stop_dance`), and quote inspection (`bmo_face_quote_count`, `bmo_face_quote_text`), then rebuilt `tools/bmo_simulator/bmo_playtest_cores.wasm`.
  - Expanded `tools/bmo_simulator/wasm_playtest_engine.js` with `BMO_EXPRESSIONS` / `BMO_EXPRESSION_NAMES` covering all 18 `BmoFace::BmoExpression` states (`IDLE` through `CELEBRATING`), `tickleBmo()`, `startBmoDance()`, `stopBmoDance()`, `sayBmoText()`, and `bmoQuotes()`.
  - Added **BMO Mode** (`#btn-bmo-mode`, `?state=bmo`) and the `#bmo-mode-panel` sidebar test bench (`index.html`, `simulator.css`, `simulator.js`) with live Buddy Lv / Happiness / Dance metrics, all 18 expression trigger buttons, instant action buttons (`Pet`, `Tickle`, `Wink + Sparkle`, `Bashful Blush`, `Sparkle Joy`, `Dance Party` + `60–220 BPM` slider), `2.5s` expression auto-cycle, interactive 2D gaze pad + 3×3 presets + direct canvas drag/pet, firmware quote selector, `63`-character custom speech input (with input-focus guard on simulator keybinds), and `Auto-sleep` / `On-screen control legend` toggles.
  - Added `test_11_bmo_mode_simulator_controls_and_wasm_bridge` in `tests/test_native_cores_execution.py` verifying `#bmo-mode-panel`, all 18 expression controls, BGR565 seafoam mint `#8AD5C3` unpacking, custom speech, tickle, and dance BPM execution in Node against `bmo_playtest_cores.wasm`.

### Fixed
- Fixed browser WASM loading stall ("LOADING FIRMWARE BMO..." / "Loading" forever) when `tools/bmo_simulator/index.html` is opened directly over `file://` by emitting `tools/bmo_simulator/bmo_playtest_cores.b64.js` from `scripts/build_wasm_cores.py` and falling back to `decodeEmbeddedWasm()` in `wasm_playtest_engine.js`.
- Fixed signed-distance polarity in 12 `smoothstepf(aaBand, 0.0f, ...)` calls in `firmware/BmoGameboy/src/core/bmo_face.cpp` (speech bubble, eyes, glints, mouth cavity/tongue/strokes, and particle borders) where `-d` inverted alpha inside SDF cull boxes, then rebuilt `bmo_playtest_cores.wasm` and `bmo_playtest_cores.b64.js`.
- Fixed silent WASM failure masking in `simulator.js` (`engineLoadError`, `requireBmoEngine()`, `#bmo-mode-status` `"WASM Error"`, and TFT `BMO WASM CORE FAILED TO LOAD` notice), added explicit `.wasm`/`.js` MIME mappings in `tools/bmo_simulator/run_simulator.py`, and fixed `.bmo-bpm-row .slider-label` spacing in `simulator.css`.
- Fixed byte-swapped BGR565 pixel unpacking in `WasmPlaytestEngine.renderBmoFrame()` (`wasm_playtest_engine.js`) so the firmware `BmoFace` background renders in seafoam mint (`#8AD5C3`) instead of swapped red/blue channels.
- Fixed speech-bubble text positioning and 2-line wrapping in `BmoSimulator.renderIdleMascotScreen()` (`simulator.js`) to fit inside the firmware SDF speech bubble (`px 95..225`, `py 25..45`).

### Verification
- Verified with Node WASM tests (including SDF corner pixel polarity assertions), Chromium headless rendering over `file://` and HTTP, host unit tests, Guardian UI-audit / verify / diff-audit, and full `scripts/validate_repo.py` including ESP32-S3 target compile (physical hardware remains unverified).

## [Repository bug/performance fixes and documentation reconciliation] - 2026-11-12

### Changed
- **Firmware bug and performance fixes**:
  - Added catalog-wide O((N + M) log N) box-art availability matching via `BoxArtLookup::CatalogMatcher` (`box_art_lookup.h`, `BoxArt::populateCatalogAvailability`) invoked after sorting in `SDCard::scanRoms()`, fixing legacy basename collision misses across subdirectories and cutting repeated SD directory probes.
  - Updated `DisplayEmu` to record streamed DMG/GBC frames into `s_streamCapBuf`, clear stale screenshot state in `resetLastFramebuffer()` and on Game Gear format edge cases, and render dark-text-safe `UI_WHITE` on coral `PLAY` footer pills.
  - Added PSRAM save-state staging buffers in `emu_lynx.cpp`, `emu_ngp.cpp`, `emu_peanut.cpp`, and `emu_walnut.cpp` so failed loads no longer corrupt active cartridge/emulator RAM before validation.
  - Fixed `BmoFace` speech bubble vertical dirty-clear bounds (`maxBoundY` in `bmo_face.cpp`), updated `bmo_face.h` header notes to reflect the 320x240 Octal PSRAM canvas, fixed `testSPISDCondention` in `unit_tests.cpp` to push a valid 240-pixel PSRAM scratch row via `DisplayEmu::pushPixels()`, and fixed `BmoGameboy.ino` `wadPath[72]` length guarding (`selectedEmulatorIndex = 3` inside the `wadLen` check) plus catalog/UI cache invalidation after SD retry/rebuild in Diagnostics.
- **Tools, simulator, and script fixes**:
  - Synchronized `scripts/deploy_sd_complete.py` to write `BMOIDX02` indexes matching `BmoIndex::catalogFingerprint`, validate filename collisions, copy atomically via `deploy_to_sd.py`, and remove unimplemented `.a78`/`.sgx` mappings from deploy/install scripts (`auto_install_romsets.py`, `deploy_to_sd.py`).
  - Fixed `tools/bmo_simulator/simulator.js` (`_activeMouseButton`, cached `_boundRenderLoop`), `wasm_playtest_engine.js` (draw last frame behind unloaded overlay), `console_history_data.js` (`.a26`), and `index.html` button telemetry copy (`8 wired + 4 optional`).
  - Expanded `tools/guardian/core/doc_verifier.py` to audit all active SDD/readme/wiring/top-level docs (`DESIGN.md`, `visualizations/README.md`), fixed `tools/guardian/core/elf_analyzer.py` toolchain version discovery, fixed Windows path handling in `scripts/generate_code_visualizations.py`, and updated `scripts/generate_ai_knowledge_base.py`, `scripts/install_git_hooks.py`, and `scripts/validate_repo.py` docstring.
- **Documentation and rule reconciliation**:
  - Reconciled `.agents/rules/` (`03`, `04`, `06`–`09`, `12`–`14`, `16`–`18`, `21`, `22`, `25`, `29`, `30`, `33`–`35`, `37`–`40`, `42`, `44`, `README.md`), `DESIGN.md`, `docs/software-design-document/section-10.md`, `docs/readme/details-01.md` & `details-02.md`, `docs/research_embedded_ui_frameworks_unevaluated.md`, and `docs/performance_report.md` (with Cppcheck) against live source, pinout, and test counts, then regenerated derived rule bundles, symbol tables, knowledge graphs, and code visualizations.

### Verification
- Host unit tests (`python -m unittest discover -s tests -v`), Guardian verification (`python -m tools.guardian verify`), AST/architecture audit, UI audit, diff audit, markdown link checker, color token verification, Cppcheck report (`0` findings), and full 9-phase `python scripts/validate_repo.py --require-target-compile` passed after final edits. Physical ESP32-S3 hardware remains unverified.

## [Logical and performance audit] - 2026-11-12

### Fixed
- Made SPI mutex initialization/acquisition/release fail-stop instead of reporting or tolerating unlocked access; added a native regression test.
- Moved ROM sorting and final pool sizing outside the shared-bus lock while retaining exclusion during SD operations.
- Reframed Guardian bus metrics as theoretical sequential/ideal-overlap estimates; removed unsupported DMA and frame-drop conclusions.
- Guarded DOOM ammo-array access when the fatal-error callback returns and fixed the remaining vendored DOOM Cppcheck findings; the deep scan now reports 0 findings.
- Corrected DOOM file-allocation and fatal-error fallthrough paths, format/varargs types, patch-column bounds, signed-shift behavior, and memory-file seeking; made Guardian accept Cppcheck's branch-limit informational messages when no findings exist.
- Corrected stale hard-coded memory health values and stopped presenting estimated ROM capacity as measured dynamic PSRAM usage.

### Documented
- Recorded Guardian baseline, confirmed findings, unresolved cold-scan limitation, and verification boundaries in [the audit handoff](docs/handoffs/2026-11-12-logical-performance-audit.md).

### Verification
- Native SPI arbiter regression, focused bus-model tests, Guardian verification (191 tests), Cppcheck (0 findings), and diff-audit passed. `python scripts/validate_repo.py` passed all nine phases and target compile (5,774,672 B app0, 253,728 B SRAM). No physical hardware test was run.

## [Clean-checkout CI fixes] - 2026-11-12

### Fixed
- Added the ROM file filter header to version control despite the generated-ROM-header ignore rule.
- Made local-only ROM headers optional in firmware and test compilation so clean checkouts do not require ignored content.
- Skip installed-game integrity checks when the gitignored `games/` directory is absent.
- Pin Pillow in CI, normalize both path separators in the box-art test helper, and keep the sanitizer probe focused on AddressSanitizer rather than an earlier UBSan report.
- Exclude downloaded toolchains from repository index inputs, regenerate bounded rules pages, and keep WASM test builds in temporary storage to avoid runner-dependent tracked binaries.
- Discover the Xtensa toolchain under Arduino CLI's configured or default data directory for live budget reports, and omit build-dependent ELF snapshots from committed knowledge indexes.

### Verification
- Clean-checkout compile passed with `Sketch uses 5192960 bytes`; the installed-games test skipped because its gitignored data directory was absent.
- `python -m unittest discover -s tests -v`: `Ran 159 tests in 39.895s`, `OK`.
- `python -m tools.guardian verify`, `ui-audit`, and `diff-audit` passed; diff-audit reported 0 critical violations and 0 documentation/rule drift issues.
- `python scripts/validate_repo.py` passed all nine phases in 184825 ms. Target compile used 5,774,632 / 16,777,216 bytes flash and 253,728 / 327,680 bytes SRAM.
- `python -m tools.guardian diff-audit` passed with 0 critical violations and 0 documentation/rule drift issues.
- Targeted host tests for the sanitizer probe, box-art paths, image pipeline, and native cores passed after the first remote run exposed Linux-only failures.
- The sanitizer, box-art, image-pipeline, and native-core test selectors passed after the CI fixes.
- The first remote run failed in host tests; later runs failed only at generated-artifact drift. Cross-platform ELF discovery fixed live budget analysis, and generated indexes now omit build-dependent snapshots while pointing to the live budget command. GitHub Actions run `43238623840` passed all workflow steps. Physical hardware verification is not claimed.
- Revalidation after integrating the main-branch enclosure and visualization work passed 164 host tests, 188 Guardian tests, all nine repository-validation phases, Guardian diff-audit, Markdown/color checks, and UI audit.

## [Production-readiness assessment and workstream handoff] - 2026-11-12

### Added
- Documented the 2026-11-12 production-readiness review, recorded design decisions (40 MHz in-spec display clock default with NVS 80 MHz opt-in and boot-button revert; isolated `fix/production-readiness` CI branch), and full specifications for workstreams `WS1`–`WS4` in [the production-readiness handoff](docs/handoffs/2026-11-12-production-readiness.md).
- Added native C++17 AddressSanitizer/UBSan test runner `build_and_run_native_cxx()` (`find_vcvars64()`, `find_native_cxx()`) in `tests/host_test_support.py` and verified ASan detection in `tests/test_host_test_support.py`.
- Added `scripts/generate_code_visualizations.py` and HTML/JSON/Mermaid views under `visualizations/`.

### Verification
- `python -m unittest tests.test_host_test_support -v` passed (1 test, ASan out-of-bounds detection confirmed). Full branch validation is recorded in the handoff and commit message.
- `python -m unittest discover -s tests` passed (`Ran 163 tests in 69.689s`, `OK`), including 5 enclosure geometry checks (`tests.test_enclosure_generator`) and 1 ASan native runner check (`tests.test_host_test_support`).
- `python -m tools.guardian verify` passed (`Ran 187 tests in 76.429s`, `OK`), `python scripts/check_markdown_links.py` passed (`0 issues`), and `python -m tools.guardian diff-audit` passed (`0` AST/drift issues).
- Full `python scripts/validate_repo.py` validation and target compile results are recorded in the commit verification notes and [the production-readiness handoff](docs/handoffs/2026-11-12-production-readiness.md). Physical fit, printing, thermal, and hardware tests remain unverified.

## [3D simulator and enclosure] - 2026-11-07

### Changed
- Reworked the WebGL console body, black display glass, button caps, shoulder legends, side BMO marks, speaker holes, and rear details to share the printable model's nominal envelope and feature coordinates.
- Added a parametric split-shell generator for seven printable STL parts and a metre-scale GLB assembly, with display rebate, button retainers, screw features, board placeholders, battery door tabs, ports, side cuts, and vents.
- Added generator tests and pinned mesh dependencies to host CI; documented print setup, export/import, clearances, and the required physical survey.

### Verification
- Generator tests, manifold imports, assembled-part intersection checks, and the five-angle browser review are recorded after final verification below. Physical fit, printing, and thermal tests are not claimed.

## [Audit remediation and repository handoff] - 2026-11-07

### Changed
- Added path-derived CRC32 keys for save files, states, screenshots, and box art, with matching Python tooling and deployment handling for nested game paths.
- Gated Game Boy, NES, and SNES battery persistence on cartridge metadata and disabled ordinary battery-RAM writes for volatile-memory cores.
- Added source-derived PSRAM budget reporting, bounded DOOM-zone sizing, index layout assertions, and validation of cached index entries.
- Added compact generated firmware-symbol pages, Guardian size and banned-language checks, and a bounded rules index generator.
- Updated firmware diagnostics and the browser simulator to show that the battery monitor is not fitted.
- Added `TFT_SPI_HZ`, `SD_SPI_HZ`, and a Diagnostics full-screen pattern to compare the configured 80 MHz display clock with a 40 MHz fallback.
- Corrected color-token values and the 14-core save description, archived oversized engineering histories and the SDD, and converted Windows-absolute Markdown links to repository-relative links.
- Added Guardian checks for Markdown links and regenerated the compact firmware symbol index and consolidated rules pages.
- Added Diagnostics stack high-water display/logging and a Guardian rule for local arrays over 1,024 bytes; split the root README into two bounded detail pages.
- Made the generated knowledge graph reproducible by excluding generation timestamps, self-indexed output files, and live host microbenchmark samples; current host measurements remain available through `python -m tools.guardian bench-host`.

### Verification
- Python and host-compiled check results, repository validator output, and compile status are recorded in [the remediation handoff](docs/handoffs/2026-11-07-remediation.md).
- Fresh pinned ESP32-S3 builds passed at the configured 80 MHz and the 40 MHz comparison clock. The 80 MHz app image is 5,774,784 bytes and leaves 2,613,824 bytes in app0; global data is 253,728 bytes (77.4% of SRAM).
- `python scripts/validate_repo.py --require-target-compile` passed all nine phases; `python -m unittest discover -s tests -v` passed all 157 tests; Guardian diff-audit/UI audit, color-token verification, and Markdown-link verification passed.
- The local CI workflow is defined, but no remote CI run or physical board test is claimed. The handoff lists remaining hardware and runtime work.
- Earlier entries are preserved in [the changelog archive](docs/archive/changelog-history.md).
