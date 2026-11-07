# .agents/rules/ Index
**RULESET_VERSION: 8**

**Zero-Context Fast Start (Recommended for all AI models):**
1. Read [`31_quick_start_primer.md`](31_quick_start_primer.md), [`00_hard_stops.md`](00_hard_stops.md), and [`01_hardware.md`](01_hardware.md).
2. Run `python -m tools.guardian prompt-pack "<your task>"` to generate a self-contained context packet with exact target files, live C/C++ signatures, pin mappings, and verification commands.

| File | Purpose | When to Read |
|---|---|---|
| [`00_hard_stops.md`](00_hard_stops.md) | Irreversible/boot-bricking mistakes to never make | **Always** |
| [`01_hardware.md`](01_hardware.md) | Physically wired ground truth + pin map (`config.h`) | **Always** |
| [`02_architecture.md`](02_architecture.md) | Toolchain, build config, dual-core FreeRTOS & SPI patterns | Firmware tasks |
| [`03_conventions.md`](03_conventions.md) | Repo structure, testing rule, licensing, doc maintenance | Firmware tasks |
| [`04_known_issues.md`](04_known_issues.md) | Technical debt log + chronological session audit log | **Update on completion** |
| [`05_git_workflow.md`](05_git_workflow.md) | Mandatory two-part commit format (`-m "subject" -m "bullets"`) | **Before committing** |
| [`06_verification_standards.md`](06_verification_standards.md) | Evidence rules, status vocabulary, banned language | **Always** |
| [`07_task_protocol.md`](07_task_protocol.md) | Step-by-step task checklist + report format | Task workflow |
| [`08_ui_style_guide.md`](08_ui_style_guide.md) | Theme constants, layout, typography, motion rules | UI tasks |
| [`09_testing_infrastructure.md`](09_testing_infrastructure.md) | Native x64 10-core harness (`host_cores_runner.c`) & Blargg tests | Core/CPU tasks |
| [`10_symbol_reference.md`](10_symbol_reference.md) | Ground-truth symbol table (prevents invented names) | Check before citing APIs |
| [`11_rules_meta.md`](11_rules_meta.md) | Rules about the rules directory itself | Rule edits |
| [`12_extensibility_contract.md`](12_extensibility_contract.md) | Emulator core contract, teardown rule, no-per-frame-alloc rule | Emulator tasks |
| [`13_code_style.md`](13_code_style.md) | Code style, conventions, naming | Code edits |
| [`14_error_handling_and_fault_isolation.md`](14_error_handling_and_fault_isolation.md) | Failure paths and fault isolation | Core tasks |
| [`15_performance_budgets.md`](15_performance_budgets.md) | Hot path tracking, zero-allocation rules | Core tasks |
| [`16_logging_and_diagnostics.md`](16_logging_and_diagnostics.md) | Log levels, visibility, boot logs | Core tasks |
| [`17_release_and_versioning.md`](17_release_and_versioning.md) | Version definitions and release checklist | Releases |
| [`18_dependency_and_vendor_sync.md`](18_dependency_and_vendor_sync.md) | Patch handling and upstream syncing | Vendor updates |
| [`19_security_and_data_integrity.md`](19_security_and_data_integrity.md) | Untrusted input constraints & atomic save writes | Storage/I/O tasks |
| [`20_multi_agent_protocol.md`](20_multi_agent_protocol.md) | Agent-to-agent crediting and handoffs | Handoffs |
| [`21_documentation_standards.md`](21_documentation_standards.md) | In-code and out-of-code documentation rules | Docs tasks |
| [`22_review_checklist.md`](22_review_checklist.md) | Lightweight everyday self-review | Before reporting done |
| [`23_incident_postmortem_log.md`](23_incident_postmortem_log.md) | Record of historical hardware/process incidents | Reference |
| [`24_vendor_flag_safety.md`](24_vendor_flag_safety.md) | Mandatory read-before-enable protocol for vendor `#define` flags | Touching vendor flags |
| [`25_game_compatibility_ledger.md`](25_game_compatibility_ledger.md) | Per-game hardware test ledger | Emulator changes |
| [`26_emulator_exit_contract.md`](26_emulator_exit_contract.md) | `destroyActiveEmulator()` 14-core PSRAM cleanup contract | Emulator/menu tasks |
| [`27_codebase_map.md`](27_codebase_map.md) | Full architecture map: directories, 7 states, 14 cores, SPI, PSRAM | Architecture lookup |
| [`28_display_and_spi_contract.md`](28_display_and_spi_contract.md) | BGR565 wire format, Core 0 worker, `SpiArbiter` sharing rules | Display/SPI tasks |
| [`29_adding_a_baked_rom.md`](29_adding_a_baked_rom.md) | Step-by-step checklist for baking a new ROM into flash | Baking ROMs |
| [`30_common_agent_mistakes.md`](30_common_agent_mistakes.md) | Anti-pattern catalogue (M-1 through M-20) | Debugging/preflight |
| [`31_quick_start_primer.md`](31_quick_start_primer.md) | Zero-context 90-second agent on-ramp & decision matrix | **Always start here** |
| [`32_modular_core_template.md`](32_modular_core_template.md) | Copy-paste template and scaffolding for adding new cores/screens | New core/screen tasks |
| [`33_agent_handoff_and_optimization_cycle.md`](33_agent_handoff_and_optimization_cycle.md) | Agent-to-agent continuous optimization loop | Session handoff |
| [`34_ai_agent_sandbox_and_guardrails.md`](34_ai_agent_sandbox_and_guardrails.md) | LLM anti-hallucination protocols, memory alignment, hardware safety | **Always** |
| [`35_bmo_face_contract.md`](35_bmo_face_contract.md) | Procedural SDF mascot renderer contract & PSRAM canvas | Mascot/UI tasks |
| [`36_bug_intake_protocol.md`](36_bug_intake_protocol.md) | Structured bug intake protocol for device misbehavior | Bug investigation |
| [`37_rom_governance_and_flash_budget.md`](37_rom_governance_and_flash_budget.md) | ROM tracking resolution & 8MB `app0` flash-budget invariant | Assets/ROM tasks |
| [`38_mandatory_documentation_update.md`](38_mandatory_documentation_update.md) | Required documentation updates per change type | Before committing |
| [`39_performance_and_benchmark_framework.md`](39_performance_and_benchmark_framework.md) | Guardian performance & ground-truth framework governance | Performance tasks |
| [`40_ai_knowledge_graph_and_indexing_protocol.md`](40_ai_knowledge_graph_and_indexing_protocol.md) | AI Knowledge Graph, AST Indexing & Decision Matrix Protocol | Knowledge base tasks |
| [`41_engineering_communication_and_critical_pushback.md`](41_engineering_communication_and_critical_pushback.md) | Engineering communication, fact-checking & critical pushback | All sessions |
| [`42_quality_of_life_and_roadmap_ledger.md`](42_quality_of_life_and_roadmap_ledger.md) | Quality of Life (QoL), UI/UX and system roadmap ledger | Feature planning |
| [`43_no_ai_jargon_and_buzzwords.md`](43_no_ai_jargon_and_buzzwords.md) | Strict ban on AI buzzwords, marketing filler, and hype adjectives | **Always** |
| [`44_anti_ai_ui_design_standards.md`](44_anti_ai_ui_design_standards.md) | Anti-AI UI & visual craft standards, design tokens, contrast & motion | UI/Display tasks |

## Current references

- [Generated firmware symbol index](../../docs/generated/symbols.md)
- [Active issue registry](04_known_issues.md)
- [Current remediation handoff](../../docs/handoffs/2026-11-07-remediation.md)
- [Archived issue history](../../docs/archive/known-issues-history.md)
