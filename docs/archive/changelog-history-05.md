# Archived changelog (history archived 2026-11-07)

## [Phase E: AI Development Environment & Harness v8 Overhaul] - 2026-10-21
### Added
- **Cross-Document & Rule Truth Verifier (`tools/guardian/core/doc_verifier.py`, `scripts/validate_repo.py` Phase 7)**:
  - Automatically verifies that `README.md`, `AGENTS.md`, `docs/`, and `.agents/rules/` match live `config.h` pin macros, valid file paths, and all 14 emulator wrappers.
- **Self-Contained Task Context Packet Generator (`python -m tools.guardian prompt-pack "<task>"`)**:
  - Generates a compact (< 3,000 token) context packet for any AI model containing matched task intent, `config.h` pins, live C/C++ public header signatures, guardrails, and verification steps.
- **Pre-Commit Diff Auditor (`python -m tools.guardian diff-audit`)**:
  - Audits uncommitted git changes for AST critical violations, documentation/rule drift, and changelog updates.
- **Commit-Msg Git Hook (`scripts/install_git_hooks.py`, `.git/hooks/commit-msg`)**:
  - Enforces structured two-part commit messages (`-m "type(scope): summary" -m "- bullet points"`) per `.agents/rules/05_git_workflow.md` and rejects banned AI buzzwords per `.agents/rules/43_no_ai_jargon_and_buzzwords.md`.
- **Anti-Buzzword & Marketing Filler Ban (`.agents/rules/43_no_ai_jargon_and_buzzwords.md`, `tools/guardian/core/doc_verifier.py`)**:
  - Added Rule 43 banning AI buzzwords, brand vanity labels, and marketing filler across code comments, documentation, rules, and commit messages.
  - Added `DocTruthVerifier.verify_no_buzzwords()` and stripped marketing jargon across `firmware/BmoGameboy/src/core/`, `docs/`, `.agents/rules/`, `scripts/`, `tests/`, `tools/`, and `CHANGELOG.md`; deleted obsolete fluff files `docs/research_apple_nintendo_ui_ux.md` and `docs/project-rules.md`.
- **WebAssembly Playtest Cores in Simulator (`scripts/build_wasm_cores.py`, `tools/bmo_simulator/bmo_playtest_cores.wasm`, `tools/bmo_simulator/wasm_playtest_engine.js`, `tools/bmo_simulator/simulator.js`)**:
  - Added `scripts/build_wasm_cores.py` generating `bmo_playtest_cores.wasm` (16-page Wasm linear memory, `wasm_core_init`, `wasm_core_load_rom`, `wasm_core_step_frame`, `wasm_core_get_rom_ptr`, `wasm_core_get_fb_ptr`, `wasm_core_get_checksum`, `wasm_core_get_frame_count`).
  - Integrated `WasmPlaytestEngine` into `tools/bmo_simulator/` supporting live 60 Hz RGB565 frame execution across all 15 console viewports, `.p8` cart section parsing, 1:1 console bezels, A/B button swap, and pause menu save/load state slots.
- **Portable AI Harness Exporter & Pre-Flight Patch Checker (`tools/guardian/cli.py`)**:
  - Added `python -m tools.guardian patch-check` for pre-flight checking target files or uncommitted changes for brace balance, AST guardrails, and Rule 43 buzzwords.
  - Added `python -m tools.guardian export-harness <dest>` exporting the 14 core governance rules, `tools/guardian/`, and git hooks into any external project.
  - Added `/api/host-cores/run` HTTP endpoint in `tools/bmo_simulator/run_simulator.py` and connected one-click host core verification in the simulator UI.

### Fixed & Removed
- **Guardian Core Fixes (`elf_analyzer.py`, `ast_linter.py`, `host_bench.py`, `generate_ai_knowledge_base.py`)**:
  - Fixed `ElfAnalyzer.find_latest_elf()` to discover `BmoGameboy.ino.elf` under `%LOCALAPPDATA%/arduino/sketches` and exclude `.dummy` linker sections (`DRAM: 218,816 B`, `IRAM: 77,824 B`, `Flash: 5,692,724 B`).
  - Fixed `FirmwareAstLinter` `EXCLUDED_TOP_DIRS` matching so `firmware/BmoGameboy/src/assets/roms/` is indexed while skipping slow regex scans over 27.7 MB of C hex byte lines; added `DIRECT_DIGITAL_READ_IN_HOT_PATH` and `UNARBITRATED_SD_IO_IN_CORE` rules.
  - Updated `HostBenchmarkSuite` GPIO bitmask shifts to match `config.h` (`4, 5, 6, 7, 16, 17, 18, 21`) and integrated native x64 10-core 500-frame C benchmark execution (`Native_C_10_Cores_500_Frames`).
  - Removed duplicate `convert.py`, `tools/convert.py`, `IMPLEMENTATION_PLAN.md`, `report_guardian.md`, `test_compile/`, and partial `tools/zig/` directory; refactored `scripts/convert_roms.py`, `scripts/color_calc.py`, and `scripts/test_runner.py`.

---
