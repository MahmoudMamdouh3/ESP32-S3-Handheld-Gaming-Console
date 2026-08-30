# Changelog
All notable changes to the BMO-Handheld-Gaming-Console project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased] - 2026-08-30 (Ruleset v4 & AI Environment Upgrade)
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
