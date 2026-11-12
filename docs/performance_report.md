# BMO Handheld Console — Complete Line-by-Line Benchmark & Architecture Audit
**Generated:** 2026-11-12 19:44:56
**Target MCU:** ESP32-S3-N16R8 (240MHz Xtensa LX7, 16MB QIO Flash, 8MB Octal PSRAM)
**Display:** ST7789VW 2.4" TFT on Shared 80MHz FSPI
**Total Repository Files Audited:** 594 files (421,480 total lines, 384,746 SLOC)
**Ground-Truth Quality Gate:** PASS (Zero Critical Violations)

---

## 1. Executive Summary & Hardware Health Dashboard

| Metric | Measured Value | Hardware Budget | Utilization | Health Status |
| :--- | :--- | :--- | :--- | :--- |
| **Internal SRAM (DRAM)** | 253,728 bytes | 327,680 bytes | 77.4% | ⚠️ TIGHT (77.4%) |
| **Program Flash Sections / Physical Chip** | 5,774,633 bytes | 16,777,216 bytes | 34.4% | ✅ OK (Chip headroom 65.6%) |
| **Static PSRAM** | 0 bytes | 8,388,608 bytes | 0.0% | ✅ OK |
| **Dynamic PSRAM Runtime** | Not measured | 8,388,608 bytes | Not measured | ⚪ DEVICE TELEMETRY REQUIRED |
| **AST Critical Findings** | 0 | 0 allowed | - | ✅ PASS (0 Critical) |
| **AST Warnings** | 9 | < 15 allowed | - | ✅ OK |
| **Cppcheck Findings** | 0 | 0 errors | - | ✅ CLEAN |

---

## 2. Mathematical Hardware Bus & Frame Transmission Physics Model

Mathematical limits of the 80 MHz FSPI display bus and CPU computation budget per resolution mode:

| Platform / Viewport | Native Res | Rendered Res | Frame Size | SPI Wire Time | Target FPS | Seq. CPU Budget | Ideal Overlap CPU Budget | Bus Saturation |
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
> **Model limitation:** At the assumed 80 MHz SPI rate, transmitting a full 320×240 frame takes **15.36 ms** (92.1% of a 16.67 ms frame period), leaving **1.31 ms** for CPU work only in a sequential transfer-and-compute schedule. The ideal overlap column is a theoretical upper bound; it does not establish that DMA is used or required, that the target frame rate is achieved, or that frames are dropped. The display path uses blocking SPI writes on a worker task, and on-device frame timing has not been measured.

---

## 3. Comprehensive Line-by-Line Repository Codebase Audit

Exhaustive line-by-line statistics across every single file in the repository:

