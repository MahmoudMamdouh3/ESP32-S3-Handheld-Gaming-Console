# Archived changelog (history archived 2026-11-07)

## [Milestone 4.5] - 2026-08-30 (Ruleset v3 & SDD Upgrade)

### Added
- **Software Design Document (SDD) v2.0**: Completely rewritten [`docs/software-design-document.md`](../software-design-document.md) to serve as a technical specification. Documents the complete rendering pipeline, procedural SDF mascot engine, emulator contracts, memory maps, SPI bus sharing, and multi-agent AI environment.
- **Agent Quick-Start Primer**: Added [`.agents/rules/31_quick_start_primer.md`](../../.agents/rules/31_quick_start_primer.md) providing a 90-second zero-context on-ramp and decision matrix for AI agents and human contributors.
- **Symbol Reference Expansion**: Extended [`.agents/rules/10_symbol_reference.md`](../../.agents/rules/10_symbol_reference.md) to include verified public APIs for `DisplayEmu`, `Buttons`, `SDCard`, `BmoFace`, `Battery`, `PeanutEmu`, `WalnutEmu`, `NesEmu`, and `DoomEmu`.
- **Anti-Pattern Registry M-16**: Added M-16 entry in [`.agents/rules/30_common_agent_mistakes.md`](../../.agents/rules/30_common_agent_mistakes.md).
- **Changelog**: Added this repository-level [`CHANGELOG.md`](../../CHANGELOG.md).

### Fixed
- **PSRAM Cartridge RAM Teardown**: Fixed return-to-menu SELECT+UP exit path in [`firmware/BmoGameboy/BmoGameboy.ino`](../../firmware/BmoGameboy/BmoGameboy.ino) by wiring `WalnutEmu::destroy()` and `PeanutEmu::destroy()`, recovering 128KB PSRAM on every session exit.
- **Directory Structure Sync**: Synchronized directory tree in [`.agents/rules/03_conventions.md`](../../.agents/rules/03_conventions.md) and [`.agents/rules/27_codebase_map.md`](../../.agents/rules/27_codebase_map.md) with `src/engine/` and `scripts/`.
- **Tooling & Test Paths**: Synchronized `scripts/` and `tests/` paths with the unified `firmware/BmoGameboy/` architecture; validated 100% pass rate across python unit tests, benchmarks, and repo checks.
- **Ruleset Version**: Bumped ruleset to Version 3 in [`.agents/rules/README.md`](../../.agents/rules/README.md).

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
