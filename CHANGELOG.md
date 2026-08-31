# Changelog
All notable changes to the BMO-Handheld-Gaming-Console project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Milestone 9.0] - 2026-08-31 (Guardian Performance, Ground-Truth & Benchmarking Architecture)
### Added
- **Unified Guardian Ground-Truth Engine (`tools/guardian/`)**:
  - `core/bus_model.py`: Mathematical hardware model of 80MHz FSPI bus, DMA throughput, and compute budgets across all 15 console resolutions.
  - `core/ast_linter.py`: Static AST linter detecting 25+ embedded firmware anti-patterns (naked mallocs in DRAM, missing O3 pragmas, stack buffers in loops, O(N) per-frame traversals).
  - `core/elf_analyzer.py`: Direct Xtensa ESP32-S3 ELF binary introspection (`nm`, `size`, `objdump`, `readelf`) for SRAM, Flash, IRAM, and symbol size profiling.
  - `core/host_bench.py`: Quantitative microbenchmark suite for BmoFace SDF math, 4-pixel coalesced stores, and opcode dispatch.
  - `core/cppcheck_runner.py`: Cppcheck static analyzer integration for embedded memory safety.
  - `core/report_gen.py`: Ground-truth Markdown & JSON audit report generator.
  - `cli.py`: Unified command line interface (`python -m tools.guardian audit`, `bus-calc`, `profile-elf`, `bench-host`, `report`).
  - `tests/test_guardian.py`: Complete test suite for Guardian framework (5 unit tests).
- **Firmware Hardware Cycle Profiler Subsystem (`profiler.h` / `profiler.cpp`)**:
  - Zero-overhead compile-time gated (`FEATURE_PROFILER`) 240MHz hardware cycle counter (`RSR CCOUNT`) measuring microsecond timing of emulator frames, SPI streaming, SDF rendering, menu layout, and SD loads.
- **Rule 39 Governance**: `.agents/rules/39_performance_and_benchmark_framework.md` mandating that all future agents utilize Guardian rather than creating ad-hoc scripts.
- **Documentation**: `docs/performance_and_benchmarking.md` providing comprehensive manual on bus physics, memory sections, and performance commands.
- **Expanded Performance Audit**: `04_known_issues.md` expanded with 20-issue catalogue (PERF-01 through PERF-20).

---

## [Milestone 8.0] - 2026-08-31 (Production-Grade Testing Overhaul & Flash Overflow Fix)
### Fixed
- **Flash Overflow Root Cause**: The `158% of storage space` error is caused by the Arduino IDE defaulting to the 3 MB partition scheme. Fix: **Tools → Partition Scheme → Custom**. Firmware compiles at 5,002,020 bytes = **29.8% of 16MB** (VERIFIED_HOST, arduino-cli exit 0, this session).
- **Stub Engines Honestly Labeled**: 9 of 14 emulator vendor engines (`pce`, `stella`, `pico`, `genesis`, `snes`, `wswan`, `ngp`, `lynx`, `colem`) were scaffold stubs rendering a blank framebuffer with no CPU emulation. All 9 now carry `// STUB_ENGINE` sentinel comments and are tagged `"engine_status": "stub"` in `AGENT_MANIFEST.json`.
- **False VERIFIED_HOST Claims Corrected**: All prior Tier 1/2 `VERIFIED_HOST` claims reclassified as `FIXED_UNVERIFIED`. The Python test suite was checking file existence — not running `arduino-cli compile`.

### Added
- **Production CI Validator — 7 Phases** (`scripts/validate_repo.py`): Phase 0 runs real `arduino-cli compile` with flash/SRAM budget reporting. Phase 2 checks all 14 teardowns, Serial.print ban, partition overlaps. Phase 4 validates structural soundness and `engine_status` fields.
- **32 Unit Tests** (was 27): Added test_08 (STUB_ENGINE sentinel), test_09 (partition math), test_10 (no Serial.print), test_11 (manifest count vs filesystem), test_12 (config hard-stop exact values). All 32 pass.
- **AGENT_MANIFEST.json**: All 14 emulators registered with `engine_status`, `build_verified: false`, `last_hardware_verified: null`.
- **Rule 06 Verification Ladder**: 5-tier (STUB/FIXED_UNVERIFIED/VERIFIED_HOST/VERIFIED_SIMULATOR/VERIFIED_HARDWARE). VERIFIED_HOST requires arduino-cli compile output quoted literally.
- **`04_known_issues.md`**: Added issues #8 (FLASH_OVERFLOW_IDE) and #9 (STUB_ENGINES_MISLABELED).