| File Path | Category | Total Lines | SLOC | Comments | Blanks | Code Density |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `.agents/rules/00_hard_stops.md` | `AGENT_RULES` | 6 | 6 | 0 | 0 | 100.0% |
| `.agents/rules/01_hardware.md` | `AGENT_RULES` | 63 | 55 | 0 | 8 | 87.3% |
| `.agents/rules/02_architecture.md` | `AGENT_RULES` | 20 | 15 | 0 | 5 | 75.0% |
| `.agents/rules/03_conventions.md` | `AGENT_RULES` | 63 | 53 | 0 | 10 | 84.1% |
| `.agents/rules/04_known_issues.md` | `AGENT_RULES` | 78 | 69 | 0 | 9 | 88.5% |
| `.agents/rules/05_git_workflow.md` | `AGENT_RULES` | 35 | 29 | 0 | 6 | 82.9% |
| `.agents/rules/06_verification_standards.md` | `AGENT_RULES` | 117 | 100 | 0 | 17 | 85.5% |
| `.agents/rules/07_task_protocol.md` | `AGENT_RULES` | 61 | 56 | 0 | 5 | 91.8% |
| `.agents/rules/08_ui_style_guide.md` | `AGENT_RULES` | 53 | 47 | 0 | 6 | 88.7% |
| `.agents/rules/09_testing_infrastructure.md` | `AGENT_RULES` | 20 | 17 | 0 | 3 | 85.0% |
| `.agents/rules/10_symbol_reference.md` | `AGENT_RULES` | 21 | 14 | 0 | 7 | 66.7% |
| `.agents/rules/11_rules_meta.md` | `AGENT_RULES` | 22 | 21 | 0 | 1 | 95.5% |
| `.agents/rules/12_extensibility_contract.md` | `AGENT_RULES` | 21 | 17 | 0 | 4 | 81.0% |
| `.agents/rules/13_code_style.md` | `AGENT_RULES` | 27 | 21 | 0 | 6 | 77.8% |
| `.agents/rules/14_error_handling_and_fault_isolation.md` | `AGENT_RULES` | 25 | 20 | 0 | 5 | 80.0% |
| `.agents/rules/15_performance_budgets.md` | `AGENT_RULES` | 19 | 10 | 0 | 9 | 52.6% |
| `.agents/rules/16_logging_and_diagnostics.md` | `AGENT_RULES` | 20 | 15 | 0 | 5 | 75.0% |
| `.agents/rules/17_release_and_versioning.md` | `AGENT_RULES` | 25 | 20 | 0 | 5 | 80.0% |
| `.agents/rules/18_dependency_and_vendor_sync.md` | `AGENT_RULES` | 34 | 28 | 0 | 6 | 82.4% |
| `.agents/rules/19_security_and_data_integrity.md` | `AGENT_RULES` | 23 | 12 | 0 | 11 | 52.2% |
| `.agents/rules/20_multi_agent_protocol.md` | `AGENT_RULES` | 19 | 15 | 0 | 4 | 78.9% |
| `.agents/rules/21_documentation_standards.md` | `AGENT_RULES` | 19 | 15 | 0 | 4 | 78.9% |
| `.agents/rules/22_review_checklist.md` | `AGENT_RULES` | 14 | 12 | 0 | 2 | 85.7% |
| `.agents/rules/23_incident_postmortem_log.md` | `AGENT_RULES` | 35 | 29 | 0 | 6 | 82.9% |
| `.agents/rules/24_vendor_flag_safety.md` | `AGENT_RULES` | 78 | 68 | 0 | 10 | 87.2% |
| `.agents/rules/25_game_compatibility_ledger.md` | `AGENT_RULES` | 52 | 44 | 0 | 8 | 84.6% |
| `.agents/rules/26_emulator_exit_contract.md` | `AGENT_RULES` | 18 | 11 | 0 | 7 | 61.1% |
| `.agents/rules/27_codebase_map.md` | `AGENT_RULES` | 70 | 54 | 0 | 16 | 77.1% |
| `.agents/rules/28_display_and_spi_contract.md` | `AGENT_RULES` | 25 | 17 | 0 | 8 | 68.0% |
| `.agents/rules/29_adding_a_baked_rom.md` | `AGENT_RULES` | 80 | 67 | 0 | 13 | 83.8% |
| `.agents/rules/30_common_agent_mistakes.md` | `AGENT_RULES` | 170 | 129 | 0 | 41 | 75.9% |
| `.agents/rules/31_quick_start_primer.md` | `AGENT_RULES` | 33 | 21 | 0 | 12 | 63.6% |
| `.agents/rules/32_modular_core_template.md` | `AGENT_RULES` | 24 | 19 | 0 | 5 | 79.2% |
| `.agents/rules/33_agent_handoff_and_optimization_cycle.md` | `AGENT_RULES` | 59 | 44 | 0 | 15 | 74.6% |
| `.agents/rules/34_ai_agent_sandbox_and_guardrails.md` | `AGENT_RULES` | 51 | 37 | 0 | 14 | 72.5% |
| `.agents/rules/35_bmo_face_contract.md` | `AGENT_RULES` | 71 | 64 | 0 | 7 | 90.1% |
| `.agents/rules/36_bug_intake_protocol.md` | `AGENT_RULES` | 68 | 60 | 0 | 8 | 88.2% |
| `.agents/rules/37_rom_governance_and_flash_budget.md` | `AGENT_RULES` | 59 | 53 | 0 | 6 | 89.8% |
| `.agents/rules/38_mandatory_documentation_update.md` | `AGENT_RULES` | 96 | 74 | 0 | 22 | 77.1% |
| `.agents/rules/39_performance_and_benchmark_framework.md` | `AGENT_RULES` | 70 | 51 | 0 | 19 | 72.9% |
| `.agents/rules/40_ai_knowledge_graph_and_indexing_protocol.md` | `AGENT_RULES` | 76 | 59 | 0 | 17 | 77.6% |
| `.agents/rules/41_engineering_communication_and_critical_pushback.md` | `AGENT_RULES` | 76 | 56 | 0 | 20 | 73.7% |
| `.agents/rules/42_quality_of_life_and_roadmap_ledger.md` | `AGENT_RULES` | 120 | 90 | 0 | 30 | 75.0% |
| `.agents/rules/43_no_ai_jargon_and_buzzwords.md` | `AGENT_RULES` | 51 | 36 | 0 | 15 | 70.6% |
| `.agents/rules/44_anti_ai_ui_design_standards.md` | `AGENT_RULES` | 75 | 55 | 0 | 20 | 73.3% |
| `.agents/rules/CONTEXT_INDEX.json` | `AGENT_RULES` | 341 | 341 | 0 | 0 | 100.0% |
| `.agents/rules/README.md` | `AGENT_RULES` | 61 | 57 | 0 | 4 | 93.4% |
| `.mcp.json` | `OTHER` | 9 | 9 | 0 | 0 | 100.0% |
| `AGENTS.md` | `OTHER` | 19 | 16 | 0 | 3 | 84.2% |
| `AGENT_DECISION_TREE.json` | `OTHER` | 287 | 287 | 0 | 0 | 100.0% |
| `AGENT_KNOWLEDGE_GRAPH.json` | `OTHER` | 21,511 | 21,511 | 0 | 0 | 100.0% |
| `AGENT_MANIFEST.json` | `OTHER` | 307 | 307 | 0 | 0 | 100.0% |
| `CHANGELOG.md` | `OTHER` | 81 | 65 | 0 | 16 | 80.2% |
| `DESIGN.md` | `OTHER` | 195 | 151 | 0 | 44 | 77.4% |
| `README.md` | `OTHER` | 43 | 30 | 0 | 13 | 69.8% |
| `diagram.json` | `OTHER` | 158 | 155 | 0 | 3 | 98.1% |
| `docs/archive/changelog-history-01.md` | `DOCUMENTATION` | 53 | 46 | 0 | 7 | 86.8% |
| `docs/archive/changelog-history-02.md` | `DOCUMENTATION` | 86 | 74 | 0 | 12 | 86.0% |
| `docs/archive/changelog-history-03.md` | `DOCUMENTATION` | 91 | 71 | 0 | 20 | 78.0% |
| `docs/archive/changelog-history-04.md` | `DOCUMENTATION` | 76 | 68 | 0 | 8 | 89.5% |
| `docs/archive/changelog-history-05.md` | `DOCUMENTATION` | 31 | 28 | 0 | 3 | 90.3% |
| `docs/archive/changelog-history-06.md` | `DOCUMENTATION` | 55 | 51 | 0 | 4 | 92.7% |
| `docs/archive/changelog-history-07.md` | `DOCUMENTATION` | 105 | 95 | 0 | 10 | 90.5% |
| `docs/archive/changelog-history-08.md` | `DOCUMENTATION` | 104 | 96 | 0 | 8 | 92.3% |
| `docs/archive/changelog-history-09.md` | `DOCUMENTATION` | 124 | 112 | 0 | 12 | 90.3% |
| `docs/archive/changelog-history-10.md` | `DOCUMENTATION` | 99 | 88 | 0 | 11 | 88.9% |
| `docs/archive/changelog-history-11.md` | `DOCUMENTATION` | 75 | 63 | 0 | 12 | 84.0% |
| `docs/archive/changelog-history-12.md` | `DOCUMENTATION` | 44 | 35 | 0 | 9 | 79.5% |
| `docs/archive/changelog-history.md` | `DOCUMENTATION` | 16 | 14 | 0 | 2 | 87.5% |
| `docs/archive/known-issues-history-01.md` | `DOCUMENTATION` | 64 | 49 | 0 | 15 | 76.6% |
| `docs/archive/known-issues-history-02.md` | `DOCUMENTATION` | 53 | 44 | 0 | 9 | 83.0% |
| `docs/archive/known-issues-history-03.md` | `DOCUMENTATION` | 38 | 32 | 0 | 6 | 84.2% |
| `docs/archive/known-issues-history-04.md` | `DOCUMENTATION` | 26 | 24 | 0 | 2 | 92.3% |
| `docs/archive/known-issues-history-05.md` | `DOCUMENTATION` | 50 | 48 | 0 | 2 | 96.0% |
| `docs/archive/known-issues-history-06.md` | `DOCUMENTATION` | 119 | 97 | 0 | 22 | 81.5% |
| `docs/archive/known-issues-history-07.md` | `DOCUMENTATION` | 94 | 77 | 0 | 17 | 81.9% |
| `docs/archive/known-issues-history-08.md` | `DOCUMENTATION` | 90 | 75 | 0 | 15 | 83.3% |
| `docs/archive/known-issues-history-09.md` | `DOCUMENTATION` | 12 | 11 | 0 | 1 | 91.7% |
| `docs/archive/known-issues-history.md` | `DOCUMENTATION` | 13 | 11 | 0 | 2 | 84.6% |
| `docs/archive/performance-report-2026-11-03.md` | `DOCUMENTATION` | 5 | 3 | 0 | 2 | 60.0% |
| `docs/consolidated-rules.md` | `DOCUMENTATION` | 53 | 49 | 0 | 4 | 92.5% |
| `docs/consolidated-rules/rule-00_hard_stops-01.md` | `DOCUMENTATION` | 10 | 8 | 0 | 2 | 80.0% |
| `docs/consolidated-rules/rule-01_hardware-01.md` | `DOCUMENTATION` | 67 | 57 | 0 | 10 | 85.1% |
| `docs/consolidated-rules/rule-02_architecture-01.md` | `DOCUMENTATION` | 24 | 17 | 0 | 7 | 70.8% |
| `docs/consolidated-rules/rule-03_conventions-01.md` | `DOCUMENTATION` | 67 | 55 | 0 | 12 | 82.1% |
| `docs/consolidated-rules/rule-04_known_issues-01.md` | `DOCUMENTATION` | 72 | 63 | 0 | 9 | 87.5% |
| `docs/consolidated-rules/rule-04_known_issues-02.md` | `DOCUMENTATION` | 13 | 10 | 0 | 3 | 76.9% |
| `docs/consolidated-rules/rule-05_git_workflow-01.md` | `DOCUMENTATION` | 39 | 31 | 0 | 8 | 79.5% |
| `docs/consolidated-rules/rule-06_verification_standards-01.md` | `DOCUMENTATION` | 121 | 102 | 0 | 19 | 84.3% |
| `docs/consolidated-rules/rule-07_task_protocol-01.md` | `DOCUMENTATION` | 65 | 58 | 0 | 7 | 89.2% |
| `docs/consolidated-rules/rule-08_ui_style_guide-01.md` | `DOCUMENTATION` | 57 | 49 | 0 | 8 | 86.0% |
| `docs/consolidated-rules/rule-09_testing_infrastructure-01.md` | `DOCUMENTATION` | 24 | 19 | 0 | 5 | 79.2% |
| `docs/consolidated-rules/rule-10_symbol_reference-01.md` | `DOCUMENTATION` | 25 | 16 | 0 | 9 | 64.0% |
| `docs/consolidated-rules/rule-11_rules_meta-01.md` | `DOCUMENTATION` | 26 | 23 | 0 | 3 | 88.5% |
| `docs/consolidated-rules/rule-12_extensibility_contract-01.md` | `DOCUMENTATION` | 25 | 19 | 0 | 6 | 76.0% |
| `docs/consolidated-rules/rule-13_code_style-01.md` | `DOCUMENTATION` | 31 | 23 | 0 | 8 | 74.2% |
| `docs/consolidated-rules/rule-14_error_handling_and_fault_isolation-01.md` | `DOCUMENTATION` | 29 | 22 | 0 | 7 | 75.9% |
| `docs/consolidated-rules/rule-15_performance_budgets-01.md` | `DOCUMENTATION` | 23 | 12 | 0 | 11 | 52.2% |
| `docs/consolidated-rules/rule-16_logging_and_diagnostics-01.md` | `DOCUMENTATION` | 24 | 17 | 0 | 7 | 70.8% |
| `docs/consolidated-rules/rule-17_release_and_versioning-01.md` | `DOCUMENTATION` | 29 | 22 | 0 | 7 | 75.9% |
| `docs/consolidated-rules/rule-18_dependency_and_vendor_sync-01.md` | `DOCUMENTATION` | 38 | 30 | 0 | 8 | 78.9% |
| `docs/consolidated-rules/rule-19_security_and_data_integrity-01.md` | `DOCUMENTATION` | 27 | 14 | 0 | 13 | 51.9% |
| `docs/consolidated-rules/rule-20_multi_agent_protocol-01.md` | `DOCUMENTATION` | 23 | 17 | 0 | 6 | 73.9% |
| `docs/consolidated-rules/rule-21_documentation_standards-01.md` | `DOCUMENTATION` | 23 | 17 | 0 | 6 | 73.9% |
| `docs/consolidated-rules/rule-22_review_checklist-01.md` | `DOCUMENTATION` | 18 | 14 | 0 | 4 | 77.8% |
| `docs/consolidated-rules/rule-23_incident_postmortem_log-01.md` | `DOCUMENTATION` | 39 | 31 | 0 | 8 | 79.5% |
| `docs/consolidated-rules/rule-24_vendor_flag_safety-01.md` | `DOCUMENTATION` | 82 | 70 | 0 | 12 | 85.4% |
| `docs/consolidated-rules/rule-25_game_compatibility_ledger-01.md` | `DOCUMENTATION` | 56 | 46 | 0 | 10 | 82.1% |
| `docs/consolidated-rules/rule-26_emulator_exit_contract-01.md` | `DOCUMENTATION` | 22 | 13 | 0 | 9 | 59.1% |
| `docs/consolidated-rules/rule-27_codebase_map-01.md` | `DOCUMENTATION` | 74 | 56 | 0 | 18 | 75.7% |
| `docs/consolidated-rules/rule-28_display_and_spi_contract-01.md` | `DOCUMENTATION` | 29 | 19 | 0 | 10 | 65.5% |
| `docs/consolidated-rules/rule-29_adding_a_baked_rom-01.md` | `DOCUMENTATION` | 84 | 69 | 0 | 15 | 82.1% |
| `docs/consolidated-rules/rule-30_common_agent_mistakes-01.md` | `DOCUMENTATION` | 173 | 131 | 0 | 42 | 75.7% |
| `docs/consolidated-rules/rule-31_quick_start_primer-01.md` | `DOCUMENTATION` | 37 | 23 | 0 | 14 | 62.2% |
| `docs/consolidated-rules/rule-32_modular_core_template-01.md` | `DOCUMENTATION` | 28 | 21 | 0 | 7 | 75.0% |
| `docs/consolidated-rules/rule-33_agent_handoff_and_optimization_cycle-01.md` | `DOCUMENTATION` | 63 | 46 | 0 | 17 | 73.0% |
| `docs/consolidated-rules/rule-34_ai_agent_sandbox_and_guardrails-01.md` | `DOCUMENTATION` | 55 | 39 | 0 | 16 | 70.9% |
| `docs/consolidated-rules/rule-35_bmo_face_contract-01.md` | `DOCUMENTATION` | 75 | 66 | 0 | 9 | 88.0% |
| `docs/consolidated-rules/rule-36_bug_intake_protocol-01.md` | `DOCUMENTATION` | 72 | 62 | 0 | 10 | 86.1% |
| `docs/consolidated-rules/rule-37_rom_governance_and_flash_budget-01.md` | `DOCUMENTATION` | 63 | 55 | 0 | 8 | 87.3% |
| `docs/consolidated-rules/rule-38_mandatory_documentation_update-01.md` | `DOCUMENTATION` | 100 | 76 | 0 | 24 | 76.0% |
| `docs/consolidated-rules/rule-39_performance_and_benchmark_framework-01.md` | `DOCUMENTATION` | 74 | 53 | 0 | 21 | 71.6% |
| `docs/consolidated-rules/rule-40_ai_knowledge_graph_and_indexing_protocol-01.md` | `DOCUMENTATION` | 80 | 61 | 0 | 19 | 76.2% |
| `docs/consolidated-rules/rule-41_engineering_communication_and_critical_pushback-01.md` | `DOCUMENTATION` | 80 | 58 | 0 | 22 | 72.5% |
| `docs/consolidated-rules/rule-42_quality_of_life_and_roadmap_ledger-01.md` | `DOCUMENTATION` | 84 | 62 | 0 | 22 | 73.8% |
| `docs/consolidated-rules/rule-42_quality_of_life_and_roadmap_ledger-02.md` | `DOCUMENTATION` | 43 | 32 | 0 | 11 | 74.4% |
| `docs/consolidated-rules/rule-43_no_ai_jargon_and_buzzwords-01.md` | `DOCUMENTATION` | 55 | 38 | 0 | 17 | 69.1% |
| `docs/consolidated-rules/rule-44_anti_ai_ui_design_standards-01.md` | `DOCUMENTATION` | 79 | 57 | 0 | 22 | 72.2% |
| `docs/decisions/0001-hardware-configuration.md` | `DOCUMENTATION` | 17 | 10 | 0 | 7 | 58.8% |
| `docs/decisions/0002-shared-spi-bus.md` | `DOCUMENTATION` | 17 | 10 | 0 | 7 | 58.8% |
| `docs/decisions/0003-rom-keys-and-index-v2.md` | `DOCUMENTATION` | 17 | 10 | 0 | 7 | 58.8% |
| `docs/decisions/0004-battery-persistence.md` | `DOCUMENTATION` | 17 | 10 | 0 | 7 | 58.8% |
| `docs/decisions/0005-psram-and-doom-budget.md` | `DOCUMENTATION` | 17 | 10 | 0 | 7 | 58.8% |
| `docs/decisions/0006-portable-power.md` | `DOCUMENTATION` | 17 | 10 | 0 | 7 | 58.8% |
| `docs/decisions/0007-fram-wiring.md` | `DOCUMENTATION` | 17 | 10 | 0 | 7 | 58.8% |
| `docs/decisions/0008-rom-policy-and-licensing.md` | `DOCUMENTATION` | 17 | 10 | 0 | 7 | 58.8% |
| `docs/decisions/0009-emulator-exit-confirmation.md` | `DOCUMENTATION` | 18 | 11 | 0 | 7 | 61.1% |
| `docs/decisions/0010-simulator-boundary.md` | `DOCUMENTATION` | 17 | 10 | 0 | 7 | 58.8% |
| `docs/decisions/0011-index-path-storage.md` | `DOCUMENTATION` | 17 | 14 | 0 | 3 | 82.4% |
| `docs/decisions/0012-save-state-schema.md` | `DOCUMENTATION` | 9 | 8 | 0 | 1 | 88.9% |
| `docs/decisions/0013-partition-layout.md` | `DOCUMENTATION` | 8 | 7 | 0 | 1 | 87.5% |
| `docs/decisions/0014-display-spi-clock.md` | `DOCUMENTATION` | 11 | 10 | 0 | 1 | 90.9% |
| `docs/decisions/README.md` | `DOCUMENTATION` | 22 | 19 | 0 | 3 | 86.4% |
| `docs/enclosure-3d-printing.md` | `DOCUMENTATION` | 99 | 67 | 0 | 32 | 67.7% |
| `docs/face-render-measurement-template.md` | `DOCUMENTATION` | 19 | 18 | 0 | 1 | 94.7% |
| `docs/generated/symbols.md` | `DOCUMENTATION` | 17 | 14 | 0 | 3 | 82.4% |
| `docs/generated/symbols/core-01.md` | `DOCUMENTATION` | 262 | 227 | 0 | 35 | 86.6% |
| `docs/generated/symbols/core-02.md` | `DOCUMENTATION` | 80 | 71 | 0 | 9 | 88.8% |
| `docs/generated/symbols/emulators-01.md` | `DOCUMENTATION` | 172 | 141 | 0 | 31 | 82.0% |
| `docs/generated/symbols/engine-01.md` | `DOCUMENTATION` | 212 | 209 | 0 | 3 | 98.6% |
| `docs/generated/symbols/engine-02.md` | `DOCUMENTATION` | 225 | 222 | 0 | 3 | 98.7% |
| `docs/generated/symbols/engine-03.md` | `DOCUMENTATION` | 249 | 246 | 0 | 3 | 98.8% |
| `docs/generated/symbols/engine-04.md` | `DOCUMENTATION` | 209 | 206 | 0 | 3 | 98.6% |
| `docs/generated/symbols/engine-05.md` | `DOCUMENTATION` | 195 | 192 | 0 | 3 | 98.5% |
| `docs/generated/symbols/engine-06.md` | `DOCUMENTATION` | 189 | 186 | 0 | 3 | 98.4% |
| `docs/generated/symbols/engine-07.md` | `DOCUMENTATION` | 196 | 193 | 0 | 3 | 98.5% |
| `docs/generated/symbols/engine-08.md` | `DOCUMENTATION` | 117 | 114 | 0 | 3 | 97.4% |
| `docs/handoffs/2026-11-07-remediation.md` | `DOCUMENTATION` | 112 | 92 | 0 | 20 | 82.1% |
| `docs/handoffs/2026-11-12-logical-performance-audit.md` | `DOCUMENTATION` | 37 | 27 | 0 | 10 | 73.0% |
| `docs/handoffs/2026-11-12-production-readiness.md` | `DOCUMENTATION` | 151 | 121 | 0 | 30 | 80.1% |
| `docs/hardware-measurement-template.json` | `DOCUMENTATION` | 18 | 18 | 0 | 0 | 100.0% |
| `docs/hardware-measurement-template.md` | `DOCUMENTATION` | 14 | 13 | 0 | 1 | 92.9% |
| `docs/hardware-notes.md` | `DOCUMENTATION` | 51 | 31 | 0 | 20 | 60.8% |
| `docs/hardware_measurements.md` | `DOCUMENTATION` | 34 | 22 | 0 | 12 | 64.7% |
| `docs/performance_and_benchmarking.md` | `DOCUMENTATION` | 28 | 16 | 0 | 12 | 57.1% |
| `docs/performance_report.md` | `DOCUMENTATION` | 760 | 735 | 0 | 25 | 96.7% |
| `docs/readme/details-01.md` | `DOCUMENTATION` | 93 | 62 | 0 | 31 | 66.7% |
| `docs/readme/details-02.md` | `DOCUMENTATION` | 61 | 44 | 0 | 17 | 72.1% |
| `docs/research_embedded_ui_frameworks_unevaluated.md` | `DOCUMENTATION` | 57 | 48 | 0 | 9 | 84.2% |
| `docs/software-design-document.md` | `DOCUMENTATION` | 68 | 52 | 0 | 16 | 76.5% |
| `docs/software-design-document/section-01.md` | `DOCUMENTATION` | 64 | 47 | 0 | 17 | 73.4% |
| `docs/software-design-document/section-02.md` | `DOCUMENTATION` | 48 | 29 | 0 | 19 | 60.4% |
| `docs/software-design-document/section-03.md` | `DOCUMENTATION` | 63 | 48 | 0 | 15 | 76.2% |
| `docs/software-design-document/section-04.md` | `DOCUMENTATION` | 59 | 46 | 0 | 13 | 78.0% |
| `docs/software-design-document/section-05-part-01.md` | `DOCUMENTATION` | 69 | 52 | 0 | 17 | 75.4% |
| `docs/software-design-document/section-05-part-02.md` | `DOCUMENTATION` | 35 | 24 | 0 | 11 | 68.6% |
| `docs/software-design-document/section-05-part-03.md` | `DOCUMENTATION` | 53 | 36 | 0 | 17 | 67.9% |
| `docs/software-design-document/section-06.md` | `DOCUMENTATION` | 53 | 36 | 0 | 17 | 67.9% |
| `docs/software-design-document/section-07.md` | `DOCUMENTATION` | 68 | 51 | 0 | 17 | 75.0% |
| `docs/software-design-document/section-08.md` | `DOCUMENTATION` | 27 | 14 | 0 | 13 | 51.9% |
| `docs/software-design-document/section-09-part-01.md` | `DOCUMENTATION` | 37 | 27 | 0 | 10 | 73.0% |
| `docs/software-design-document/section-09-part-02.md` | `DOCUMENTATION` | 29 | 18 | 0 | 11 | 62.1% |
| `docs/software-design-document/section-09-part-03.md` | `DOCUMENTATION` | 13 | 7 | 0 | 6 | 53.8% |
| `docs/software-design-document/section-10.md` | `DOCUMENTATION` | 53 | 33 | 0 | 20 | 62.3% |
| `docs/software-design-document/section-11.md` | `DOCUMENTATION` | 19 | 10 | 0 | 9 | 52.6% |
| `docs/software-design-document/section-12.md` | `DOCUMENTATION` | 23 | 15 | 0 | 8 | 65.2% |
| `docs/software-design-document/section-13.md` | `DOCUMENTATION` | 21 | 11 | 0 | 10 | 52.4% |
| `docs/software-design-document/section-14.md` | `DOCUMENTATION` | 31 | 21 | 0 | 10 | 67.7% |
| `docs/software-design-document/section-15.md` | `DOCUMENTATION` | 34 | 22 | 0 | 12 | 64.7% |
| `docs/software-design-document/section-16.md` | `DOCUMENTATION` | 17 | 9 | 0 | 8 | 52.9% |
| `docs/software-design-document/section-17.md` | `DOCUMENTATION` | 25 | 17 | 0 | 8 | 68.0% |
| `docs/wiring/01-display-and-buttons.md` | `DOCUMENTATION` | 51 | 37 | 0 | 14 | 72.5% |
| `docs/wiring/02-sd-card.md` | `DOCUMENTATION` | 33 | 23 | 0 | 10 | 69.7% |
| `firmware/BmoGameboy/BmoGameboy.ino` | `FIRMWARE_TOP` | 1,349 | 1,180 | 67 | 102 | 87.5% |
| `firmware/BmoGameboy/partitions.csv` | `FIRMWARE_TOP` | 5 | 5 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/src/assets/bmo_assets.h` | `BAKED_ROMS` | 17 | 13 | 2 | 2 | 76.5% |
| `firmware/BmoGameboy/src/assets/roms/aladdin.h` | `BAKED_ROMS` | 65,542 | 65,542 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/src/assets/roms/lego_racers.h` | `BAKED_ROMS` | 65,542 | 65,542 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/src/assets/roms/mario_deluxe.h` | `BAKED_ROMS` | 65,542 | 65,542 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/src/assets/roms/zelda_ages.h` | `BAKED_ROMS` | 65,542 | 65,542 | 0 | 0 | 100.0% |
| `firmware/BmoGameboy/src/core/audio_i2s.cpp` | `FIRMWARE_CORE` | 63 | 51 | 2 | 10 | 81.0% |
| `firmware/BmoGameboy/src/core/audio_i2s.h` | `FIRMWARE_CORE` | 9 | 7 | 1 | 1 | 77.8% |
| `firmware/BmoGameboy/src/core/battery.cpp` | `FIRMWARE_CORE` | 84 | 72 | 2 | 10 | 85.7% |
| `firmware/BmoGameboy/src/core/battery.h` | `FIRMWARE_CORE` | 21 | 10 | 5 | 6 | 47.6% |
| `firmware/BmoGameboy/src/core/battery_save.h` | `FIRMWARE_CORE` | 48 | 43 | 0 | 5 | 89.6% |
| `firmware/BmoGameboy/src/core/bmo_face.cpp` | `FIRMWARE_CORE` | 1,149 | 865 | 139 | 145 | 75.3% |
| `firmware/BmoGameboy/src/core/bmo_face.h` | `FIRMWARE_CORE` | 124 | 58 | 46 | 20 | 46.8% |
| `firmware/BmoGameboy/src/core/box_art.cpp` | `FIRMWARE_CORE` | 368 | 313 | 9 | 46 | 85.1% |
| `firmware/BmoGameboy/src/core/box_art.h` | `FIRMWARE_CORE` | 37 | 23 | 7 | 7 | 62.2% |
| `firmware/BmoGameboy/src/core/buttons.cpp` | `FIRMWARE_CORE` | 134 | 104 | 10 | 20 | 77.6% |
| `firmware/BmoGameboy/src/core/buttons.h` | `FIRMWARE_CORE` | 48 | 23 | 15 | 10 | 47.9% |
| `firmware/BmoGameboy/src/core/config.h` | `FIRMWARE_CORE` | 129 | 75 | 37 | 17 | 58.1% |
| `firmware/BmoGameboy/src/core/display_emu.cpp` | `FIRMWARE_CORE` | 2,116 | 1,731 | 126 | 259 | 81.8% |
| `firmware/BmoGameboy/src/core/display_emu.h` | `FIRMWARE_CORE` | 190 | 74 | 70 | 46 | 38.9% |
| `firmware/BmoGameboy/src/core/fram_save.cpp` | `FIRMWARE_CORE` | 74 | 55 | 5 | 14 | 74.3% |
| `firmware/BmoGameboy/src/core/fram_save.h` | `FIRMWARE_CORE` | 13 | 8 | 2 | 3 | 61.5% |
| `firmware/BmoGameboy/src/core/frame_pacing.h` | `FIRMWARE_CORE` | 26 | 17 | 6 | 3 | 65.4% |
| `firmware/BmoGameboy/src/core/frame_percentiles.h` | `FIRMWARE_CORE` | 34 | 27 | 0 | 7 | 79.4% |
| `firmware/BmoGameboy/src/core/harmonic_spring.h` | `FIRMWARE_CORE` | 59 | 51 | 0 | 8 | 86.4% |
| `firmware/BmoGameboy/src/core/path_key.h` | `FIRMWARE_CORE` | 79 | 71 | 0 | 8 | 89.9% |
| `firmware/BmoGameboy/src/core/pause_quit_policy.h` | `FIRMWARE_CORE` | 73 | 60 | 0 | 13 | 82.2% |
| `firmware/BmoGameboy/src/core/profiler.cpp` | `FIRMWARE_CORE` | 102 | 82 | 1 | 19 | 80.4% |
| `firmware/BmoGameboy/src/core/profiler.h` | `FIRMWARE_CORE` | 64 | 40 | 11 | 13 | 62.5% |
| `firmware/BmoGameboy/src/core/rom_file_filter.h` | `FIRMWARE_CORE` | 72 | 63 | 0 | 9 | 87.5% |
| `firmware/BmoGameboy/src/core/rom_index.h` | `FIRMWARE_CORE` | 64 | 54 | 2 | 8 | 84.4% |
| `firmware/BmoGameboy/src/core/save_manager.cpp` | `FIRMWARE_CORE` | 767 | 659 | 12 | 96 | 85.9% |
| `firmware/BmoGameboy/src/core/save_manager.h` | `FIRMWARE_CORE` | 73 | 54 | 7 | 12 | 74.0% |
| `firmware/BmoGameboy/src/core/sd_card.cpp` | `FIRMWARE_CORE` | 1,250 | 1,121 | 23 | 106 | 89.7% |
| `firmware/BmoGameboy/src/core/sd_card.h` | `FIRMWARE_CORE` | 94 | 74 | 8 | 12 | 78.7% |
| `firmware/BmoGameboy/src/core/spi_arbiter.cpp` | `FIRMWARE_CORE` | 25 | 21 | 0 | 4 | 84.0% |
| `firmware/BmoGameboy/src/core/spi_arbiter.h` | `FIRMWARE_CORE` | 24 | 6 | 14 | 4 | 25.0% |
| `firmware/BmoGameboy/src/core/theme.h` | `FIRMWARE_CORE` | 132 | 88 | 26 | 18 | 66.7% |
| `firmware/BmoGameboy/src/emulators/emu_atari.cpp` | `EMULATOR_WRAPPERS` | 96 | 80 | 1 | 15 | 83.3% |
| `firmware/BmoGameboy/src/emulators/emu_atari.h` | `EMULATOR_WRAPPERS` | 17 | 14 | 0 | 3 | 82.4% |
| `firmware/BmoGameboy/src/emulators/emu_colem.cpp` | `EMULATOR_WRAPPERS` | 126 | 113 | 0 | 13 | 89.7% |
| `firmware/BmoGameboy/src/emulators/emu_colem.h` | `EMULATOR_WRAPPERS` | 16 | 14 | 0 | 2 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_doom.cpp` | `EMULATOR_WRAPPERS` | 313 | 244 | 28 | 41 | 78.0% |
| `firmware/BmoGameboy/src/emulators/emu_doom.h` | `EMULATOR_WRAPPERS` | 19 | 10 | 5 | 4 | 52.6% |
| `firmware/BmoGameboy/src/emulators/emu_genesis.cpp` | `EMULATOR_WRAPPERS` | 138 | 125 | 0 | 13 | 90.6% |
| `firmware/BmoGameboy/src/emulators/emu_genesis.h` | `EMULATOR_WRAPPERS` | 16 | 14 | 0 | 2 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_lynx.cpp` | `EMULATOR_WRAPPERS` | 104 | 91 | 0 | 13 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_lynx.h` | `EMULATOR_WRAPPERS` | 16 | 14 | 0 | 2 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_nes.cpp` | `EMULATOR_WRAPPERS` | 126 | 104 | 5 | 17 | 82.5% |
| `firmware/BmoGameboy/src/emulators/emu_nes.h` | `EMULATOR_WRAPPERS` | 18 | 14 | 1 | 3 | 77.8% |
| `firmware/BmoGameboy/src/emulators/emu_ngp.cpp` | `EMULATOR_WRAPPERS` | 104 | 91 | 0 | 13 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_ngp.h` | `EMULATOR_WRAPPERS` | 16 | 14 | 0 | 2 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_pce.cpp` | `EMULATOR_WRAPPERS` | 103 | 86 | 1 | 16 | 83.5% |
| `firmware/BmoGameboy/src/emulators/emu_pce.h` | `EMULATOR_WRAPPERS` | 19 | 14 | 1 | 4 | 73.7% |
| `firmware/BmoGameboy/src/emulators/emu_peanut.cpp` | `EMULATOR_WRAPPERS` | 237 | 176 | 24 | 37 | 74.3% |
| `firmware/BmoGameboy/src/emulators/emu_peanut.h` | `EMULATOR_WRAPPERS` | 22 | 12 | 5 | 5 | 54.5% |
| `firmware/BmoGameboy/src/emulators/emu_pico.cpp` | `EMULATOR_WRAPPERS` | 101 | 85 | 1 | 15 | 84.2% |
| `firmware/BmoGameboy/src/emulators/emu_pico.h` | `EMULATOR_WRAPPERS` | 17 | 14 | 0 | 3 | 82.4% |
| `firmware/BmoGameboy/src/emulators/emu_sms.cpp` | `EMULATOR_WRAPPERS` | 108 | 88 | 5 | 15 | 81.5% |
| `firmware/BmoGameboy/src/emulators/emu_sms.h` | `EMULATOR_WRAPPERS` | 18 | 14 | 1 | 3 | 77.8% |
| `firmware/BmoGameboy/src/emulators/emu_snes.cpp` | `EMULATOR_WRAPPERS` | 138 | 125 | 0 | 13 | 90.6% |
| `firmware/BmoGameboy/src/emulators/emu_snes.h` | `EMULATOR_WRAPPERS` | 16 | 14 | 0 | 2 | 87.5% |
| `firmware/BmoGameboy/src/emulators/emu_walnut.cpp` | `EMULATOR_WRAPPERS` | 297 | 197 | 58 | 42 | 66.3% |
| `firmware/BmoGameboy/src/emulators/emu_walnut.h` | `EMULATOR_WRAPPERS` | 22 | 12 | 5 | 5 | 54.5% |
| `firmware/BmoGameboy/src/emulators/emu_wswan.cpp` | `EMULATOR_WRAPPERS` | 121 | 108 | 0 | 13 | 89.3% |
| `firmware/BmoGameboy/src/emulators/emu_wswan.h` | `EMULATOR_WRAPPERS` | 16 | 14 | 0 | 2 | 87.5% |
| `firmware/BmoGameboy/src/emulators/psram_zone_budget.h` | `EMULATOR_WRAPPERS` | 20 | 17 | 0 | 3 | 85.0% |
| `firmware/BmoGameboy/src/engine/walnut_cgb/walnut_cgb.h` | `ENGINE_GBC` | 9,983 | 7,751 | 889 | 1,343 | 77.6% |
| `firmware/BmoGameboy/src/tests/unit_tests.cpp` | `FIRMWARE_TOP` | 169 | 120 | 21 | 28 | 71.0% |
| `firmware/BmoGameboy/src/tests/unit_tests.h` | `FIRMWARE_TOP` | 5 | 2 | 2 | 1 | 40.0% |
| `firmware/BmoGameboy/src/vendor/agnes/agnes.c` | `VENDOR_ENGINES` | 2,640 | 2,225 | 96 | 319 | 84.3% |
| `firmware/BmoGameboy/src/vendor/agnes/agnes.h` | `VENDOR_ENGINES` | 88 | 51 | 21 | 16 | 58.0% |
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
| `firmware/BmoGameboy/src/vendor/doom/src/d_iwad.c` | `VENDOR_ENGINES` | 858 | 519 | 133 | 206 | 60.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_iwad.h` | `VENDOR_ENGINES` | 52 | 28 | 16 | 8 | 53.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_loop.c` | `VENDOR_ENGINES` | 826 | 464 | 169 | 193 | 56.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_loop.h` | `VENDOR_ENGINES` | 81 | 23 | 34 | 24 | 28.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/d_main.c` | `VENDOR_ENGINES` | 1,847 | 1,092 | 418 | 337 | 59.1% |
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
| `firmware/BmoGameboy/src/vendor/doom/src/f_finale.c` | `VENDOR_ENGINES` | 730 | 540 | 77 | 113 | 74.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/f_finale.h` | `VENDOR_ENGINES` | 45 | 9 | 23 | 13 | 20.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/f_wipe.c` | `VENDOR_ENGINES` | 294 | 220 | 31 | 43 | 74.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/f_wipe.h` | `VENDOR_ENGINES` | 63 | 29 | 22 | 12 | 46.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/g_game.c` | `VENDOR_ENGINES` | 2,303 | 1,572 | 317 | 414 | 68.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/g_game.h` | `VENDOR_ENGINES` | 80 | 29 | 28 | 23 | 36.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/gusconf.c` | `VENDOR_ENGINES` | 270 | 190 | 24 | 56 | 70.4% |
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
| `firmware/BmoGameboy/src/vendor/doom/src/i_system.c` | `VENDOR_ENGINES` | 622 | 409 | 103 | 110 | 65.8% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_system.h` | `VENDOR_ENGINES` | 84 | 19 | 37 | 28 | 22.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_timer.c` | `VENDOR_ENGINES` | 96 | 36 | 32 | 28 | 37.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_timer.h` | `VENDOR_ENGINES` | 42 | 9 | 23 | 10 | 21.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_video.c` | `VENDOR_ENGINES` | 495 | 309 | 74 | 112 | 62.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/i_video.h` | `VENDOR_ENGINES` | 175 | 63 | 65 | 47 | 36.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/icon.c` | `VENDOR_ENGINES` | 262 | 260 | 0 | 2 | 99.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/info.c` | `VENDOR_ENGINES` | 4,662 | 4,492 | 21 | 149 | 96.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/info.h` | `VENDOR_ENGINES` | 1,331 | 1,296 | 21 | 14 | 97.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_argv.c` | `VENDOR_ENGINES` | 263 | 148 | 52 | 63 | 56.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_argv.h` | `VENDOR_ENGINES` | 49 | 11 | 26 | 12 | 22.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_bbox.c` | `VENDOR_ENGINES` | 54 | 21 | 20 | 13 | 38.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_bbox.h` | `VENDOR_ENGINES` | 47 | 18 | 19 | 10 | 38.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_cheat.c` | `VENDOR_ENGINES` | 89 | 38 | 32 | 19 | 42.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_cheat.h` | `VENDOR_ENGINES` | 62 | 24 | 23 | 15 | 38.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_config.c` | `VENDOR_ENGINES` | 2,137 | 625 | 948 | 564 | 29.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_config.h` | `VENDOR_ENGINES` | 39 | 16 | 17 | 6 | 41.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_controls.c` | `VENDOR_ENGINES` | 398 | 269 | 57 | 72 | 67.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_controls.h` | `VENDOR_ENGINES` | 168 | 121 | 17 | 30 | 72.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_fixed.c` | `VENDOR_ENGINES` | 62 | 24 | 21 | 17 | 38.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_fixed.h` | `VENDOR_ENGINES` | 39 | 8 | 20 | 11 | 20.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_menu.c` | `VENDOR_ENGINES` | 2,125 | 1,502 | 250 | 373 | 70.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_menu.h` | `VENDOR_ENGINES` | 61 | 11 | 33 | 17 | 18.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_misc.c` | `VENDOR_ENGINES` | 599 | 399 | 85 | 115 | 66.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_misc.h` | `VENDOR_ENGINES` | 51 | 27 | 17 | 7 | 52.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_random.c` | `VENDOR_ENGINES` | 65 | 37 | 22 | 6 | 56.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/m_random.h` | `VENDOR_ENGINES` | 39 | 7 | 21 | 11 | 17.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/memio.c` | `VENDOR_ENGINES` | 210 | 136 | 27 | 47 | 64.8% |
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
| `firmware/BmoGameboy/src/vendor/doom/src/p_doors.c` | `VENDOR_ENGINES` | 776 | 569 | 83 | 124 | 73.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_enemy.c` | `VENDOR_ENGINES` | 2,009 | 1,344 | 238 | 427 | 66.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_floor.c` | `VENDOR_ENGINES` | 546 | 429 | 54 | 63 | 78.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_inter.c` | `VENDOR_ENGINES` | 924 | 646 | 115 | 163 | 69.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_inter.h` | `VENDOR_ENGINES` | 30 | 4 | 17 | 9 | 13.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_lights.c` | `VENDOR_ENGINES` | 350 | 201 | 72 | 77 | 57.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_local.h` | `VENDOR_ENGINES` | 297 | 155 | 66 | 76 | 52.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_map.c` | `VENDOR_ENGINES` | 1,448 | 828 | 304 | 316 | 57.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_maputl.c` | `VENDOR_ENGINES` | 1,001 | 651 | 161 | 189 | 65.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_mobj.c` | `VENDOR_ENGINES` | 1,061 | 664 | 186 | 211 | 62.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_mobj.h` | `VENDOR_ENGINES` | 284 | 77 | 161 | 46 | 27.1% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_plats.c` | `VENDOR_ENGINES` | 304 | 218 | 29 | 57 | 71.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_pspr.c` | `VENDOR_ENGINES` | 888 | 537 | 173 | 178 | 60.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_pspr.h` | `VENDOR_ENGINES` | 71 | 21 | 35 | 15 | 29.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_saveg.c` | `VENDOR_ENGINES` | 1,890 | 975 | 435 | 480 | 51.6% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_saveg.h` | `VENDOR_ENGINES` | 62 | 21 | 24 | 17 | 33.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/p_setup.c` | `VENDOR_ENGINES` | 854 | 547 | 134 | 173 | 64.1% |
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
| `firmware/BmoGameboy/src/vendor/doom/src/r_things.c` | `VENDOR_ENGINES` | 984 | 608 | 171 | 205 | 61.8% |
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
| `firmware/BmoGameboy/src/vendor/doom/src/w_checksum.c` | `VENDOR_ENGINES` | 94 | 54 | 22 | 18 | 57.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_checksum.h` | `VENDOR_ENGINES` | 27 | 5 | 17 | 5 | 18.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_file.c` | `VENDOR_ENGINES` | 95 | 44 | 32 | 19 | 46.3% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_file.h` | `VENDOR_ENGINES` | 78 | 23 | 31 | 24 | 29.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_file_stdc.c` | `VENDOR_ENGINES` | 113 | 66 | 22 | 25 | 58.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_main.c` | `VENDOR_ENGINES` | 198 | 87 | 66 | 45 | 43.9% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_main.h` | `VENDOR_ENGINES` | 24 | 4 | 16 | 4 | 16.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_merge.h` | `VENDOR_ENGINES` | 44 | 9 | 23 | 12 | 20.5% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_wad.c` | `VENDOR_ENGINES` | 656 | 396 | 124 | 136 | 60.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/w_wad.h` | `VENDOR_ENGINES` | 78 | 31 | 24 | 23 | 39.7% |
| `firmware/BmoGameboy/src/vendor/doom/src/wi_stuff.c` | `VENDOR_ENGINES` | 1,829 | 1,215 | 210 | 404 | 66.4% |
| `firmware/BmoGameboy/src/vendor/doom/src/wi_stuff.h` | `VENDOR_ENGINES` | 48 | 14 | 24 | 10 | 29.2% |
| `firmware/BmoGameboy/src/vendor/doom/src/z_zone.c` | `VENDOR_ENGINES` | 500 | 290 | 89 | 121 | 58.0% |
| `firmware/BmoGameboy/src/vendor/doom/src/z_zone.h` | `VENDOR_ENGINES` | 73 | 29 | 29 | 15 | 39.7% |
| `firmware/BmoGameboy/src/vendor/genesis/genesis.c` | `VENDOR_ENGINES` | 786 | 593 | 94 | 99 | 75.4% |
| `firmware/BmoGameboy/src/vendor/genesis/genesis.h` | `VENDOR_ENGINES` | 65 | 48 | 6 | 11 | 73.8% |
| `firmware/BmoGameboy/src/vendor/lynx/lynx.c` | `VENDOR_ENGINES` | 368 | 302 | 22 | 44 | 82.1% |
| `firmware/BmoGameboy/src/vendor/lynx/lynx.h` | `VENDOR_ENGINES` | 40 | 30 | 2 | 8 | 75.0% |
| `firmware/BmoGameboy/src/vendor/ngp/ngp.c` | `VENDOR_ENGINES` | 430 | 365 | 31 | 34 | 84.9% |
| `firmware/BmoGameboy/src/vendor/ngp/ngp.h` | `VENDOR_ENGINES` | 36 | 27 | 2 | 7 | 75.0% |
| `firmware/BmoGameboy/src/vendor/pce/pce.c` | `VENDOR_ENGINES` | 658 | 497 | 88 | 73 | 75.5% |
| `firmware/BmoGameboy/src/vendor/pce/pce.h` | `VENDOR_ENGINES` | 67 | 50 | 6 | 11 | 74.6% |
| `firmware/BmoGameboy/src/vendor/peanut_gb/peanut_gb.h` | `VENDOR_ENGINES` | 4,101 | 2,906 | 568 | 627 | 70.9% |
| `firmware/BmoGameboy/src/vendor/peanut_gb/peanut_gb_config.h` | `VENDOR_ENGINES` | 8 | 3 | 4 | 1 | 37.5% |
| `firmware/BmoGameboy/src/vendor/pico/pico.c` | `VENDOR_ENGINES` | 296 | 243 | 24 | 29 | 82.1% |
| `firmware/BmoGameboy/src/vendor/pico/pico.h` | `VENDOR_ENGINES` | 39 | 31 | 1 | 7 | 79.5% |
| `firmware/BmoGameboy/src/vendor/smsplus/sms.c` | `VENDOR_ENGINES` | 375 | 325 | 22 | 28 | 86.7% |
| `firmware/BmoGameboy/src/vendor/smsplus/sms.h` | `VENDOR_ENGINES` | 56 | 45 | 3 | 8 | 80.4% |
| `firmware/BmoGameboy/src/vendor/snes/snes.c` | `VENDOR_ENGINES` | 694 | 598 | 26 | 70 | 86.2% |
| `firmware/BmoGameboy/src/vendor/snes/snes.h` | `VENDOR_ENGINES` | 57 | 46 | 3 | 8 | 80.7% |
| `firmware/BmoGameboy/src/vendor/stella/atari.c` | `VENDOR_ENGINES` | 557 | 448 | 45 | 64 | 80.4% |
| `firmware/BmoGameboy/src/vendor/stella/atari.h` | `VENDOR_ENGINES` | 48 | 36 | 3 | 9 | 75.0% |
| `firmware/BmoGameboy/src/vendor/wswan/wswan.c` | `VENDOR_ENGINES` | 592 | 437 | 75 | 80 | 73.8% |
| `firmware/BmoGameboy/src/vendor/wswan/wswan.h` | `VENDOR_ENGINES` | 53 | 38 | 5 | 10 | 71.7% |
| `scripts/audit_all_games.py` | `SCRIPTS` | 184 | 164 | 0 | 20 | 89.1% |
| `scripts/auto_install_romsets.py` | `SCRIPTS` | 293 | 251 | 11 | 31 | 85.7% |
| `scripts/benchmark_repo.py` | `SCRIPTS` | 30 | 21 | 0 | 9 | 70.0% |
| `scripts/build_wasm_cores.py` | `SCRIPTS` | 214 | 198 | 3 | 13 | 92.5% |
| `scripts/bulk_cover_manager.py` | `SCRIPTS` | 349 | 295 | 16 | 38 | 84.5% |
| `scripts/bulk_download_manager.py` | `SCRIPTS` | 336 | 291 | 12 | 33 | 86.6% |
| `scripts/bundle_rules.py` | `SCRIPTS` | 139 | 116 | 3 | 20 | 83.5% |
| `scripts/check_collection_files.py` | `SCRIPTS` | 31 | 28 | 1 | 2 | 90.3% |
| `scripts/check_markdown_links.py` | `SCRIPTS` | 25 | 17 | 1 | 7 | 68.0% |
| `scripts/color_calc.py` | `SCRIPTS` | 52 | 37 | 4 | 11 | 71.2% |
| `scripts/convert_roms.py` | `SCRIPTS` | 66 | 52 | 3 | 11 | 78.8% |
| `scripts/deploy_sd_complete.py` | `SCRIPTS` | 227 | 164 | 28 | 35 | 72.2% |
| `scripts/deploy_to_sd.py` | `SCRIPTS` | 285 | 245 | 3 | 37 | 86.0% |
| `scripts/extract_atari.py` | `SCRIPTS` | 61 | 48 | 3 | 10 | 78.7% |
| `scripts/fetch_atari2600_boxarts.py` | `SCRIPTS` | 159 | 131 | 5 | 23 | 82.4% |
| `scripts/fetch_covers.py` | `SCRIPTS` | 434 | 370 | 18 | 46 | 85.3% |
| `scripts/find_all_collections.py` | `SCRIPTS` | 35 | 33 | 0 | 2 | 94.3% |
| `scripts/fix_genesis_smd.py` | `SCRIPTS` | 45 | 39 | 1 | 5 | 86.7% |
| `scripts/generate_ai_knowledge_base.py` | `SCRIPTS` | 423 | 380 | 12 | 31 | 89.8% |
| `scripts/generate_code_visualizations.py` | `SCRIPTS` | 1,561 | 1,407 | 37 | 117 | 90.1% |
| `scripts/install_git_hooks.py` | `SCRIPTS` | 66 | 49 | 8 | 9 | 74.2% |
| `scripts/process_games.py` | `SCRIPTS` | 139 | 116 | 0 | 23 | 83.5% |
| `scripts/repo_tools.py` | `SCRIPTS` | 31 | 22 | 0 | 9 | 71.0% |
| `scripts/rom_key.py` | `SCRIPTS` | 38 | 30 | 2 | 6 | 78.9% |
| `scripts/test_candidate_sets.py` | `SCRIPTS` | 35 | 32 | 1 | 2 | 91.4% |
| `scripts/test_runner.py` | `SCRIPTS` | 59 | 41 | 3 | 15 | 69.5% |
| `scripts/validate_repo.py` | `SCRIPTS` | 640 | 497 | 59 | 84 | 77.7% |
| `scripts/verify_collection_urls.py` | `SCRIPTS` | 30 | 27 | 0 | 3 | 90.0% |
| `scripts/verify_color_tokens.py` | `SCRIPTS` | 102 | 82 | 1 | 19 | 80.4% |
| `tests/host/battery_save_test.cpp` | `TESTS` | 10 | 9 | 0 | 1 | 90.0% |
| `tests/host/fakes/freertos/FreeRTOS.h` | `TESTS` | 10 | 7 | 0 | 3 | 70.0% |
| `tests/host/fakes/freertos/semphr.h` | `TESTS` | 10 | 7 | 0 | 3 | 70.0% |
| `tests/host/frame_pacing_test.cpp` | `TESTS` | 14 | 12 | 0 | 2 | 85.7% |
| `tests/host/frame_percentiles_test.cpp` | `TESTS` | 32 | 25 | 0 | 7 | 78.1% |
| `tests/host/harmonic_spring_test.cpp` | `TESTS` | 44 | 39 | 0 | 5 | 88.6% |
| `tests/host/pause_quit_policy_test.cpp` | `TESTS` | 33 | 24 | 2 | 7 | 72.7% |
| `tests/host/psram_zone_budget_test.cpp` | `TESTS` | 26 | 24 | 0 | 2 | 92.3% |
| `tests/host/rom_file_filter_test.cpp` | `TESTS` | 27 | 22 | 0 | 5 | 81.5% |
| `tests/host/rom_index_layout_test.cpp` | `TESTS` | 10 | 9 | 0 | 1 | 90.0% |
| `tests/host/rom_key_parity.cpp` | `TESTS` | 15 | 13 | 0 | 2 | 86.7% |
| `tests/host/spi_arbiter_test.cpp` | `TESTS` | 59 | 51 | 0 | 8 | 86.4% |
| `tests/host/theme_color_test.cpp` | `TESTS` | 22 | 20 | 0 | 2 | 90.9% |
| `tests/host_test_support.py` | `TESTS` | 135 | 112 | 4 | 19 | 83.0% |
| `tests/test_all_tiers_validation.py` | `TESTS` | 334 | 235 | 63 | 36 | 70.4% |
| `tests/test_apple_nintendo_ui.py` | `TESTS` | 84 | 60 | 11 | 13 | 71.4% |
| `tests/test_battery_save.py` | `TESTS` | 27 | 21 | 0 | 6 | 77.8% |
| `tests/test_bmo_face_spring.py` | `TESTS` | 60 | 52 | 0 | 8 | 86.7% |
| `tests/test_bmo_mascot_engine.py` | `TESTS` | 49 | 40 | 0 | 9 | 81.6% |
| `tests/test_box_art.py` | `TESTS` | 113 | 85 | 8 | 20 | 75.2% |
| `tests/test_elf_analyzer.py` | `TESTS` | 39 | 32 | 0 | 7 | 82.1% |
| `tests/test_enclosure_generator.py` | `TESTS` | 103 | 87 | 1 | 15 | 84.5% |
| `tests/test_frame_pacing.py` | `TESTS` | 44 | 36 | 0 | 8 | 81.8% |
| `tests/test_frame_percentiles.py` | `TESTS` | 39 | 32 | 0 | 7 | 82.1% |
| `tests/test_generated_artifacts_deterministic.py` | `TESTS` | 55 | 47 | 0 | 8 | 85.5% |
| `tests/test_genesis_emu.py` | `TESTS` | 167 | 109 | 36 | 22 | 65.3% |
| `tests/test_host_test_support.py` | `TESTS` | 45 | 39 | 0 | 6 | 86.7% |
| `tests/test_native_cores_execution.py` | `TESTS` | 429 | 390 | 5 | 34 | 90.9% |
| `tests/test_new_cores_and_qol.py` | `TESTS` | 199 | 143 | 15 | 41 | 71.9% |
| `tests/test_pause_quit_policy.py` | `TESTS` | 39 | 32 | 0 | 7 | 82.1% |
| `tests/test_pce_emu.py` | `TESTS` | 110 | 64 | 28 | 18 | 58.2% |
| `tests/test_platform_mapping.py` | `TESTS` | 115 | 101 | 0 | 14 | 87.8% |
| `tests/test_psram_budget.py` | `TESTS` | 39 | 30 | 0 | 9 | 76.9% |
| `tests/test_repo_tools.py` | `TESTS` | 71 | 53 | 4 | 14 | 74.6% |
| `tests/test_rom_file_filter.py` | `TESTS` | 49 | 41 | 0 | 8 | 83.7% |
| `tests/test_rom_index.py` | `TESTS` | 183 | 130 | 24 | 29 | 71.0% |
| `tests/test_rom_index_layout.py` | `TESTS` | 27 | 21 | 0 | 6 | 77.8% |
| `tests/test_rom_key.py` | `TESTS` | 86 | 72 | 0 | 14 | 83.7% |
| `tests/test_save_manager.py` | `TESTS` | 126 | 106 | 2 | 18 | 84.1% |
| `tests/test_sd_deploy.py` | `TESTS` | 123 | 103 | 0 | 20 | 83.7% |
| `tests/test_simulator_security.py` | `TESTS` | 35 | 27 | 0 | 8 | 77.1% |
| `tests/test_spi_arbiter.py` | `TESTS` | 48 | 39 | 0 | 9 | 81.2% |
| `tests/test_telemetry_plotter.py` | `TESTS` | 55 | 48 | 0 | 7 | 87.3% |
| `tests/test_theme_color.py` | `TESTS` | 27 | 21 | 0 | 6 | 77.8% |
| `tests/test_tier1_validation.py` | `TESTS` | 127 | 99 | 13 | 15 | 78.0% |
| `tests/test_tier2_validation.py` | `TESTS` | 98 | 78 | 10 | 10 | 79.6% |
| `tests/test_validate_repo_strict_mode.py` | `TESTS` | 55 | 44 | 0 | 11 | 80.0% |
| `tests/test_wokwi_hardware_parity.py` | `TESTS` | 79 | 65 | 0 | 14 | 82.3% |
| `tests/test_wswan_emu.py` | `TESTS` | 126 | 88 | 21 | 17 | 69.8% |
| `tools/bmo_simulator/atari_wasm.cpp` | `TOOLS` | 207 | 179 | 0 | 28 | 86.5% |
| `tools/bmo_simulator/bmo_face_arduino_compat.h` | `TOOLS` | 19 | 15 | 0 | 4 | 78.9% |
| `tools/bmo_simulator/bmo_face_compat/Arduino.h` | `TOOLS` | 1 | 1 | 0 | 0 | 100.0% |
| `tools/bmo_simulator/bmo_face_firmware.cpp` | `TOOLS` | 2 | 2 | 0 | 0 | 100.0% |
| `tools/bmo_simulator/bmo_face_wasm.cpp` | `TOOLS` | 207 | 167 | 0 | 40 | 80.7% |
| `tools/bmo_simulator/colem_wasm.cpp` | `TOOLS` | 239 | 210 | 0 | 29 | 87.9% |
| `tools/bmo_simulator/gameboy_wasm.cpp` | `TOOLS` | 242 | 204 | 0 | 38 | 84.3% |
| `tools/bmo_simulator/hal/Arduino.h` | `TOOLS` | 119 | 98 | 4 | 17 | 82.4% |
| `tools/bmo_simulator/hal/HardwareSerial.h` | `TOOLS` | 21 | 18 | 1 | 2 | 85.7% |
| `tools/bmo_simulator/hal/esp32-hal-log.h` | `TOOLS` | 7 | 7 | 0 | 0 | 100.0% |
| `tools/bmo_simulator/hal/esp_attr.h` | `TOOLS` | 9 | 9 | 0 | 0 | 100.0% |
| `tools/bmo_simulator/hal/esp_heap_caps.h` | `TOOLS` | 31 | 24 | 3 | 4 | 77.4% |
| `tools/bmo_simulator/hal/freertos/FreeRTOS.h` | `TOOLS` | 24 | 22 | 0 | 2 | 91.7% |
| `tools/bmo_simulator/hal/freertos/queue.h` | `TOOLS` | 12 | 11 | 0 | 1 | 91.7% |
| `tools/bmo_simulator/hal/freertos/semphr.h` | `TOOLS` | 17 | 16 | 0 | 1 | 94.1% |
| `tools/bmo_simulator/hal/freertos/task.h` | `TOOLS` | 22 | 16 | 3 | 3 | 72.7% |
| `tools/bmo_simulator/hal/pgmspace.h` | `TOOLS` | 45 | 40 | 1 | 4 | 88.9% |
| `tools/bmo_simulator/hal/rom/ets_sys.h` | `TOOLS` | 9 | 9 | 0 | 0 | 100.0% |
| `tools/bmo_simulator/hal/soc/gpio_reg.h` | `TOOLS` | 18 | 14 | 2 | 2 | 77.8% |
| `tools/bmo_simulator/hal/stdlib_noniso.h` | `TOOLS` | 21 | 18 | 0 | 3 | 85.7% |
| `tools/bmo_simulator/nes_wasm.cpp` | `TOOLS` | 252 | 219 | 1 | 32 | 86.9% |
| `tools/bmo_simulator/run_simulator.py` | `TOOLS` | 205 | 171 | 5 | 29 | 83.4% |
| `tools/enclosure/generate_bmo_enclosure.py` | `TOOLS` | 576 | 493 | 23 | 60 | 85.6% |
| `tools/guardian/__init__.py` | `GUARDIAN_TOOLCHAIN` | 5 | 2 | 2 | 1 | 40.0% |
| `tools/guardian/__main__.py` | `GUARDIAN_TOOLCHAIN` | 8 | 4 | 2 | 2 | 50.0% |
| `tools/guardian/cli.py` | `GUARDIAN_TOOLCHAIN` | 1,284 | 1,039 | 56 | 189 | 80.9% |
| `tools/guardian/core/__init__.py` | `GUARDIAN_TOOLCHAIN` | 26 | 22 | 2 | 2 | 84.6% |
| `tools/guardian/core/ast_linter.py` | `GUARDIAN_TOOLCHAIN` | 539 | 476 | 20 | 43 | 88.3% |
| `tools/guardian/core/bus_model.py` | `GUARDIAN_TOOLCHAIN` | 270 | 242 | 7 | 21 | 89.6% |
| `tools/guardian/core/clang_tidy_runner.py` | `GUARDIAN_TOOLCHAIN` | 146 | 126 | 3 | 17 | 86.3% |
| `tools/guardian/core/cppcheck_runner.py` | `GUARDIAN_TOOLCHAIN` | 128 | 105 | 6 | 17 | 82.0% |
| `tools/guardian/core/doc_verifier.py` | `GUARDIAN_TOOLCHAIN` | 359 | 317 | 14 | 28 | 88.3% |
| `tools/guardian/core/elf_analyzer.py` | `GUARDIAN_TOOLCHAIN` | 226 | 195 | 3 | 28 | 86.3% |
| `tools/guardian/core/host_bench.py` | `GUARDIAN_TOOLCHAIN` | 297 | 251 | 11 | 35 | 84.5% |
| `tools/guardian/core/psram_budget.py` | `GUARDIAN_TOOLCHAIN` | 336 | 300 | 5 | 31 | 89.3% |
| `tools/guardian/core/report_gen.py` | `GUARDIAN_TOOLCHAIN` | 295 | 269 | 3 | 23 | 91.2% |
| `tools/guardian/core/symbols.py` | `GUARDIAN_TOOLCHAIN` | 116 | 102 | 1 | 13 | 87.9% |
| `tools/guardian/core/ui_auditor.py` | `GUARDIAN_TOOLCHAIN` | 365 | 304 | 17 | 44 | 83.3% |
| `tools/guardian/mcp_server.py` | `GUARDIAN_TOOLCHAIN` | 264 | 234 | 6 | 24 | 88.6% |
| `tools/guardian/tests/__init__.py` | `GUARDIAN_TOOLCHAIN` | 3 | 1 | 2 | 0 | 33.3% |
| `tools/guardian/tests/test_guardian.py` | `GUARDIAN_TOOLCHAIN` | 482 | 395 | 27 | 60 | 82.0% |
| `tools/host_cores_runner.c` | `TOOLS` | 488 | 415 | 17 | 56 | 85.0% |
| `tools/host_test.cpp` | `TOOLS` | 123 | 99 | 2 | 22 | 80.5% |
| `tools/telemetry_plotter.py` | `TOOLS` | 241 | 202 | 7 | 32 | 83.8% |
| `visualizations/README.md` | `OTHER` | 25 | 18 | 0 | 7 | 72.0% |
| `visualizations/codecharta/bmo_gameboy.cc.json` | `OTHER` | 4,269 | 4,269 | 0 | 0 | 100.0% |
| `visualizations/codegraphcontext/cgc_graph.json` | `OTHER` | 550 | 550 | 0 | 0 | 100.0% |
| `visualizations/understand_anything/graph.json` | `OTHER` | 174 | 174 | 0 | 0 | 100.0% |

