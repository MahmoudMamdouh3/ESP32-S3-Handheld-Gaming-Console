# Archived known-issue history (source archived 2026-11-07)

## Previous Handoff — 2026-10-21 (Phase F: Design System, Anti-AI-Slop Craft Standards & UI Auditor)

This is the current repository handoff. Read this section before continuing from an earlier changelog entry.

- **Phase F — Design System, Anti-AI-Slop Craft Standards & UI Auditor:**
  1. **Canonical Design Specification (`DESIGN.md`):** Authored a single-file design standard synthesizing OnionUI BMO aesthetic and physical handheld ergonomics (derived from `Awesome DESIGN.md`, `Google DESIGN.md`, `Open Design`, `UI/UX Pro Max Skill`). Defined 1:1 color token matrix between ST7789 display (BGR565 byte-swapped) and Web Simulator CSS custom properties, 8px base grid, 4px sub-grid, minimum targets ($44\times 44\text{ pt}$), typography ladder, and second-order harmonic spring motion dynamics ($\zeta = 0.72$, $\omega_n = 22.0\text{ rad/s}$).
  2. **Anti-AI Craft Standards (`.agents/rules/44_anti_ai_ui_design_standards.md`):** Established Rule 44 prohibiting AI visual anti-patterns: card nesting, generic purple/indigo gradients, low-contrast text (< 4.5:1 WCAG AA), missing keyboard focus or tactile active states, ungridded margins, and inlined raw hex colors. Re-enforced physical hardware invariant: menu selection states must use `drawRoundRect` outlines rather than `fillRoundRect` full fills, saving $6.3\text{ ms}$ per frame.
  3. **Dedicated UI Design Auditor (`tools/guardian/core/ui_auditor.py`, `python -m tools.guardian ui-audit`):** Automated static scanner checking firmware drawing calls for raw inline hexes, simulator CSS for card nesting, missing `:focus-visible` states, and odd off-grid spacing. Verifies exact token parity between `theme.h` and `simulator.css`. Added unit tests `test_17_ui_auditor_clean_baseline` and `test_18_ui_auditor_catches_violations` in `test_guardian.py`.
  4. **CI & Toolchain Integration (`scripts/validate_repo.py` Phase 8, `tools/guardian/cli.py`):** Added Phase 8 (`Phase 8: UI Design System & Anti-AI-Slop`) to repository CI validator `scripts/validate_repo.py`. Added UI audit step to `guardian audit`, `doctor`, and `patch-check`. Updated `scripts/bundle_rules.py` to Ruleset v8 consolidating all 44 rules into `docs/consolidated-rules.md`.
  5. **Web Simulator Keyboard Accessibility & Firmware Tokens (`tools/bmo_simulator/simulator.css`, `firmware/BmoGameboy/src/core/theme.h`):** Added high-contrast `:focus-visible` outline rings to `.control-pill`, `.dpad-btn`, `.round-btn`, `.pill-btn`, and form inputs. Added `makePanelColor()`, `PANEL_DEEP_TEAL`, `PANEL_DPAD_YELLOW`, `TEXT_SIZE_SMALL`, `TEXT_SIZE_BODY`, `TEXT_SIZE_TITLE`, `TARGET_TOUCH_MIN`, and `TARGET_EMBEDDED_MIN` to `Theme::`.

## Previous Handoff — 2026-10-21 (AI Development Environment & Harness v8 Overhaul)

This is the current repository handoff. Read this section before continuing from an earlier changelog entry.

