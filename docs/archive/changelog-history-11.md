# Archived changelog (history archived 2026-11-07)

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
- **Master Multi-Tier Validation Suite**: Built [`tests/test_tier2_validation.py`](../../tests/test_tier2_validation.py) and consolidated [`tests/test_all_tiers_validation.py`](../../tests/test_all_tiers_validation.py) (27/27 unit tests passed across all tiers).
- **Display Streaming Extensions**: Added 6 atomic SPI DMA frame streaming methods in `DisplayEmu` (`streamGenesisFrame`, `streamSNESFrame`, `streamWSwanFrame`, `streamNGPFrame`, `streamLynxFrame`, `streamColemFrame`).
### Changed
- **UI Carousel Expansion**: Scaled `CONSOLES` array and `DisplayEmu::drawConsoleSelectMenu` to support all 15 gaming platforms with unique badge rendering, launch dispatch, and SELECT+UP dynamic teardowns.
- **SD Card Extensions**: Registered all 15 console extensions (`.gen`, `.md`, `.smd`, `.sfc`, `.smc`, `.ws`, `.wsc`, `.ngp`, `.ngc`, `.lnx`, `.col`, `.sg`) in `SDCard`.

---

## [Milestone 6.0] - 2026-08-30 (Ruleset v5 — Governance Gaps & Tier 1 Expansion)
### Added
- **Tier 1 Multi-Console Architecture**: Added full emulator core integration and UI support for **Sega Master System** (`.sms`), **Sega Game Gear** (`.gg`), **PC Engine / TurboGrafx-16** (`.pce`), **Atari 2600** (`.a26`), and **PICO-8** (`.p8`) following Rule 32 (Modular Core Template) and Rule 26 (Emulator Teardown Contract).
- **SD Card Catalog Scaling (16,384 ROMs in PSRAM)**: Scaled firmware ROM index from 2,048 entries to **16,384 entries** allocated dynamically in Octal PSRAM (`MALLOC_CAP_SPIRAM`), consuming 0 bytes of internal SRAM.
- **Automated Multi-Console Catalogue Expansion (15,360 Games)**: Upgraded [`scripts/auto_install_romsets.py`](../../scripts/auto_install_romsets.py) with high-speed curl downloading and atomic verification, downloading and installing **15,360 games** directly into `games/` across all 8 supported systems.
- **Rule 35 (BmoFace Mascot Subsystem Contract)**: Created [`.agents/rules/35_bmo_face_contract.md`](../../.agents/rules/35_bmo_face_contract.md) establishing all invariants governing the procedural 2D SDF mascot renderer (call timing, dirty-flag caching, expression state transitions, memory limits, and failure signatures).
- **Rule 36 (Hardware Bug Intake Protocol)**: Created [`.agents/rules/36_bug_intake_protocol.md`](../../.agents/rules/36_bug_intake_protocol.md) providing structured, anti-hallucination intake checklists and static-audit-before-hypothesis workflows for human-reported hardware issues.
- **Rule 37 (ROM Governance & Flash-Budget Invariant)**: Created [`.agents/rules/37_rom_governance_and_flash_budget.md`](../../.agents/rules/37_rom_governance_and_flash_budget.md) establishing git tracking truth for baked ROM headers, standing flash-budget invariant ($< 8\text{MB}$ `app0`), and safe partition modification protocols.

### Changed
- **Ruleset Version**: Bumped ruleset to Version 5 across [`.agents/rules/README.md`](../../.agents/rules/README.md), [`AGENTS.md`](../../AGENTS.md), [`11_rules_meta.md`](../../.agents/rules/11_rules_meta.md), [`AGENT_MANIFEST.json`](../../AGENT_MANIFEST.json), and [`.agents/rules/CONTEXT_INDEX.json`](../../.agents/rules/CONTEXT_INDEX.json).
- **Cross-References**: Updated [`07_task_protocol.md`](../../.agents/rules/07_task_protocol.md) (flash-budget Definition of Done), [`29_adding_a_baked_rom.md`](../../.agents/rules/29_adding_a_baked_rom.md) (standing invariant reference), [`30_common_agent_mistakes.md`](../../.agents/rules/30_common_agent_mistakes.md) (M-7/M-19 pointers), and [`33_agent_handoff_and_optimization_cycle.md`](../../.agents/rules/33_agent_handoff_and_optimization_cycle.md) (Stage 1 orientation link).