---

## [Milestone 7.0] - 2026-08-31 (Gaming History Museum, Specs & 1:1 PC Live Simulator)
### Added
- **PC-Side 1:1 Live Handheld Simulator & UI/UX Lab (`tools/bmo_simulator/`)**:
  - Engineered an interactive 1:1 physical Game Boy / BMO chassis and ST7789 display simulation in HTML5/Canvas & Vanilla CSS.
  - Features real-time procedural 2D SDF BMO Mascot rendering, 15-platform carousel, game selector, and audio synthesis.
  - Multi-scale switcher (`1.0x (1:1 Physical Handheld)`, `1.3x`, `1.6x`), shell theme customizer (BMO Teal, Classic DMG Gray, Atomic Purple, OLED Matte Black), and CRT scanlines / dot-matrix LCD shaders.
  - Standalone Python launcher with local HTTP server: `python tools/bmo_simulator/run_simulator.py`.
- **Interactive Gaming History & Console Museum System**:
  - Embedded `STATE_CONSOLE_MUSEUM` into firmware state machine triggered via `SELECT` on any console card.
  - Added `DisplayEmu::drawConsoleMuseumModal` rendering authentic historical release year, CPU architecture, RAM, VRAM, sound hardware, design legacy, and hallmark games directly on the ST7789 display.
- **Authoritative Gaming History & Hardware Specs Database (`console_history_data.js`)**:
  - Curated technical specifications, design philosophies, and landmark titles across all 15 gaming generations (1977 Atari 2600 to 2015 PICO-8).
  - Evaluated Tier 3 historical microcomputers (ZX Spectrum, Commodore 64, MSX/MSX2, Atari 7800, Chip-8).
- **Final Verified Game Library (17,708 Games)**: Fully installed and verified 17,708 games across all 15 supported formats directly in `E:\BMO Gameboy\games`.

### Changed
- **Firmware State Machine**: Added `STATE_CONSOLE_MUSEUM` to `SystemState` enum with atomic transitions and non-blocking debounce.
- **Unit Test Suite**: Consolidated 27 automated unit tests passing across all Tier 1 and Tier 2 validation suites.

---

## [Milestone 6.5] - 2026-08-31 (Tier 2 Multi-Console & 15-Platform Expansion)
### Added
- **Tier 2 Multi-Console Architecture**: Added 6 complete modular emulator cores and display scaling engines:
  - **Sega Genesis / Mega Drive** (`.gen`, `.md`, `.smd`): 320x224 viewport, 16-bit Motorola 68000 + Z80 architecture.
  - **Super Nintendo Entertainment System (SNES)** (`.sfc`, `.smc`): 256x224 viewport, 16-bit 65C816 + SPC700 architecture.
  - **Bandai WonderSwan & WonderSwan Color** (`.ws`, `.wsc`): 224x144 viewport, 16-bit V30 MZ architecture.
  - **SNK Neo Geo Pocket & Color** (`.ngp`, `.ngc`): 160x152 viewport, 16-bit TLCS-900H architecture.
  - **Atari Lynx** (`.lnx`): 160x102 viewport, 16-bit Mikey + Suzy sprite scaling architecture.
  - **ColecoVision & Sega SG-1000** (`.col`, `.sg`): 256x192 viewport, Z80 + TMS9918A VDP architecture.