---

## 4. Host-Side Quantitative Microbenchmarks

| Benchmark Kernel | Iterations | Total Time | Avg Latency | Throughput | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **BMO_SDF_Procedural_Face_Render_Unculled** | 100 | 123.84 ms | 1238.420 µs | 3.31 MOps/s | Evaluates raw procedural 2D SDF math across 128x128 grid without spatial culling. |
| **BMO_SDF_Procedural_Face_Render_Culled** | 200 | 87.51 ms | 437.555 µs | 9.36 MOps/s | Evaluates procedural 2D SDF with bounding-box spatial culling (~75% math reduction). |
| **4Pixel_Coalesced_Aligned_Store_Kernel** | 50,000 | 112.04 ms | 2.241 µs | 13.39 MOps/s | Measures throughput of Xtensa LX7 32-bit coalesced memory stores for 3:2 scaling. |
| **O1_Palette_Scanline_Transformation** | 10,000 | 43.69 ms | 4.369 µs | 54.93 MOps/s | Measures 240-pixel scanline indexed-to-BGR565 palette transform throughput. |
| **CPU_Emulation_Opcode_Dispatch** | 500,000 | 32.06 ms | 0.064 µs | 15.60 MOps/s | Simulates CPU opcode fetch-decode-execute cycle dispatch throughput. |
| **Direct_GPIO_Atomic_Bitmask_Sampling** | 100,000 | 53.38 ms | 0.534 µs | 14.99 MOps/s | Measures atomic 8-pin bitmask unpacking (GPIO 4,5,6,7,16,17,18,21) replacing 8 digitalRead calls. |
| **SD_Catalog_O1_Query_And_Page_Jump** | 10,000 | 1.64 ms | 0.164 µs | 6.11 MOps/s | Measures 16,384 PSRAM ROM O(1) count queries and rapid +/-10 page-jump calculations. |
| **Native_C_11_Cores_575_Frames** | 575 | 178.04 ms | 309.640 µs | 3.23 MOps/s | Executes 50 frames across all 11 native C emulator engines + save-state replay (575 frames total, kFPS). |