---

## [Milestone 5.0] - 2026-08-30 (Ruleset v4 & AI Environment Upgrade)
### Added
- **Baked ROM Registration & Flash Audit**: Validated and registered `aladdin.h` and `lego_racers.h` (1,048,576 bytes each) into `sd_card.cpp` alongside `mario_deluxe.h` and `zelda_ages.h`. Added Flash `.rodata` protection guards in `SDCard::freeRom()`. Compiled binary size is 4,986,092 bytes (59.44% of 8MB `app0` partition), retaining 3,402,516 bytes of headroom.
- **Software Design Document (SDD) v3.0**: Upgraded [`docs/software-design-document.md`](../software-design-document.md) to a comprehensive living architectural specification suitable for autonomous agents and external reviewers without filesystem access. Includes complete hardware matrix, memory topologies, 2D SDF mascot mathematics, N3 SPI streaming protocol, and dual-ROM fallback contracts.
- **Rule 32 (Modular Core Template)**: Created [`.agents/rules/32_modular_core_template.md`](../../.agents/rules/32_modular_core_template.md) with copy-paste templates and 6-step checklist for frictionless emulator and subsystem additions.
- **Rule 33 (Agent Handoff & Optimization Cycle)**: Created [`.agents/rules/33_agent_handoff_and_optimization_cycle.md`](../../.agents/rules/33_agent_handoff_and_optimization_cycle.md) establishing continuous optimization and status tagging across sequential AI sessions.
- **Rule 34 (AI Agent Sandbox & Guardrails)**: Created [`.agents/rules/34_ai_agent_sandbox_and_guardrails.md`](../../.agents/rules/34_ai_agent_sandbox_and_guardrails.md) establishing strict invariants for LLMs (hardware safety, anti-hallucination, memory alignment, and teardown lifecycle).
- **Machine-Readable Metadata**: Added [`AGENT_MANIFEST.json`](../../AGENT_MANIFEST.json) and [`.agents/rules/CONTEXT_INDEX.json`](../../.agents/rules/CONTEXT_INDEX.json) for instant task-to-rule indexing by autonomous tools and agents.
- **Anti-Patterns M-17 to M-20**: Added M-17 (Unaligned pointer casts on Flash), M-18 (DOOM PSRAM pre-buffering), M-19 (Unpaired `startFrame()` SPI lock), and M-20 (Missing handoff logs) to [`.agents/rules/30_common_agent_mistakes.md`](../../.agents/rules/30_common_agent_mistakes.md).
- **AI Guardian CI Validator**: Expanded [`scripts/validate_repo.py`](../../scripts/validate_repo.py) into a multi-phase CI validator checking Python syntax, ROM checksums, firmware safety guardrails, and ruleset integrity.
- **Unit Test Expansion**: Added automated unit tests in [`tests/test_repo_tools.py`](../../tests/test_repo_tools.py) covering all validation guardrails.

### Fixed
- **BmoFace Mascot Rendering & Visibility**: Fixed missing/flickering mascot face across console and game selection menus. Overwriting of the top-left face by `DisplayEmu::writeMenuCanvas()` when `isDirty()` was false is resolved by drawing the face persistently on every menu frame while caching `faceBuf` to avoid redundant SDF recomputations. Added a 1000ms boot splash hold in `setup()` and a 400ms celebratory hold with immediate return on game launch in `BmoGameboy.ino`.

### Changed
- **Ruleset Version**: Bumped ruleset to Version 4 across [`.agents/rules/README.md`](../../.agents/rules/README.md) and [`AGENTS.md`](../../AGENTS.md).
- **Quick-Start Primer**: Updated [`31_quick_start_primer.md`](../../.agents/rules/31_quick_start_primer.md) with new rule pointers and CI validation workflows.

---