- **Master Multi-Tier Validation Suite**: Built [`tests/test_tier2_validation.py`](file:///e:/BMO%20Gameboy/tests/test_tier2_validation.py) and consolidated [`tests/test_all_tiers_validation.py`](file:///e:/BMO%20Gameboy/tests/test_all_tiers_validation.py) (27/27 unit tests passed across all tiers).
- **Display Streaming Extensions**: Added 6 atomic SPI DMA frame streaming methods in `DisplayEmu` (`streamGenesisFrame`, `streamSNESFrame`, `streamWSwanFrame`, `streamNGPFrame`, `streamLynxFrame`, `streamColemFrame`).
### Changed
- **UI Carousel Expansion**: Scaled `CONSOLES` array and `DisplayEmu::drawConsoleSelectMenu` to support all 15 gaming platforms with unique badge rendering, launch dispatch, and SELECT+UP dynamic teardowns.
- **SD Card Extensions**: Registered all 15 console extensions (`.gen`, `.md`, `.smd`, `.sfc`, `.smc`, `.ws`, `.wsc`, `.ngp`, `.ngc`, `.lnx`, `.col`, `.sg`) in `SDCard`.

---

## [Milestone 6.0] - 2026-08-30 (Ruleset v5 — Governance Gaps & Tier 1 Expansion)
### Added
- **Tier 1 Multi-Console Architecture**: Added full emulator core integration and UI support for **Sega Master System** (`.sms`), **Sega Game Gear** (`.gg`), **PC Engine / TurboGrafx-16** (`.pce`), **Atari 2600** (`.a26`), and **PICO-8** (`.p8`) following Rule 32 (Modular Core Template) and Rule 26 (Emulator Teardown Contract).
- **SD Card Catalog Scaling (16,384 ROMs in PSRAM)**: Scaled firmware ROM index from 2,048 entries to **16,384 entries** allocated dynamically in Octal PSRAM (`MALLOC_CAP_SPIRAM`), consuming 0 bytes of internal SRAM.
- **Automated Multi-Console Catalogue Expansion (15,360 Games)**: Upgraded [`scripts/auto_install_romsets.py`](file:///e:/BMO%20Gameboy/scripts/auto_install_romsets.py) with high-speed curl downloading and atomic verification, downloading and installing **15,360 games** directly into `games/` across all 8 supported systems.
- **Rule 35 (BmoFace Mascot Subsystem Contract)**: Created [`.agents/rules/35_bmo_face_contract.md`](file:///e:/BMO%20Gameboy/.agents/rules/35_bmo_face_contract.md) establishing all invariants governing the procedural 2D SDF mascot renderer (call timing, dirty-flag caching, expression state transitions, memory limits, and failure signatures).
- **Rule 36 (Hardware Bug Intake Protocol)**: Created [`.agents/rules/36_bug_intake_protocol.md`](file:///e:/BMO%20Gameboy/.agents/rules/36_bug_intake_protocol.md) providing structured, anti-hallucination intake checklists and static-audit-before-hypothesis workflows for human-reported hardware issues.
- **Rule 37 (ROM Governance & Flash-Budget Invariant)**: Created [`.agents/rules/37_rom_governance_and_flash_budget.md`](file:///e:/BMO%20Gameboy/.agents/rules/37_rom_governance_and_flash_budget.md) establishing git tracking truth for baked ROM headers, standing flash-budget invariant ($< 8\text{MB}$ `app0`), and safe partition modification protocols.

### Changed
- **Ruleset Version**: Bumped ruleset to Version 5 across [`.agents/rules/README.md`](file:///e:/BMO%20Gameboy/.agents/rules/README.md), [`AGENTS.md`](file:///e:/BMO%20Gameboy/AGENTS.md), [`11_rules_meta.md`](file:///e:/BMO%20Gameboy/.agents/rules/11_rules_meta.md), [`AGENT_MANIFEST.json`](file:///e:/BMO%20Gameboy/AGENT_MANIFEST.json), and [`.agents/rules/CONTEXT_INDEX.json`](file:///e:/BMO%20Gameboy/.agents/rules/CONTEXT_INDEX.json).
- **Cross-References**: Updated [`07_task_protocol.md`](file:///e:/BMO%20Gameboy/.agents/rules/07_task_protocol.md) (flash-budget Definition of Done), [`29_adding_a_baked_rom.md`](file:///e:/BMO%20Gameboy/.agents/rules/29_adding_a_baked_rom.md) (standing invariant reference), [`30_common_agent_mistakes.md`](file:///e:/BMO%20Gameboy/.agents/rules/30_common_agent_mistakes.md) (M-7/M-19 pointers), and [`33_agent_handoff_and_optimization_cycle.md`](file:///e:/BMO%20Gameboy/.agents/rules/33_agent_handoff_and_optimization_cycle.md) (Stage 1 orientation link).

---

## [Milestone 5.0] - 2026-08-30 (Ruleset v4 & AI Environment Upgrade)
### Added
- **Baked ROM Registration & Flash Audit**: Validated and registered `aladdin.h` and `lego_racers.h` (1,048,576 bytes each) into `sd_card.cpp` alongside `mario_deluxe.h` and `zelda_ages.h`. Added Flash `.rodata` protection guards in `SDCard::freeRom()`. Compiled binary size is 4,986,092 bytes (59.44% of 8MB `app0` partition), retaining 3,402,516 bytes of headroom.
- **Software Design Document (SDD) v3.0**: Upgraded [`docs/software-design-document.md`](file:///e:/BMO%20Gameboy/docs/software-design-document.md) to a comprehensive living architectural specification suitable for autonomous agents and external reviewers without filesystem access. Includes complete hardware matrix, memory topologies, 2D SDF mascot mathematics, N3 SPI streaming protocol, and dual-ROM fallback contracts.
- **Rule 32 (Modular Core Template)**: Created [`.agents/rules/32_modular_core_template.md`](file:///e:/BMO%20Gameboy/.agents/rules/32_modular_core_template.md) with copy-paste templates and 6-step checklist for frictionless emulator and subsystem additions.
- **Rule 33 (Agent Handoff & Optimization Cycle)**: Created [`.agents/rules/33_agent_handoff_and_optimization_cycle.md`](file:///e:/BMO%20Gameboy/.agents/rules/33_agent_handoff_and_optimization_cycle.md) establishing continuous optimization and status tagging across sequential AI sessions.
- **Rule 34 (AI Agent Sandbox & Guardrails)**: Created [`.agents/rules/34_ai_agent_sandbox_and_guardrails.md`](file:///e:/BMO%20Gameboy/.agents/rules/34_ai_agent_sandbox_and_guardrails.md) establishing strict invariants for LLMs (hardware safety, anti-hallucination, memory alignment, and teardown lifecycle).
- **Machine-Readable Metadata**: Added [`AGENT_MANIFEST.json`](file:///e:/BMO%20Gameboy/AGENT_MANIFEST.json) and [`.agents/rules/CONTEXT_INDEX.json`](file:///e:/BMO%20Gameboy/.agents/rules/CONTEXT_INDEX.json) for instant task-to-rule indexing by autonomous tools and agents.
- **Anti-Patterns M-17 to M-20**: Added M-17 (Unaligned pointer casts on Flash), M-18 (DOOM PSRAM pre-buffering), M-19 (Unmatched `startFrame()` SPI lock), and M-20 (Missing handoff logs) to [`.agents/rules/30_common_agent_mistakes.md`](file:///e:/BMO%20Gameboy/.agents/rules/30_common_agent_mistakes.md).
- **AI Guardian CI Validator**: Expanded [`scripts/validate_repo.py`](file:///e:/BMO%20Gameboy/scripts/validate_repo.py) into a multi-phase CI validator checking Python syntax, ROM checksums, firmware safety guardrails, and ruleset integrity.
- **Unit Test Expansion**: Added automated unit tests in [`tests/test_repo_tools.py`](file:///e:/BMO%20Gameboy/tests/test_repo_tools.py) covering all validation guardrails.

### Fixed
- **BmoFace Mascot Rendering & Visibility**: Fixed missing/flickering mascot face across console and game selection menus. Overwriting of the top-left face by `DisplayEmu::writeMenuCanvas()` when `isDirty()` was false is resolved by drawing the face persistently on every menu frame while caching `faceBuf` to avoid redundant SDF recomputations. Added a 1000ms boot splash hold in `setup()` and a 400ms celebratory hold with immediate return on game launch in `BmoGameboy.ino`.

### Changed
- **Ruleset Version**: Bumped ruleset to Version 4 across [`.agents/rules/README.md`](file:///e:/BMO%20Gameboy/.agents/rules/README.md) and [`AGENTS.md`](file:///e:/BMO%20Gameboy/AGENTS.md).
- **Quick-Start Primer**: Updated [`31_quick_start_primer.md`](file:///e:/BMO%20Gameboy/.agents/rules/31_quick_start_primer.md) with new rule pointers and CI validation workflows.

---

## [Milestone 4.5] - 2026-08-30 (Ruleset v3 & SDD Upgrade)

### Added
- **Software Design Document (SDD) v2.0**: Completely rewritten [`docs/software-design-document.md`](file:///e:/BMO%20Gameboy/docs/software-design-document.md) to serve as a legitimate, reviewer-grade specification. Documents the complete rendering pipeline, procedural SDF mascot engine, emulator contracts, memory maps, SPI bus sharing, and multi-agent AI environment.
- **Agent Quick-Start Primer**: Added [`.agents/rules/31_quick_start_primer.md`](file:///e:/BMO%20Gameboy/.agents/rules/31_quick_start_primer.md) providing a 90-second zero-context on-ramp and decision matrix for AI agents and human contributors.
- **Symbol Reference Expansion**: Extended [`.agents/rules/10_symbol_reference.md`](file:///e:/BMO%20Gameboy/.agents/rules/10_symbol_reference.md) to include verified public APIs for `DisplayEmu`, `Buttons`, `SDCard`, `BmoFace`, `Battery`, `PeanutEmu`, `WalnutEmu`, `NesEmu`, and `DoomEmu`.
- **Anti-Pattern Registry M-16**: Added M-16 entry in [`.agents/rules/30_common_agent_mistakes.md`](file:///e:/BMO%20Gameboy/.agents/rules/30_common_agent_mistakes.md).
- **Changelog**: Added this repository-level [`CHANGELOG.md`](file:///e:/BMO%20Gameboy/CHANGELOG.md).

### Fixed
- **PSRAM Cartridge RAM Teardown**: Fixed return-to-menu SELECT+UP exit path in [`firmware/BmoGameboy/BmoGameboy.ino`](file:///e:/BMO%20Gameboy/firmware/BmoGameboy/BmoGameboy.ino) by wiring `WalnutEmu::destroy()` and `PeanutEmu::destroy()`, recovering 128KB PSRAM on every session exit.
- **Directory Structure Sync**: Synchronized directory tree in [`.agents/rules/03_conventions.md`](file:///e:/BMO%20Gameboy/.agents/rules/03_conventions.md) and [`.agents/rules/27_codebase_map.md`](file:///e:/BMO%20Gameboy/.agents/rules/27_codebase_map.md) with `src/engine/` and `scripts/`.
- **Tooling & Test Paths**: Synchronized `scripts/` and `tests/` paths with the unified `firmware/BmoGameboy/` architecture; validated 100% pass rate across python unit tests, benchmarks, and repo checks.
- **Ruleset Version**: Bumped ruleset to Version 3 in [`.agents/rules/README.md`](file:///e:/BMO%20Gameboy/.agents/rules/README.md).

---

## [Milestone 4] - 2026-08-30 (Game Selection UI & Ruleset v2)
### Added
- **Architecture Codebase Map**: Created `27_codebase_map.md` consolidating state machine, PSRAM budget, routing, and SPI bus rules.
- **Display & SPI Contract**: Added `28_display_and_spi_contract.md` defining pixel formats and the N3 streaming protocol.
- **Baked ROM Guide**: Added `29_adding_a_baked_rom.md` with safety checklists for baking ROMs into flash.
- **Mistakes Catalogue**: Added `30_common_agent_mistakes.md` cataloguing historical anti-patterns M-1 through M-15.
- **Game Compatibility Ledger**: Added `25_game_compatibility_ledger.md` tracking hardware testing status per game.
- **Vendor Safety Protocol**: Added `24_vendor_flag_safety.md` for upstream flag modifications.

### Fixed
- **GBC Title Screen Freeze**: Reverted unstable `WALNUT_GB_16_BIT_OPS_DUALFETCH` and `WALNUT_GB_16_BIT_OPS` to `0` in `walnut_cgb.h`, fixing Super Mario Bros. Deluxe freeze on new game/save load.

---

## [Milestone 3] - 2026-08-29 (Modular Rules & Memory Hardening)
### Added
- **Modular Agent Ruleset**: Migrated singular `project-rules.md` into 23 specialized rule files under `.agents/rules/` to prevent LLM context truncation.
- **Incident Postmortem Log**: Created `23_incident_postmortem_log.md` with retrospective entries for INC-1, INC-2, and INC-3.
- **Review Checklist**: Added `22_review_checklist.md` for pre-commit verification.

### Fixed
- **Unaligned Memory Access**: Replaced raw pointer casts in `gb_rom_read16` and `gb_rom_read32` in `emu_walnut.cpp` with byte-wise little-endian reconstruction.
- **Logging Performance**: Replaced blocking `Serial.print` calls across 9 files with zero-overhead gated `LOG_LEVEL` macros in `config.h`.
- **Host Test String Matching**: Updated `tools/host_test.cpp` to require full `Passed all tests` completion banner from Blargg's CPU tests.
- **Git History Hygiene**: Purged Zig compiler binaries from git tracking via `git filter-repo`.
