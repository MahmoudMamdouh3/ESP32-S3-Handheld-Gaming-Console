# BMO Handheld Console — Complete Line-by-Line Benchmark & Architecture Audit
**Generated:** 2026-10-21 18:40:03  
**Target MCU:** ESP32-S3-N16R8 (240MHz Xtensa LX7, 16MB QIO Flash, 8MB Octal PSRAM)  
**Display:** ST7789VW 2.4" TFT on Shared 80MHz FSPI  
**Total Repository Files Audited:** 376 files (444,323 total lines, 409,438 SLOC)  
**Ground-Truth Quality Gate:** PASS (Zero Critical Violations)

---

## 1. Executive Summary & Hardware Health Dashboard

| Metric | Measured Value | Hardware Budget | Utilization | Health Status |
| :--- | :--- | :--- | :--- | :--- |
| **Internal SRAM (DRAM)** | 218,816 bytes | 327,680 bytes | 66.8% | ✅ OK |
| **Flash Storage (app0)** | 5,692,909 bytes | 16,777,216 bytes | 33.9% | ✅ OK (Headroom 70.2%) |
| **Static PSRAM** | 0 bytes | 8,388,608 bytes | 0.0% | ✅ OK |
| **Dynamic PSRAM Allocation** | 16,384 ROM slots (1.05MB) | 8,388,608 bytes | 12.5% | ✅ OK (7.3MB Free) |
| **AST Critical Findings** | 0 | 0 allowed | - | ✅ PASS (0 Critical) |
| **AST Warnings** | 13 | < 15 allowed | - | ⚠️ REVIEW |
| **Cppcheck Violations** | 0 | 0 critical | - | ✅ CLEAN |

---

## 2. Mathematical Hardware Bus & Frame Transmission Physics Model

Mathematical limits of the 80 MHz FSPI display bus and CPU computation budget per resolution mode:

| Platform / Viewport | Native Res | Rendered Res | Frame Size | SPI Wire Time | Target FPS | Seq. CPU Budget | Parallel DMA Budget | Bus Saturation |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Game Boy DMG (1.5x Scaled)** | 160×144 | 240×216 | 103,680 B | 10.37 ms | 59.7 | 6.37 ms (38%) | 16.74 ms (100%) | 61.9% |
| **Game Boy Color (1.5x Scaled)** | 160×144 | 240×216 | 103,680 B | 10.37 ms | 59.7 | 6.37 ms (38%) | 16.74 ms (100%) | 61.9% |
| **NES (Native 256x240)** | 256×240 | 256×240 | 122,880 B | 12.29 ms | 60.1 | 4.35 ms (26%) | 16.64 ms (100%) | 73.9% |
| **DOOM (Native 320x200)** | 320×200 | 320×200 | 128,000 B | 12.80 ms | 35.0 | 15.77 ms (55%) | 28.57 ms (100%) | 44.8% |
| **Sega Master System (Native 256x192)** | 256×192 | 256×192 | 98,304 B | 9.83 ms | 59.9 | 6.86 ms (41%) | 16.69 ms (100%) | 58.9% |
| **Game Gear (Native 160x144)** | 160×144 | 160×144 | 46,080 B | 4.61 ms | 59.9 | 12.08 ms (72%) | 16.69 ms (100%) | 27.6% |
| **PC Engine (256x240)** | 256×240 | 256×240 | 122,880 B | 12.29 ms | 59.8 | 4.43 ms (26%) | 16.72 ms (100%) | 73.5% |
| **Atari 2600 (160x192)** | 160×192 | 160×192 | 61,440 B | 6.15 ms | 60.0 | 10.52 ms (63%) | 16.67 ms (100%) | 36.9% |
| **PICO-8 (128x128)** | 128×128 | 128×128 | 32,768 B | 3.28 ms | 30.0 | 30.06 ms (90%) | 33.33 ms (100%) | 9.8% |
| **Sega Genesis (320x224)** | 320×224 | 320×224 | 143,360 B | 14.34 ms | 59.9 | 2.35 ms (14%) | 16.69 ms (100%) | 85.9% |
| **Super Nintendo (256x224)** | 256×224 | 256×224 | 114,688 B | 11.47 ms | 60.1 | 5.17 ms (31%) | 16.64 ms (100%) | 68.9% |
| **WonderSwan (224x144)** | 224×144 | 224×144 | 64,512 B | 6.45 ms | 75.5 | 6.80 ms (51%) | 13.25 ms (100%) | 48.7% |
| **Neo Geo Pocket (160x152)** | 160×152 | 160×152 | 48,640 B | 4.87 ms | 59.7 | 11.88 ms (71%) | 16.74 ms (100%) | 29.1% |
| **Atari Lynx (160x102)** | 160×102 | 160×102 | 32,640 B | 3.27 ms | 75.0 | 10.07 ms (76%) | 13.33 ms (100%) | 24.5% |
| **ColecoVision (256x192)** | 256×192 | 256×192 | 98,304 B | 9.83 ms | 60.0 | 6.84 ms (41%) | 16.67 ms (100%) | 59.0% |
| **Menu UI Fullscreen (320x240)** | 320×240 | 320×240 | 153,600 B | 15.36 ms | 60.0 | 1.31 ms (8%) | 16.67 ms (100%) | 92.2% |

> [!IMPORTANT]
> **Hardware Bus Insight:** At 80 MHz SPI, transmitting a full 320×240 frame consumes **15.36 ms** (92.1% of a 16.67 ms frame budget). In sequential blocking mode, only **1.31 ms** of CPU time remains. **DMA double-buffering is mathematically required** to achieve full 60 FPS on 320×240 without frame drops.

---

## 3. Comprehensive Line-by-Line Repository Codebase Audit

Exhaustive line-by-line statistics across every single file in the repository:

| File Path | Category | Total Lines | SLOC | Comments | Blanks | Code Density |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `.agents/rules/00_hard_stops.md` | `AGENT_RULES` | 6 | 6 | 0 | 0 | 100.0% |
| `.agents/rules/01_hardware.md` | `AGENT_RULES` | 58 | 50 | 0 | 8 | 86.2% |
| `.agents/rules/02_architecture.md` | `AGENT_RULES` | 25 | 23 | 0 | 2 | 92.0% |
| `.agents/rules/03_conventions.md` | `AGENT_RULES` | 63 | 53 | 0 | 10 | 84.1% |
| `.agents/rules/04_known_issues.md` | `AGENT_RULES` | 413 | 349 | 0 | 64 | 84.5% |
| `.agents/rules/05_git_workflow.md` | `AGENT_RULES` | 35 | 29 | 0 | 6 | 82.9% |
| `.agents/rules/06_verification_standards.md` | `AGENT_RULES` | 118 | 101 | 0 | 17 | 85.6% |
| `.agents/rules/07_task_protocol.md` | `AGENT_RULES` | 86 | 80 | 0 | 6 | 93.0% |
| `.agents/rules/08_ui_style_guide.md` | `AGENT_RULES` | 49 | 44 | 0 | 5 | 89.8% |
| `.agents/rules/09_testing_infrastructure.md` | `AGENT_RULES` | 20 | 17 | 0 | 3 | 85.0% |
| `.agents/rules/10_symbol_reference.md` | `AGENT_RULES` | 376 | 343 | 0 | 33 | 91.2% |
| `.agents/rules/11_rules_meta.md` | `AGENT_RULES` | 22 | 21 | 0 | 1 | 95.5% |
| `.agents/rules/12_extensibility_contract.md` | `AGENT_RULES` | 42 | 38 | 0 | 4 | 90.5% |
| `.agents/rules/13_code_style.md` | `AGENT_RULES` | 27 | 21 | 0 | 6 | 77.8% |
| `.agents/rules/14_error_handling_and_fault_isolation.md` | `AGENT_RULES` | 25 | 20 | 0 | 5 | 80.0% |
| `.agents/rules/15_performance_budgets.md` | `AGENT_RULES` | 51 | 42 | 0 | 9 | 82.4% |
| `.agents/rules/16_logging_and_diagnostics.md` | `AGENT_RULES` | 20 | 15 | 0 | 5 | 75.0% |
| `.agents/rules/17_release_and_versioning.md` | `AGENT_RULES` | 25 | 20 | 0 | 5 | 80.0% |
| `.agents/rules/18_dependency_and_vendor_sync.md` | `AGENT_RULES` | 34 | 28 | 0 | 6 | 82.4% |
| `.agents/rules/19_security_and_data_integrity.md` | `AGENT_RULES` | 34 | 26 | 0 | 8 | 76.5% |
| `.agents/rules/20_multi_agent_protocol.md` | `AGENT_RULES` | 19 | 15 | 0 | 4 | 78.9% |
| `.agents/rules/21_documentation_standards.md` | `AGENT_RULES` | 19 | 15 | 0 | 4 | 78.9% |
| `.agents/rules/22_review_checklist.md` | `AGENT_RULES` | 14 | 12 | 0 | 2 | 85.7% |
| `.agents/rules/23_incident_postmortem_log.md` | `AGENT_RULES` | 35 | 29 | 0 | 6 | 82.9% |
| `.agents/rules/24_vendor_flag_safety.md` | `AGENT_RULES` | 78 | 68 | 0 | 10 | 87.2% |
| `.agents/rules/25_game_compatibility_ledger.md` | `AGENT_RULES` | 52 | 44 | 0 | 8 | 84.6% |
| `.agents/rules/26_emulator_exit_contract.md` | `AGENT_RULES` | 19 | 16 | 0 | 3 | 84.2% |
| `.agents/rules/27_codebase_map.md` | `AGENT_RULES` | 125 | 112 | 0 | 13 | 89.6% |
| `.agents/rules/28_display_and_spi_contract.md` | `AGENT_RULES` | 85 | 76 | 0 | 9 | 89.4% |
| `.agents/rules/29_adding_a_baked_rom.md` | `AGENT_RULES` | 80 | 67 | 0 | 13 | 83.8% |
| `.agents/rules/30_common_agent_mistakes.md` | `AGENT_RULES` | 170 | 129 | 0 | 41 | 75.9% |
| `.agents/rules/31_quick_start_primer.md` | `AGENT_RULES` | 87 | 71 | 0 | 16 | 81.6% |
| `.agents/rules/32_modular_core_template.md` | `AGENT_RULES` | 128 | 96 | 0 | 32 | 75.0% |
| `.agents/rules/33_agent_handoff_and_optimization_cycle.md` | `AGENT_RULES` | 59 | 44 | 0 | 15 | 74.6% |
| `.agents/rules/34_ai_agent_sandbox_and_guardrails.md` | `AGENT_RULES` | 51 | 37 | 0 | 14 | 72.5% |
| `.agents/rules/35_bmo_face_contract.md` | `AGENT_RULES` | 71 | 64 | 0 | 7 | 90.1% |
| `.agents/rules/36_bug_intake_protocol.md` | `AGENT_RULES` | 68 | 60 | 0 | 8 | 88.2% |
| `.agents/rules/37_rom_governance_and_flash_budget.md` | `AGENT_RULES` | 59 | 53 | 0 | 6 | 89.8% |
| `.agents/rules/38_mandatory_documentation_update.md` | `AGENT_RULES` | 96 | 74 | 0 | 22 | 77.1% |
| `.agents/rules/39_performance_and_benchmark_framework.md` | `AGENT_RULES` | 70 | 51 | 0 | 19 | 72.9% |
| `.agents/rules/40_ai_knowledge_graph_and_indexing_protocol.md` | `AGENT_RULES` | 76 | 59 | 0 | 17 | 77.6% |
| `.agents/rules/41_engineering_communication_and_critical_pushback.md` | `AGENT_RULES` | 80 | 60 | 0 | 20 | 75.0% |
| `.agents/rules/42_quality_of_life_and_roadmap_ledger.md` | `AGENT_RULES` | 120 | 90 | 0 | 30 | 75.0% |
| `.agents/rules/43_no_ai_jargon_and_buzzwords.md` | `AGENT_RULES` | 50 | 35 | 0 | 15 | 70.0% |
| `.agents/rules/CONTEXT_INDEX.json` | `AGENT_RULES` | 337 | 337 | 0 | 0 | 100.0% |
| `.agents/rules/README.md` | `AGENT_RULES` | 53 | 51 | 0 | 2 | 96.2% |
| `AGENT_DECISION_TREE.json` | `OTHER` | 287 | 287 | 0 | 0 | 100.0% |
| `AGENT_KNOWLEDGE_GRAPH.json` | `OTHER` | 18,314 | 18,314 | 0 | 0 | 100.0% |
| `AGENT_MANIFEST.json` | `OTHER` | 309 | 309 | 0 | 0 | 100.0% |
| `AGENTS.md` | `OTHER` | 18 | 15 | 0 | 3 | 83.3% |
| `CHANGELOG.md` | `OTHER` | 635 | 564 | 0 | 71 | 88.8% |
| `diagram.json` | `OTHER` | 18 | 18 | 0 | 0 | 100.0% |
| `docs/consolidated-rules.md` | `DOCUMENTATION` | 3,512 | 2,798 | 44 | 670 | 79.7% |
| `docs/hardware-notes.md` | `DOCUMENTATION` | 162 | 112 | 0 | 50 | 69.1% |
| `docs/performance_and_benchmarking.md` | `DOCUMENTATION` | 113 | 89 | 0 | 24 | 78.8% |
| `docs/performance_report.md` | `DOCUMENTATION` | 499 | 477 | 0 | 22 | 95.6% |
| `docs/research_embedded_ui_frameworks_unevaluated.md` | `DOCUMENTATION` | 57 | 48 | 0 | 9 | 84.2% |
| `docs/software-design-document.md` | `DOCUMENTATION` | 463 | 378 | 0 | 85 | 81.6% |
| `docs/wiring/01-display-and-buttons.md` | `DOCUMENTATION` | 52 | 40 | 0 | 12 | 76.9% |
| `docs/wiring/02-sd-card.md` | `DOCUMENTATION` | 54 | 43 | 0 | 11 | 79.6% |
| `firmware/03_emulator/build/esp32.esp32.esp32s3/build.options.json` | `OTHER` | 9 | 9 | 0 | 0 | 100.0% |
| `firmware/03_emulator/build/esp32.esp32.esp32s3/partitions.csv` | `OTHER` | 5 | 5 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/BmoGameboy.ino` | `FIRMWARE_TOP` | 1,160 | 1,000 | 64 | 96 | 86.2% |
| `firmware/BmoGameboy/partitions.csv` | `FIRMWARE_TOP` | 5 | 5 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/src/assets/bmo_assets.h` | `BAKED_ROMS` | 17 | 13 | 2 | 2 | 76.5% |
| `firmware/BmoGameboy/src/assets/rom_data.h` | `BAKED_ROMS` | 13,120 | 13,120 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/src/assets/roms/aladdin.h` | `BAKED_ROMS` | 65,542 | 65,542 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/src/assets/roms/lego_racers.h` | `BAKED_ROMS` | 65,542 | 65,542 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/src/assets/roms/mario_deluxe.h` | `BAKED_ROMS` | 65,542 | 65,542 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/src/assets/roms/virtual_bmo.h` | `BAKED_ROMS` | 32,776 | 32,776 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/src/assets/roms/zelda_ages.h` | `BAKED_ROMS` | 65,542 | 65,542 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/src/core/audio_i2s.cpp` | `FIRMWARE_CORE` | 63 | 51 | 2 | 10 | 81.0% |
| `firmware/BmoGameboy/src/core/audio_i2s.h` | `FIRMWARE_CORE` | 9 | 7 | 1 | 1 | 77.8% |
| `firmware/BmoGameboy/src/core/battery.cpp` | `FIRMWARE_CORE` | 79 | 68 | 2 | 9 | 86.1% |
| `firmware/BmoGameboy/src/core/battery.h` | `FIRMWARE_CORE` | 18 | 9 | 4 | 5 | 50.0% |
| `firmware/BmoGameboy/src/core/bmo_face.cpp` | `FIRMWARE_CORE` | 1,190 | 900 | 141 | 149 | 75.6% |
| `firmware/BmoGameboy/src/core/bmo_face.h` | `FIRMWARE_CORE` | 124 | 58 | 46 | 20 | 46.8% |
| `firmware/BmoGameboy/src/core/box_art.cpp` | `FIRMWARE_CORE` | 282 | 224 | 13 | 45 | 79.4% |
| `firmware/BmoGameboy/src/core/box_art.h` | `FIRMWARE_CORE` | 36 | 22 | 7 | 7 | 61.1% |
| `firmware/BmoGameboy/src/core/buttons.cpp` | `FIRMWARE_CORE` | 123 | 96 | 7 | 20 | 78.0% |
| `firmware/BmoGameboy/src/core/buttons.h` | `FIRMWARE_CORE` | 48 | 23 | 15 | 10 | 47.9% |
| `firmware/BmoGameboy/src/core/config.h` | `FIRMWARE_CORE` | 109 | 63 | 31 | 15 | 57.8% |
| `firmware/BmoGameboy/src/core/display_emu.cpp` | `FIRMWARE_CORE` | 1,893 | 1,534 | 121 | 238 | 81.0% |
| `firmware/BmoGameboy/src/core/display_emu.h` | `FIRMWARE_CORE` | 178 | 64 | 69 | 45 | 36.0% |
| `firmware/BmoGameboy/src/core/fram_save.cpp` | `FIRMWARE_CORE` | 74 | 55 | 5 | 14 | 74.3% |
| `firmware/BmoGameboy/src/core/fram_save.h` | `FIRMWARE_CORE` | 13 | 8 | 2 | 3 | 61.5% |
| `firmware/BmoGameboy/src/core/profiler.cpp` | `FIRMWARE_CORE` | 102 | 82 | 1 | 19 | 80.4% |
| `firmware/BmoGameboy/src/core/profiler.h` | `FIRMWARE_CORE` | 64 | 40 | 11 | 13 | 62.5% |
| `firmware/BmoGameboy/src/core/rom_index.h` | `FIRMWARE_CORE` | 33 | 25 | 2 | 6 | 75.8% |
| `firmware/BmoGameboy/src/core/save_manager.cpp` | `FIRMWARE_CORE` | 487 | 404 | 7 | 76 | 83.0% |
| `firmware/BmoGameboy/src/core/save_manager.h` | `FIRMWARE_CORE` | 73 | 54 | 7 | 12 | 74.0% |
| `firmware/BmoGameboy/src/core/sd_card.cpp` | `FIRMWARE_CORE` | 680 | 586 | 19 | 75 | 86.2% |
| `firmware/BmoGameboy/src/core/sd_card.h` | `FIRMWARE_CORE` | 70 | 55 | 6 | 9 | 78.6% |
| `firmware/BmoGameboy/src/core/spi_arbiter.cpp` | `FIRMWARE_CORE` | 22 | 18 | 0 | 4 | 81.8% |
| `firmware/BmoGameboy/src/core/spi_arbiter.h` | `FIRMWARE_CORE` | 27 | 9 | 14 | 4 | 33.3% |
| `firmware/BmoGameboy/src/core/theme.h` | `FIRMWARE_CORE` | 111 | 75 | 22 | 14 | 67.6% |
| `firmware/BmoGameboy/src/emulators/emu_atari.cpp` | `EMULATOR_WRAPPERS` | 96 | 80 | 1 | 15 | 83.3% |
| `firmware/BmoGameboy/src/emulators/emu_atari.h` | `EMULATOR_WRAPPERS` | 17 | 14 | 0 | 3 | 82.4% |
| `firmware/BmoGameboy/src/emulators/emu_colem.cpp` | `EMULATOR_WRAPPERS` | 112 | 99 | 0 | 13 | 88.4% |
| `firmware/BmoGameboy/src/emulators/emu_colem.h` | `EMULATOR_WRAPPERS` | 16 | 14 | 0 | 2 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_doom.cpp` | `EMULATOR_WRAPPERS` | 171 | 119 | 25 | 27 | 69.6% |
| `firmware/BmoGameboy/src/emulators/emu_doom.h` | `EMULATOR_WRAPPERS` | 15 | 8 | 4 | 3 | 53.3% |
| `firmware/BmoGameboy/src/emulators/emu_genesis.cpp` | `EMULATOR_WRAPPERS` | 113 | 100 | 0 | 13 | 88.5% |
| `firmware/BmoGameboy/src/emulators/emu_genesis.h` | `EMULATOR_WRAPPERS` | 16 | 14 | 0 | 2 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_lynx.cpp` | `EMULATOR_WRAPPERS` | 104 | 91 | 0 | 13 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_lynx.h` | `EMULATOR_WRAPPERS` | 16 | 14 | 0 | 2 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_nes.cpp` | `EMULATOR_WRAPPERS` | 126 | 104 | 5 | 17 | 82.5% |
| `firmware/BmoGameboy/src/emulators/emu_nes.h` | `EMULATOR_WRAPPERS` | 18 | 14 | 1 | 3 | 77.8% |
| `firmware/BmoGameboy/src/emulators/emu_ngp.cpp` | `EMULATOR_WRAPPERS` | 104 | 91 | 0 | 13 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_ngp.h` | `EMULATOR_WRAPPERS` | 16 | 14 | 0 | 2 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_pce.cpp` | `EMULATOR_WRAPPERS` | 99 | 82 | 1 | 16 | 82.8% |
| `firmware/BmoGameboy/src/emulators/emu_pce.h` | `EMULATOR_WRAPPERS` | 19 | 14 | 1 | 4 | 73.7% |
| `firmware/BmoGameboy/src/emulators/emu_peanut.cpp` | `EMULATOR_WRAPPERS` | 222 | 162 | 24 | 36 | 73.0% |
| `firmware/BmoGameboy/src/emulators/emu_peanut.h` | `EMULATOR_WRAPPERS` | 22 | 12 | 5 | 5 | 54.5% |
| `firmware/BmoGameboy/src/emulators/emu_pico.cpp` | `EMULATOR_WRAPPERS` | 102 | 85 | 2 | 15 | 83.3% |
| `firmware/BmoGameboy/src/emulators/emu_pico.h` | `EMULATOR_WRAPPERS` | 17 | 14 | 0 | 3 | 82.4% |
| `firmware/BmoGameboy/src/emulators/emu_sms.cpp` | `EMULATOR_WRAPPERS` | 104 | 84 | 5 | 15 | 80.8% |
| `firmware/BmoGameboy/src/emulators/emu_sms.h` | `EMULATOR_WRAPPERS` | 18 | 14 | 1 | 3 | 77.8% |
| `firmware/BmoGameboy/src/emulators/emu_snes.cpp` | `EMULATOR_WRAPPERS` | 110 | 97 | 0 | 13 | 88.2% |
| `firmware/BmoGameboy/src/emulators/emu_snes.h` | `EMULATOR_WRAPPERS` | 16 | 14 | 0 | 2 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_walnut.cpp` | `EMULATOR_WRAPPERS` | 282 | 183 | 58 | 41 | 64.9% |
| `firmware/BmoGameboy/src/emulators/emu_walnut.h` | `EMULATOR_WRAPPERS` | 22 | 12 | 5 | 5 | 54.5% |
| `firmware/BmoGameboy/src/emulators/emu_wswan.cpp` | `EMULATOR_WRAPPERS` | 107 | 94 | 0 | 13 | 87.9% |
| `firmware/BmoGameboy/src/emulators/emu_wswan.h` | `EMULATOR_WRAPPERS` | 16 | 14 | 0 | 2 | 87.5% |
| `firmware/BmoGameboy/src/engine/walnut_cgb/walnut_cgb.h` | `ENGINE_GBC` | 9,938 | 7,706 | 889 | 1,343 | 77.5% |
| `firmware/BmoGameboy/src/tests/unit_tests.cpp` | `FIRMWARE_TOP` | 157 | 106 | 23 | 28 | 67.5% |
| `firmware/BmoGameboy/src/tests/unit_tests.h` | `FIRMWARE_TOP` | 5 | 2 | 2 | 1 | 40.0% |
| `firmware/BmoGameboy/src/vendor/agnes/agnes.c` | `VENDOR_ENGINES` | 2,594 | 2,180 | 96 | 318 | 84.0% |
| `firmware/BmoGameboy/src/vendor/agnes/agnes.h` | `VENDOR_ENGINES` | 87 | 50 | 21 | 16 | 57.5% |
| `firmware/BmoGameboy/src/vendor/colem/colem.c` | `VENDOR_ENGINES` | 400 | 343 | 26 | 31 | 85.8% |
| `firmware/BmoGameboy/src/vendor/colem/colem.h` | `VENDOR_ENGINES` | 44 | 33 | 3 | 8 | 75.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/am_map.c` | `VENDOR_ENGINES` | 1,358 | 973 | 162 | 223 | 71.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/am_map.h` | `VENDOR_ENGINES` | 49 | 13 | 24 | 12 | 26.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/config.h` | `VENDOR_ENGINES` | 100 | 33 | 34 | 33 | 33.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_englsh.h` | `VENDOR_ENGINES` | 693 | 522 | 65 | 106 | 75.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_event.c` | `VENDOR_ENGINES` | 63 | 22 | 27 | 14 | 34.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_event.h` | `VENDOR_ENGINES` | 137 | 43 | 66 | 28 | 31.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_items.c` | `VENDOR_ENGINES` | 128 | 77 | 38 | 13 | 60.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_items.h` | `VENDOR_ENGINES` | 41 | 14 | 18 | 9 | 34.1% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_iwad.c` | `VENDOR_ENGINES` | 848 | 509 | 133 | 206 | 60.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_iwad.h` | `VENDOR_ENGINES` | 52 | 28 | 16 | 8 | 53.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_loop.c` | `VENDOR_ENGINES` | 826 | 464 | 169 | 193 | 56.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_loop.h` | `VENDOR_ENGINES` | 81 | 23 | 34 | 24 | 28.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_main.c` | `VENDOR_ENGINES` | 1,845 | 1,089 | 418 | 338 | 59.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_main.h` | `VENDOR_ENGINES` | 50 | 11 | 24 | 15 | 22.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_mode.c` | `VENDOR_ENGINES` | 209 | 150 | 28 | 31 | 71.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_mode.h` | `VENDOR_ENGINES` | 98 | 58 | 24 | 16 | 59.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_net.c` | `VENDOR_ENGINES` | 281 | 158 | 61 | 62 | 56.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_player.h` | `VENDOR_ENGINES` | 209 | 82 | 77 | 50 | 39.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_textur.h` | `VENDOR_ENGINES` | 43 | 10 | 22 | 11 | 23.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_think.h` | `VENDOR_ENGINES` | 68 | 19 | 29 | 20 | 27.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_ticcmd.h` | `VENDOR_ENGINES` | 56 | 17 | 26 | 13 | 30.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/deh_main.h` | `VENDOR_ENGINES` | 48 | 19 | 19 | 10 | 39.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/deh_misc.h` | `VENDOR_ENGINES` | 83 | 55 | 17 | 11 | 66.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/deh_str.h` | `VENDOR_ENGINES` | 47 | 18 | 17 | 12 | 38.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/doom.h` | `VENDOR_ENGINES` | 42 | 4 | 27 | 11 | 9.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/doomdata.h` | `VENDOR_ENGINES` | 213 | 93 | 70 | 50 | 43.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/doomdef.c` | `VENDOR_ENGINES` | 28 | 1 | 20 | 7 | 3.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/doomdef.h` | `VENDOR_ENGINES` | 168 | 86 | 48 | 34 | 51.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/doomfeatures.h` | `VENDOR_ENGINES` | 40 | 6 | 22 | 12 | 15.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/doomgeneric.c` | `VENDOR_ENGINES` | 53 | 41 | 1 | 11 | 77.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/doomgeneric.h` | `VENDOR_ENGINES` | 50 | 33 | 1 | 16 | 66.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/doomkeys.h` | `VENDOR_ENGINES` | 97 | 61 | 23 | 13 | 62.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/doomstat.c` | `VENDOR_ENGINES` | 35 | 7 | 19 | 9 | 20.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/doomstat.h` | `VENDOR_ENGINES` | 281 | 77 | 108 | 96 | 27.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/doomtype.h` | `VENDOR_ENGINES` | 106 | 40 | 36 | 30 | 37.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/dstrings.c` | `VENDOR_ENGINES` | 73 | 37 | 22 | 14 | 50.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/dstrings.h` | `VENDOR_ENGINES` | 41 | 8 | 22 | 11 | 19.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/dummy.c` | `VENDOR_ENGINES` | 53 | 8 | 30 | 15 | 15.1% |
| `firmware/BmoGameboy/src/vendor/doom/src/f_finale.c` | `VENDOR_ENGINES` | 718 | 528 | 77 | 113 | 73.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/f_finale.h` | `VENDOR_ENGINES` | 45 | 9 | 23 | 13 | 20.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/f_wipe.c` | `VENDOR_ENGINES` | 294 | 220 | 31 | 43 | 74.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/f_wipe.h` | `VENDOR_ENGINES` | 63 | 29 | 22 | 12 | 46.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/g_game.c` | `VENDOR_ENGINES` | 2,303 | 1,572 | 317 | 414 | 68.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/g_game.h` | `VENDOR_ENGINES` | 80 | 29 | 28 | 23 | 36.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/gusconf.c` | `VENDOR_ENGINES` | 271 | 190 | 24 | 57 | 70.1% |
| `firmware/BmoGameboy/src/vendor/doom/src/gusconf.h` | `VENDOR_ENGINES` | 29 | 7 | 16 | 6 | 24.1% |
| `firmware/BmoGameboy/src/vendor/doom/src/hu_lib.c` | `VENDOR_ENGINES` | 347 | 252 | 32 | 63 | 72.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/hu_lib.h` | `VENDOR_ENGINES` | 182 | 77 | 59 | 46 | 42.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/hu_stuff.c` | `VENDOR_ENGINES` | 641 | 493 | 51 | 97 | 76.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/hu_stuff.h` | `VENDOR_ENGINES` | 59 | 21 | 23 | 15 | 35.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_cdmus.c` | `VENDOR_ENGINES` | 243 | 159 | 31 | 53 | 65.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_cdmus.h` | `VENDOR_ENGINES` | 41 | 20 | 15 | 6 | 48.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_endoom.c` | `VENDOR_ENGINES` | 101 | 48 | 26 | 27 | 47.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_endoom.h` | `VENDOR_ENGINES` | 29 | 4 | 19 | 6 | 13.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_input.c` | `VENDOR_ENGINES` | 341 | 133 | 173 | 35 | 39.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_joystick.c` | `VENDOR_ENGINES` | 359 | 254 | 38 | 67 | 70.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_joystick.h` | `VENDOR_ENGINES` | 70 | 21 | 33 | 16 | 30.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_scale.c` | `VENDOR_ENGINES` | 1,452 | 889 | 225 | 338 | 61.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_scale.h` | `VENDOR_ENGINES` | 53 | 21 | 20 | 12 | 39.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_sound.c` | `VENDOR_ENGINES` | 420 | 284 | 62 | 74 | 67.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_sound.h` | `VENDOR_ENGINES` | 256 | 104 | 71 | 81 | 40.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_swap.h` | `VENDOR_ENGINES` | 73 | 34 | 18 | 21 | 46.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_system.c` | `VENDOR_ENGINES` | 607 | 375 | 112 | 120 | 61.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_system.h` | `VENDOR_ENGINES` | 84 | 19 | 37 | 28 | 22.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_timer.c` | `VENDOR_ENGINES` | 96 | 36 | 32 | 28 | 37.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_timer.h` | `VENDOR_ENGINES` | 42 | 9 | 23 | 10 | 21.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_video.c` | `VENDOR_ENGINES` | 495 | 309 | 74 | 112 | 62.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_video.h` | `VENDOR_ENGINES` | 175 | 63 | 65 | 47 | 36.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/icon.c` | `VENDOR_ENGINES` | 262 | 260 | 0 | 2 | 99.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/info.c` | `VENDOR_ENGINES` | 4,662 | 4,492 | 21 | 149 | 96.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/info.h` | `VENDOR_ENGINES` | 1,331 | 1,296 | 21 | 14 | 97.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_argv.c` | `VENDOR_ENGINES` | 265 | 150 | 52 | 63 | 56.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_argv.h` | `VENDOR_ENGINES` | 49 | 11 | 26 | 12 | 22.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_bbox.c` | `VENDOR_ENGINES` | 54 | 21 | 20 | 13 | 38.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_bbox.h` | `VENDOR_ENGINES` | 47 | 18 | 19 | 10 | 38.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_cheat.c` | `VENDOR_ENGINES` | 89 | 38 | 32 | 19 | 42.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_cheat.h` | `VENDOR_ENGINES` | 62 | 24 | 23 | 15 | 38.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_config.c` | `VENDOR_ENGINES` | 2,129 | 616 | 948 | 565 | 28.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_config.h` | `VENDOR_ENGINES` | 39 | 16 | 17 | 6 | 41.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_controls.c` | `VENDOR_ENGINES` | 398 | 269 | 57 | 72 | 67.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_controls.h` | `VENDOR_ENGINES` | 168 | 121 | 17 | 30 | 72.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_fixed.c` | `VENDOR_ENGINES` | 62 | 24 | 21 | 17 | 38.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_fixed.h` | `VENDOR_ENGINES` | 39 | 8 | 20 | 11 | 20.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_menu.c` | `VENDOR_ENGINES` | 2,125 | 1,502 | 250 | 373 | 70.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_menu.h` | `VENDOR_ENGINES` | 61 | 11 | 33 | 17 | 18.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_misc.c` | `VENDOR_ENGINES` | 536 | 335 | 87 | 114 | 62.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_misc.h` | `VENDOR_ENGINES` | 51 | 27 | 17 | 7 | 52.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_random.c` | `VENDOR_ENGINES` | 65 | 37 | 22 | 6 | 56.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_random.h` | `VENDOR_ENGINES` | 39 | 7 | 21 | 11 | 17.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/memio.c` | `VENDOR_ENGINES` | 197 | 122 | 27 | 48 | 61.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/memio.h` | `VENDOR_ENGINES` | 38 | 18 | 14 | 6 | 47.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/mus2mid.c` | `VENDOR_ENGINES` | 737 | 504 | 76 | 157 | 68.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/mus2mid.h` | `VENDOR_ENGINES` | 9 | 6 | 0 | 3 | 66.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/net_client.h` | `VENDOR_ENGINES` | 52 | 29 | 15 | 8 | 55.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/net_dedicated.h` | `VENDOR_ENGINES` | 25 | 4 | 16 | 5 | 16.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/net_defs.h` | `VENDOR_ENGINES` | 248 | 148 | 46 | 54 | 59.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/net_gui.h` | `VENDOR_ENGINES` | 29 | 5 | 18 | 6 | 17.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/net_io.h` | `VENDOR_ENGINES` | 36 | 14 | 16 | 6 | 38.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/net_loop.h` | `VENDOR_ENGINES` | 27 | 6 | 16 | 5 | 22.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/net_packet.h` | `VENDOR_ENGINES` | 44 | 18 | 16 | 10 | 40.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/net_query.h` | `VENDOR_ENGINES` | 44 | 19 | 16 | 9 | 43.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/net_sdl.h` | `VENDOR_ENGINES` | 26 | 5 | 16 | 5 | 19.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/net_server.h` | `VENDOR_ENGINES` | 42 | 8 | 21 | 13 | 19.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_ceilng.c` | `VENDOR_ENGINES` | 324 | 226 | 47 | 51 | 69.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_doors.c` | `VENDOR_ENGINES` | 778 | 571 | 83 | 124 | 73.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_enemy.c` | `VENDOR_ENGINES` | 2,006 | 1,341 | 238 | 427 | 66.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_floor.c` | `VENDOR_ENGINES` | 546 | 429 | 54 | 63 | 78.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_inter.c` | `VENDOR_ENGINES` | 922 | 643 | 115 | 164 | 69.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_inter.h` | `VENDOR_ENGINES` | 30 | 4 | 17 | 9 | 13.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_lights.c` | `VENDOR_ENGINES` | 350 | 201 | 72 | 77 | 57.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_local.h` | `VENDOR_ENGINES` | 297 | 155 | 66 | 76 | 52.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_map.c` | `VENDOR_ENGINES` | 1,448 | 828 | 304 | 316 | 57.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_maputl.c` | `VENDOR_ENGINES` | 1,001 | 651 | 161 | 189 | 65.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_mobj.c` | `VENDOR_ENGINES` | 1,060 | 663 | 186 | 211 | 62.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_mobj.h` | `VENDOR_ENGINES` | 284 | 77 | 161 | 46 | 27.1% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_plats.c` | `VENDOR_ENGINES` | 304 | 218 | 29 | 57 | 71.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_pspr.c` | `VENDOR_ENGINES` | 888 | 537 | 173 | 178 | 60.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_pspr.h` | `VENDOR_ENGINES` | 71 | 21 | 35 | 15 | 29.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_saveg.c` | `VENDOR_ENGINES` | 1,891 | 975 | 435 | 481 | 51.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_saveg.h` | `VENDOR_ENGINES` | 62 | 21 | 24 | 17 | 33.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_setup.c` | `VENDOR_ENGINES` | 855 | 547 | 134 | 174 | 64.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_setup.h` | `VENDOR_ENGINES` | 37 | 10 | 19 | 8 | 27.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_sight.c` | `VENDOR_ENGINES` | 350 | 204 | 75 | 71 | 58.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_spec.c` | `VENDOR_ENGINES` | 1,492 | 911 | 307 | 274 | 61.1% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_spec.h` | `VENDOR_ENGINES` | 637 | 380 | 75 | 182 | 59.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_switch.c` | `VENDOR_ENGINES` | 648 | 425 | 111 | 112 | 65.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_telept.c` | `VENDOR_ENGINES` | 133 | 69 | 35 | 29 | 51.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_tick.c` | `VENDOR_ENGINES` | 151 | 63 | 53 | 35 | 41.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_tick.h` | `VENDOR_ENGINES` | 33 | 4 | 20 | 9 | 12.1% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_user.c` | `VENDOR_ENGINES` | 379 | 219 | 73 | 87 | 57.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_bsp.c` | `VENDOR_ENGINES` | 573 | 329 | 122 | 122 | 57.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_bsp.h` | `VENDOR_ENGINES` | 61 | 23 | 19 | 19 | 37.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_data.c` | `VENDOR_ENGINES` | 912 | 543 | 167 | 202 | 59.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_data.h` | `VENDOR_ENGINES` | 51 | 14 | 25 | 12 | 27.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_defs.h` | `VENDOR_ENGINES` | 448 | 164 | 155 | 129 | 36.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_draw.c` | `VENDOR_ENGINES` | 975 | 554 | 216 | 205 | 56.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_draw.h` | `VENDOR_ENGINES` | 104 | 40 | 33 | 31 | 38.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_local.h` | `VENDOR_ENGINES` | 45 | 13 | 24 | 8 | 28.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_main.c` | `VENDOR_ENGINES` | 914 | 577 | 155 | 182 | 63.1% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_main.h` | `VENDOR_ENGINES` | 160 | 72 | 45 | 43 | 45.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_plane.c` | `VENDOR_ENGINES` | 446 | 278 | 84 | 84 | 62.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_plane.h` | `VENDOR_ENGINES` | 76 | 37 | 18 | 21 | 48.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_segs.c` | `VENDOR_ENGINES` | 743 | 507 | 103 | 133 | 68.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_segs.h` | `VENDOR_ENGINES` | 33 | 8 | 17 | 8 | 24.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_sky.c` | `VENDOR_ENGINES` | 52 | 10 | 31 | 11 | 19.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_sky.h` | `VENDOR_ENGINES` | 37 | 8 | 20 | 9 | 21.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_state.h` | `VENDOR_ENGINES` | 127 | 49 | 36 | 42 | 38.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_things.c` | `VENDOR_ENGINES` | 982 | 605 | 171 | 206 | 61.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/r_things.h` | `VENDOR_ENGINES` | 65 | 28 | 20 | 17 | 43.1% |
| `firmware/BmoGameboy/src/vendor/doom/src/s_sound.c` | `VENDOR_ENGINES` | 670 | 431 | 108 | 131 | 64.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/s_sound.h` | `VENDOR_ENGINES` | 89 | 20 | 42 | 27 | 22.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/sha1.c` | `VENDOR_ENGINES` | 319 | 239 | 49 | 31 | 74.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/sha1.h` | `VENDOR_ENGINES` | 40 | 17 | 16 | 7 | 42.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/sounds.c` | `VENDOR_ENGINES` | 229 | 192 | 25 | 12 | 83.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/sounds.h` | `VENDOR_ENGINES` | 227 | 191 | 26 | 10 | 84.1% |
| `firmware/BmoGameboy/src/vendor/doom/src/st_lib.c` | `VENDOR_ENGINES` | 284 | 186 | 35 | 63 | 65.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/st_lib.h` | `VENDOR_ENGINES` | 209 | 91 | 60 | 58 | 43.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/st_stuff.c` | `VENDOR_ENGINES` | 1,416 | 903 | 216 | 297 | 63.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/st_stuff.h` | `VENDOR_ENGINES` | 89 | 36 | 31 | 22 | 40.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/statdump.c` | `VENDOR_ENGINES` | 392 | 239 | 52 | 101 | 61.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/statdump.h` | `VENDOR_ENGINES` | 23 | 5 | 11 | 7 | 21.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/tables.c` | `VENDOR_ENGINES` | 2,227 | 2,173 | 37 | 17 | 97.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/tables.h` | `VENDOR_ENGINES` | 96 | 26 | 44 | 26 | 27.1% |
| `firmware/BmoGameboy/src/vendor/doom/src/v_patch.h` | `VENDOR_ENGINES` | 50 | 17 | 25 | 8 | 34.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/v_video.c` | `VENDOR_ENGINES` | 932 | 617 | 139 | 176 | 66.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/v_video.h` | `VENDOR_ENGINES` | 108 | 34 | 40 | 34 | 31.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_checksum.c` | `VENDOR_ENGINES` | 95 | 55 | 22 | 18 | 57.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_checksum.h` | `VENDOR_ENGINES` | 27 | 5 | 17 | 5 | 18.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_file.c` | `VENDOR_ENGINES` | 95 | 44 | 32 | 19 | 46.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_file.h` | `VENDOR_ENGINES` | 78 | 23 | 31 | 24 | 29.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_file_stdc.c` | `VENDOR_ENGINES` | 113 | 66 | 22 | 25 | 58.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_main.c` | `VENDOR_ENGINES` | 198 | 87 | 66 | 45 | 43.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_main.h` | `VENDOR_ENGINES` | 24 | 4 | 16 | 4 | 16.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_merge.h` | `VENDOR_ENGINES` | 44 | 9 | 23 | 12 | 20.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_wad.c` | `VENDOR_ENGINES` | 653 | 393 | 124 | 136 | 60.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_wad.h` | `VENDOR_ENGINES` | 78 | 31 | 24 | 23 | 39.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/wi_stuff.c` | `VENDOR_ENGINES` | 1,829 | 1,215 | 210 | 404 | 66.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/wi_stuff.h` | `VENDOR_ENGINES` | 48 | 14 | 24 | 10 | 29.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/z_zone.c` | `VENDOR_ENGINES` | 488 | 278 | 89 | 121 | 57.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/z_zone.h` | `VENDOR_ENGINES` | 73 | 29 | 29 | 15 | 39.7% |
| `firmware/BmoGameboy/src/vendor/genesis/genesis.c` | `VENDOR_ENGINES` | 786 | 593 | 94 | 99 | 75.4% |
| `firmware/BmoGameboy/src/vendor/genesis/genesis.h` | `VENDOR_ENGINES` | 65 | 48 | 6 | 11 | 73.8% |
| `firmware/BmoGameboy/src/vendor/lynx/lynx.c` | `VENDOR_ENGINES` | 368 | 302 | 22 | 44 | 82.1% |
| `firmware/BmoGameboy/src/vendor/lynx/lynx.h` | `VENDOR_ENGINES` | 40 | 30 | 2 | 8 | 75.0% |
| `firmware/BmoGameboy/src/vendor/ngp/ngp.c` | `VENDOR_ENGINES` | 430 | 365 | 31 | 34 | 84.9% |
| `firmware/BmoGameboy/src/vendor/ngp/ngp.h` | `VENDOR_ENGINES` | 36 | 27 | 2 | 7 | 75.0% |
| `firmware/BmoGameboy/src/vendor/pce/pce.c` | `VENDOR_ENGINES` | 658 | 497 | 88 | 73 | 75.5% |
| `firmware/BmoGameboy/src/vendor/pce/pce.h` | `VENDOR_ENGINES` | 67 | 50 | 6 | 11 | 74.6% |
| `firmware/BmoGameboy/src/vendor/peanut_gb/peanut_gb.h` | `VENDOR_ENGINES` | 4,056 | 2,861 | 568 | 627 | 70.5% |
| `firmware/BmoGameboy/src/vendor/peanut_gb/peanut_gb_config.h` | `VENDOR_ENGINES` | 8 | 3 | 4 | 1 | 37.5% |
| `firmware/BmoGameboy/src/vendor/pico/pico.c` | `VENDOR_ENGINES` | 296 | 243 | 24 | 29 | 82.1% |
| `firmware/BmoGameboy/src/vendor/pico/pico.h` | `VENDOR_ENGINES` | 39 | 31 | 1 | 7 | 79.5% |
| `firmware/BmoGameboy/src/vendor/smsplus/sms.c` | `VENDOR_ENGINES` | 375 | 325 | 22 | 28 | 86.7% |
| `firmware/BmoGameboy/src/vendor/smsplus/sms.h` | `VENDOR_ENGINES` | 56 | 45 | 3 | 8 | 80.4% |
| `firmware/BmoGameboy/src/vendor/snes/snes.c` | `VENDOR_ENGINES` | 688 | 592 | 26 | 70 | 86.0% |
| `firmware/BmoGameboy/src/vendor/snes/snes.h` | `VENDOR_ENGINES` | 57 | 46 | 3 | 8 | 80.7% |
| `firmware/BmoGameboy/src/vendor/stella/atari.c` | `VENDOR_ENGINES` | 557 | 448 | 45 | 64 | 80.4% |
| `firmware/BmoGameboy/src/vendor/stella/atari.h` | `VENDOR_ENGINES` | 48 | 36 | 3 | 9 | 75.0% |
| `firmware/BmoGameboy/src/vendor/wswan/wswan.c` | `VENDOR_ENGINES` | 592 | 437 | 75 | 80 | 73.8% |
| `firmware/BmoGameboy/src/vendor/wswan/wswan.h` | `VENDOR_ENGINES` | 53 | 38 | 5 | 10 | 71.7% |
| `README.md` | `OTHER` | 177 | 131 | 0 | 46 | 74.0% |
| `scripts/auto_install_romsets.py` | `SCRIPTS` | 293 | 251 | 11 | 31 | 85.7% |
| `scripts/benchmark_repo.py` | `SCRIPTS` | 30 | 21 | 0 | 9 | 70.0% |
| `scripts/build_wasm_cores.py` | `SCRIPTS` | 352 | 247 | 43 | 62 | 70.2% |
| `scripts/bundle_rules.py` | `SCRIPTS` | 54 | 32 | 7 | 15 | 59.3% |
| `scripts/color_calc.py` | `SCRIPTS` | 52 | 37 | 4 | 11 | 71.2% |
| `scripts/convert_roms.py` | `SCRIPTS` | 66 | 52 | 3 | 11 | 78.8% |
| `scripts/fetch_covers.py` | `SCRIPTS` | 169 | 134 | 9 | 26 | 79.3% |
| `scripts/generate_ai_knowledge_base.py` | `SCRIPTS` | 430 | 385 | 12 | 33 | 89.5% |
| `scripts/install_git_hooks.py` | `SCRIPTS` | 75 | 58 | 8 | 9 | 77.3% |
| `scripts/process_games.py` | `SCRIPTS` | 139 | 116 | 0 | 23 | 83.5% |
| `scripts/repo_tools.py` | `SCRIPTS` | 31 | 22 | 0 | 9 | 71.0% |
| `scripts/test_runner.py` | `SCRIPTS` | 59 | 41 | 3 | 15 | 69.5% |
| `scripts/validate_repo.py` | `SCRIPTS` | 527 | 400 | 55 | 72 | 75.9% |
| `tests/test_all_tiers_validation.py` | `TESTS` | 334 | 235 | 63 | 36 | 70.4% |
| `tests/test_apple_nintendo_ui.py` | `TESTS` | 84 | 60 | 11 | 13 | 71.4% |
| `tests/test_bmo_mascot_engine.py` | `TESTS` | 208 | 141 | 35 | 32 | 67.8% |
| `tests/test_box_art.py` | `TESTS` | 113 | 85 | 8 | 20 | 75.2% |
| `tests/test_genesis_emu.py` | `TESTS` | 167 | 109 | 36 | 22 | 65.3% |
| `tests/test_native_cores_execution.py` | `TESTS` | 192 | 166 | 3 | 23 | 86.5% |
| `tests/test_new_cores_and_qol.py` | `TESTS` | 194 | 138 | 15 | 41 | 71.1% |
| `tests/test_pce_emu.py` | `TESTS` | 110 | 64 | 28 | 18 | 58.2% |
| `tests/test_repo_tools.py` | `TESTS` | 71 | 53 | 4 | 14 | 74.6% |
| `tests/test_rom_index.py` | `TESTS` | 175 | 124 | 23 | 28 | 70.9% |
| `tests/test_save_manager.py` | `TESTS` | 126 | 106 | 2 | 18 | 84.1% |
| `tests/test_tier1_validation.py` | `TESTS` | 126 | 98 | 13 | 15 | 77.8% |
| `tests/test_tier2_validation.py` | `TESTS` | 98 | 78 | 10 | 10 | 79.6% |
| `tests/test_wswan_emu.py` | `TESTS` | 126 | 88 | 21 | 17 | 69.8% |
| `tools/bmo_simulator/run_simulator.py` | `TOOLS` | 91 | 78 | 3 | 10 | 85.7% |
| `tools/guardian/__init__.py` | `GUARDIAN_TOOLCHAIN` | 5 | 2 | 2 | 1 | 40.0% |
| `tools/guardian/__main__.py` | `GUARDIAN_TOOLCHAIN` | 8 | 4 | 2 | 2 | 50.0% |
| `tools/guardian/cli.py` | `GUARDIAN_TOOLCHAIN` | 1,144 | 928 | 49 | 167 | 81.1% |
| `tools/guardian/core/__init__.py` | `GUARDIAN_TOOLCHAIN` | 26 | 22 | 2 | 2 | 84.6% |
| `tools/guardian/core/ast_linter.py` | `GUARDIAN_TOOLCHAIN` | 452 | 396 | 17 | 39 | 87.6% |
| `tools/guardian/core/bus_model.py` | `GUARDIAN_TOOLCHAIN` | 278 | 245 | 11 | 22 | 88.1% |
| `tools/guardian/core/cppcheck_runner.py` | `GUARDIAN_TOOLCHAIN` | 121 | 99 | 6 | 16 | 81.8% |
| `tools/guardian/core/doc_verifier.py` | `GUARDIAN_TOOLCHAIN` | 240 | 206 | 11 | 23 | 85.8% |
| `tools/guardian/core/elf_analyzer.py` | `GUARDIAN_TOOLCHAIN` | 213 | 184 | 3 | 26 | 86.4% |
| `tools/guardian/core/host_bench.py` | `GUARDIAN_TOOLCHAIN` | 297 | 251 | 11 | 35 | 84.5% |
| `tools/guardian/core/report_gen.py` | `GUARDIAN_TOOLCHAIN` | 243 | 220 | 3 | 20 | 90.5% |
| `tools/guardian/tests/__init__.py` | `GUARDIAN_TOOLCHAIN` | 3 | 1 | 2 | 0 | 33.3% |
| `tools/guardian/tests/test_guardian.py` | `GUARDIAN_TOOLCHAIN` | 270 | 213 | 18 | 39 | 78.9% |
| `tools/host_cores_runner.c` | `TOOLS` | 401 | 348 | 7 | 46 | 86.8% |
| `tools/host_test.cpp` | `TOOLS` | 123 | 99 | 2 | 22 | 80.5% |
| `tools/telemetry_plotter.py` | `TOOLS` | 170 | 138 | 6 | 26 | 81.2% |

---

## 4. Host-Side Quantitative Microbenchmarks

| Benchmark Kernel | Iterations | Total Time | Avg Latency | Throughput | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **BMO_SDF_Procedural_Face_Render_Unculled** | 100 | 112.80 ms | 1127.953 µs | 3.63 MOps/s | Evaluates raw procedural 2D SDF math across 128x128 grid without spatial culling. |
| **BMO_SDF_Procedural_Face_Render_Culled** | 200 | 87.77 ms | 438.854 µs | 9.33 MOps/s | Evaluates procedural 2D SDF with bounding-box spatial culling (~75% math reduction). |
| **4Pixel_Coalesced_Aligned_Store_Kernel** | 50,000 | 116.26 ms | 2.325 µs | 12.90 MOps/s | Measures throughput of Xtensa LX7 32-bit coalesced memory stores for 3:2 scaling. |
| **O1_Palette_Scanline_Transformation** | 10,000 | 40.63 ms | 4.063 µs | 59.08 MOps/s | Measures 240-pixel scanline indexed-to-BGR565 palette transform throughput. |
| **CPU_Emulation_Opcode_Dispatch** | 500,000 | 34.03 ms | 0.068 µs | 14.69 MOps/s | Simulates CPU opcode fetch-decode-execute cycle dispatch throughput. |
| **Direct_GPIO_Atomic_Bitmask_Sampling** | 100,000 | 47.09 ms | 0.471 µs | 16.99 MOps/s | Measures atomic 8-pin bitmask unpacking (GPIO 4,5,6,7,16,17,18,21) replacing 8 digitalRead calls. |
| **SD_Catalog_O1_Query_And_Page_Jump** | 10,000 | 1.68 ms | 0.168 µs | 5.96 MOps/s | Measures 16,384 PSRAM ROM O(1) count queries and rapid +/-10 page-jump calculations. |

---

## 5. Static AST & Anti-Pattern Linter Findings (13 total)

| Severity | Rule ID | File : Line | Issue Description | Suggested Action |
| :--- | :--- | :--- | :--- | :--- |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `d_iwad.c:217` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `d_iwad.c:346` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `d_iwad.c:762` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `d_main.c:1124` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `i_system.c:78` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `i_system.c:314` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `i_system.c:367` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `m_config.c:2045` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `p_saveg.c:70` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `v_video.c:757` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `STACK_ALLOCATED_SCANLINE_BUFFER` | `ngp.h:22` | Large scanline buffer allocated on stack in hot path. | Make static module-level or 4-byte aligned DRAM/PSRAM buffer. |
| `🟡 WARNING` | `STACK_ALLOCATED_SCANLINE_BUFFER` | `snes.h:43` | Large scanline buffer allocated on stack in hot path. | Make static module-level or 4-byte aligned DRAM/PSRAM buffer. |
| `🟡 WARNING` | `STACK_ALLOCATED_SCANLINE_BUFFER` | `wswan.h:40` | Large scanline buffer allocated on stack in hot path. | Make static module-level or 4-byte aligned DRAM/PSRAM buffer. |

---

## 6. Top Memory Consuming Symbols (Xtensa ESP32-S3 ELF)

### Largest Code Functions
| Size (Bytes) | Function Symbol | Section |
| :--- | :--- | :--- |
| 30,792 | `WGB::__gb_step_cpu(WGB::gb_s*)` | `text` |
| 19,653 | `(anonymous namespace)::renderMascotInternal(unsigned short*, int, int, int, int, int)` | `text` |
| 16,667 | `atari_cpu_step` | `text` |
| 16,500 | `colem_step_frame` | `text` |
| 15,184 | `PGB::__gb_step_cpu(PGB::gb_s*)` | `text` |
| 12,322 | `snes_cpu_step` | `text` |
| 11,413 | `_vfprintf_r` | `text` |
| 11,222 | `_svfprintf_r` | `text` |
| 9,910 | `WGB::__gb_draw_line(WGB::gb_s*)` | `text` |
| 8,765 | `__ssvfscanf_r` | `text` |

### Largest Data Objects
| Size (Bytes) | Object Symbol | Section |
| :--- | :--- | :--- |
| 1,048,576 | `aladdin_rom` | `data_or_bss` |
| 1,048,576 | `zelda_ages_rom` | `data_or_bss` |
| 1,048,576 | `lego_racers_rom` | `data_or_bss` |
| 1,048,576 | `mario_deluxe_rom` | `data_or_bss` |
| 524,288 | `virtual_bmo_rom` | `data_or_bss` |
| 49,972 | `(anonymous namespace)::gb` | `data_or_bss` |
| 40,960 | `finesine` | `data_or_bss` |
| 27,076 | `states` | `data_or_bss` |
| 17,408 | `ticdata` | `data_or_bss` |
| 16,916 | `(anonymous namespace)::gb` | `data_or_bss` |