---

## 5. Static AST & Anti-Pattern Linter Findings (9 total)

| Severity | Rule ID | File : Line | Issue Description | Suggested Action |
| :--- | :--- | :--- | :--- | :--- |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `d_iwad.c:217` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `d_iwad.c:346` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `d_iwad.c:773` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `m_config.c:2053` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `p_saveg.c:70` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `v_video.c:757` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `STACK_ALLOCATED_SCANLINE_BUFFER` | `ngp.h:22` | Large scanline buffer allocated on stack in hot path. | Make static module-level or 4-byte aligned DRAM/PSRAM buffer. |
| `🟡 WARNING` | `STACK_ALLOCATED_SCANLINE_BUFFER` | `snes.h:43` | Large scanline buffer allocated on stack in hot path. | Make static module-level or 4-byte aligned DRAM/PSRAM buffer. |
| `🟡 WARNING` | `STACK_ALLOCATED_SCANLINE_BUFFER` | `wswan.h:40` | Large scanline buffer allocated on stack in hot path. | Make static module-level or 4-byte aligned DRAM/PSRAM buffer. |

---

## 6. Cppcheck Findings

No Cppcheck findings.

---

## 7. Top Memory Consuming Symbols (Xtensa ESP32-S3 ELF)

### Largest Code Functions
| Size (Bytes) | Function Symbol | Section |
| :--- | :--- | :--- |
| 30,780 | `WGB::__gb_step_cpu(WGB::gb_s*)` | `text` |
| 19,805 | `(anonymous namespace)::renderMascotInternal(unsigned short*, int, int, int, int, int)` | `text` |
| 16,667 | `atari_cpu_step` | `text` |
| 16,504 | `colem_step_frame` | `text` |
| 15,180 | `PGB::__gb_step_cpu(PGB::gb_s*)` | `text` |
| 11,994 | `snes_cpu_step` | `text` |
| 11,413 | `_vfprintf_r` | `text` |
| 11,222 | `_svfprintf_r` | `text` |
| 9,907 | `WGB::__gb_draw_line(WGB::gb_s*)` | `text` |
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