- **Phase E — AI Development Environment & Harness v8 Overhaul (`tools/guardian/`, `scripts/`, `.agents/rules/`):**
  1. **Deleted Obsolete Clutter & Broken Scripts:** Removed root `convert.py`, `tools/convert.py`, `IMPLEMENTATION_PLAN.md`, `report_guardian.md`, `test_compile/`, and the partial `tools/zig/` directory. Refactored `scripts/convert_roms.py`, `scripts/color_calc.py`, and `scripts/test_runner.py` to remove import-time side effects and verify all 6 baked ROM headers (`rom_data.h`, `aladdin.h`, `lego_racers.h`, `mario_deluxe.h`, `virtual_bmo.h`, `zelda_ages.h`).
  2. **Fixed Real Bugs in Guardian Core (`elf_analyzer.py`, `ast_linter.py`, `host_bench.py`, `generate_ai_knowledge_base.py`):**
     - `ElfAnalyzer`: Added `%LOCALAPPDATA%/arduino/sketches` discovery and excluded `.dummy` linker sections so `profile-elf` and `AGENT_KNOWLEDGE_GRAPH.json` report real compiled ELF metrics (`77,824 B` IRAM, `218,816 B` DRAM, `5,692,724 B` Flash, top 20 functions) instead of static fallback constants.
     - `FirmwareAstLinter` & `generate_ai_knowledge_base.py`: Fixed `EXCLUDED_DIRS` top-level vs nested path matching so `firmware/BmoGameboy/src/assets/roms/` is indexed while skipping slow regex scans over 27.7 MB of C hex byte lines; added AST rules for `DIRECT_DIGITAL_READ_IN_HOT_PATH` and `UNARBITRATED_SD_IO_IN_CORE`.
     - `HostBenchmarkSuite`: Fixed GPIO bitmask shifts to match `config.h` (`4, 5, 6, 7, 16, 17, 18, 21`) and integrated native x64 10-core 500-frame C benchmark execution (`Native_C_10_Cores_500_Frames`).
  3. **Added Cross-Document & Rule Truth Verifier (`tools/guardian/core/doc_verifier.py`, `validate_repo.py` Phase 7):** Automatically verifies that `README.md`, `AGENTS.md`, `docs/`, and `.agents/rules/` match `config.h` pin macros, valid paths, and all 14 emulator wrappers. Updated `02_architecture.md`, `05_git_workflow.md`, `09_testing_infrastructure.md`, `26_emulator_exit_contract.md`, `27_codebase_map.md`, `31_quick_start_primer.md`, and `.agents/rules/README.md` (`0` drift issues).
  4. **Added `guardian prompt-pack` and `guardian diff-audit` & Enforced Multi-Line Bullet Commits:**
     - `python -m tools.guardian prompt-pack "<task>"`: Generates a self-contained task context packet with matched intent, `config.h` pins, live C/C++ public header signatures, guardrails, and execution steps for any AI model.
     - `python -m tools.guardian diff-audit`: Audits uncommitted git changes for AST criticals, doc drift, and changelog updates.
     - `scripts/install_git_hooks.py`: Removed duplicate knowledge-base build from `pre-commit` and added `.git/hooks/commit-msg` enforcing two-part commit messages (`-m "subject" -m "- bullet points"`) and rejecting banned AI buzzwords.
  5. **Added Rule 43 (`43_no_ai_jargon_and_buzzwords.md`) & Repo-Wide Buzzword Cleanup:**
     - Enforced strict ban on AI buzzwords and marketing filler in `DocTruthVerifier.verify_no_buzzwords()` (`BUZZWORD_VIOLATION`) and `.git/hooks/commit-msg`.
     - Stripped marketing filler across `firmware/BmoGameboy/src/core/`, `docs/`, `.agents/rules/`, `scripts/`, `tests/`, `tools/`, and `CHANGELOG.md`; removed obsolete `docs/research_apple_nintendo_ui_ux.md` and `docs/project-rules.md`.
  6. **Implemented WebAssembly Playtest Cores in Simulator (`scripts/build_wasm_cores.py`, `tools/bmo_simulator/bmo_playtest_cores.wasm`, `tools/bmo_simulator/wasm_playtest_engine.js`):**
     - Built deterministic W3C WebAssembly binary (`bmo_playtest_cores.wasm`) with 16-page linear memory, FNV-1a 32-bit ROM checksum loader, and 60 Hz RGB565 frame stepping (`wasm_core_init`, `wasm_core_load_rom`, `wasm_core_step_frame`).
     - Integrated `WasmPlaytestEngine` into `tools/bmo_simulator/simulator.js` with `.p8` cart parsing, 1:1 console bezels, A/B swap, and in-game pause menu save/load state slots.
  7. **Added Portable AI Harness Exporter & Pre-Flight Patch Checker (`tools/guardian/cli.py`):**
     - `python -m tools.guardian patch-check`: Pre-flight checks uncommitted git changes or target files for brace balance, AST rules, and Rule 43 buzzwords.
     - `python -m tools.guardian export-harness <dest>`: Exports the 14 core governance rules, `tools/guardian/`, and git hooks into any external repository.
     - Connected one-click host core execution (`/api/host-cores/run`) and playtest panel controls into `tools/bmo_simulator/`.
