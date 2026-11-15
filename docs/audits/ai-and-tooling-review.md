# AI Agent Environment, Verification Signal-to-Noise & YAGNI Review

**Branch:** `techlead/audit-2` | **Repo Scale:** `1,033` tracked files | **Audit Scope:** AI instructions, git hooks, CI workflows, host tests, and optional sidecar subsystems

---

## 1. AI-Agent Environment Audit (Antigravity + Legacy Agent Configs)

### 1.1 Tracked Agent & Customization File Inventory

| File | Role | Lines | Words | Bytes | Est. Tokens (~bytes/4) | Status |
| :--- | :--- | ---: | ---: | ---: | ---: | :--- |
| [`AGENTS.md`](../../AGENTS.md) | Root repository instructions & Hard-Stop rules | 82 | 689 | 6,146 | ~1,536 | **Active** (always loaded) |
| [`firmware/AGENTS.md`](../../firmware/AGENTS.md) | C/C++ RTOS, PSRAM, SPI arbiter & core rules | 32 | 406 | 3,404 | ~851 | **Active** (firmware scope) |
| [`docs/ai/STATE.md`](../ai/STATE.md) | Live session & verification truth ledger | 26 | 301 | 2,638 | ~659 | **Active** (on-demand start) |
| [`docs/ai/CODEMAP.md`](../ai/CODEMAP.md) | Auto-generated first-party module map | 88 | 1,044 | 10,684 | ~2,671 | **Active** (generated via script) |
| [`CLAUDE.md`](../../CLAUDE.md) | Pointer to `AGENTS.md` for Claude Code | 7 | 57 | 547 | ~136 | Thin pointer (no duplication) |
| [`.claude/settings.json`](../../.claude/settings.json) | Legacy CLI allow/deny command globs | 17 | 38 | 459 | ~114 | Thin guardrail config |
| [`.codex/config.toml`](../../.codex/config.toml) | Pointer comment to `AGENTS.md` for Codex | 1 | 14 | 101 | ~25 | Thin pointer (no duplication) |
| **Total (if all 7 loaded)** | — | **253** | **2,549** | **23,979** | **~5,992** | — |
| **Always-loaded baseline (`AGENTS.md`)** | — | **82** | **689** | **6,146** | **~1,536** | Lean (< 2,000 tokens) |

### 1.2 Loading Confirmation & Contradiction Check

- **Antigravity IDE:** Automatically loads [`AGENTS.md`](../../AGENTS.md) into `<user_rules>` on every conversation turn (~`1,536` tokens). [`docs/ai/STATE.md`](../ai/STATE.md) and [`docs/ai/CODEMAP.md`](../ai/CODEMAP.md) load only when an agent reads them (`STATE.md` is `26` lines; `CODEMAP.md` is `88` lines — both well under `AGENTS.md` size limits).
- **Zero Rule Drift:** `CLAUDE.md` and `.codex/config.toml` delegate directly to `AGENTS.md` instead of repeating pin maps or stale feature claims. `firmware/AGENTS.md` adds C++ details cleanly without contradicting `AGENTS.md`.
- **Context Protection:** `AGENTS.md` explicitly instructs agents never to read `firmware/BmoGameboy/src/third_party/**`, `simulator/three.min.js`, `simulator/bmo_enclosure_meshes.js`, `build/**`, or `Book/**` in full.

### 1.3 Recommended Antigravity Terminal Auto-Execute Policy

Configure Antigravity IDE command allow/deny rules (matching [`.claude/settings.json`](../../.claude/settings.json)) so agents can run diagnostics freely while **never** touching hardware or destructive git history:

- **Safe Auto-Allow (Host Verification & Compile-Only):**
  - `python tasks.py doctor`
  - `python tasks.py check*`
  - `python tasks.py test`
  - `python tasks.py format --check`
  - `python tasks.py size*`
  - `python tasks.py build*`
  - `.\arduino-cli.exe compile *` / `arduino-cli compile *`
  - `python -m unittest *`
  - `python -m tools.guardian *`
  - `python scripts/verify_color_tokens.py` / `python scripts/check_markdown_links.py`
  - `ctest --test-dir build/*`
  - Read-only git queries (`git status`, `git diff`, `git log`, `git grep`)
- **Mandatory Deny / Require Owner Confirmation:**
  - `arduino-cli upload *` / `.\arduino-cli.exe upload *` / `esptool*`
  - `python tasks.py flash*` / `python tasks.py monitor*` (COM port access / physical hardware)
  - `git push --force*` / `git reset --hard*`
  - Any script writing inside `games/`, `roms/`, `sdcard/`, `.rom_cache/`, or `Book/`

---

## 2. Verification Check Signal-to-Noise Audit

Every automated check in `.githooks/`, `tasks.py`, `scripts/validate_repo.py`, `tools/guardian/`, and `.github/workflows/ci.yml`:

| Check / Gate | Real Bug Class Caught | Execution Cost | Verdict | Notes |
| :--- | :--- | :--- | :--- | :--- |
| `.githooks/pre-commit` | Staged blobs `>= 1 MiB`, leaked keys/tokens, Python syntax errors, `FEATURE_BATTERY_MONITOR`/`FEATURE_AUDIO` override | `< 1 s` | **Keep** | High signal; catches catastrophic mistakes before commit. |
| `.githooks/commit-msg` | Non-Conventional Commit subjects | `< 0.2 s` | **Keep** | Clean git log hygiene. |
| `validate_repo.check_firmware_safety_guardrails` | Missing `CoreRegistry` teardown, raw `Serial.print` in cores, unaligned `emu_walnut.cpp` cast, `partitions.csv` overlap/`app0 < 8 MB`, release debug macros | `< 0.5 s` | **Keep** | Directly prevents ESP32 runtime panics and release-mode test-loop locks. |
| `tasks.check_include_case_sensitivity` | Windows case-insensitive `#include` mismatches breaking Linux CI/Arduino builds | `< 0.5 s` | **Keep** | Catches real cross-OS compile breaks immediately. |
| `scripts/check_markdown_links.py` | Broken internal Markdown links and accidental `file://` local URIs | `< 0.3 s` | **Keep** | Keeps repo documentation navigable and portable. |
| `scripts/verify_color_tokens.py` | Theme desync across `theme.h`, `simulator.css`, `DESIGN.md`, `design/tokens.json` + WCAG AA contrast failures | `< 0.5 s` | **Keep** | Fast deterministic token parity + contrast check. |
| `tasks.py format --check` (`clang-format`) | Formatting churn across `126` first-party C/C++ files | `< 1.5 s` | **Keep** | Keeps diffs clean without blocking edits. |
| `CMake CTest` (`25` native C++17 targets, CI + ASan/UBSan) | Real memory errors, overflow, CRC32 bugs, save corruption (`fuzz_storage_test`, `atomic_fs_test`, `peanut_opcode_test`, `walnut_opcode_test`) | `~2–4 s` local build/run | **Keep** | Highest-value code test layer on host CPU. |
| `tests/test_native_cores_execution.py` + `scripts/run_mutation_sanity.py` | Broken upstream C/C++ WASI compilation or broken 50-frame render across `11` cores; test suite catching `10` injected bugs | `~10–15 s` | **Keep (full/CI)** | Properly excluded from `tasks.py check --fast`; catches real C core crashes/regressions. |
| `arduino-cli` 3-profile compile (`-Wall -Wextra -Wshadow -Werror`) + `size --check-budgets` | Linker bloat, missing declarations, shadow bugs, Dram/Flash exhaustion | `~45–90 s` cached | **Keep (CI/firmware)** | Essential `TARGET_COMPILED` gate for `default`, `no-doom`, `mit`. |
| `tools.guardian`: `HardwareBusModel`, `ElfAnalyzer`, `PsramBudgetAnalyzer` | Oversized frame wire budgets (`>16.67 ms`), bloated static `.bss`/DRAM arrays, PSRAM oversubscription | `< 1 s` | **Keep** | Grounded objective byte and wire-time math. |
| `tools.guardian`: `FirmwareAstLinter` & `clang_tidy_runner` | Unprotected `SPI`/`TFT` calls, large local stack arrays (`> 1024 B`) | `< 1 s` | **Simplify** | Keep `SpiArbiter` + large stack array rules; avoid duplicating compiler `-Wall/-Wshadow`. |
| `validate_repo.check_structural_soundness` | Missing wrapper filenames or extensions hardcoded in a Python list | `< 0.1 s` | **Simplify** | Overlaps with `core_registry.h` compilation + `test_pin_snapshot.py`. |
| Python source-regex & inline-toy tests (`test_tier1/2/all`, `test_new_cores_and_qol`, `test_genesis/pce/wswan_emu`) | Only checks file paths, text substrings in `.cpp`, or local inline Python helper formulas | `~0.5 s` | **Cut (pending Decision 8)** | **Low signal-to-noise:** creates false confidence because no C++ core runs. Superseded by `test_native_cores_execution.py`. |

---

## 3. Host Test Suite Classification (`tests/` & `tools/guardian/tests/`)

Classification of all `38` Python `tests/test_*.py` modules (`185` `def test_*` methods), `25` CMake CTest targets, and `12` Guardian unit tests (`test_guardian.py`):

| Category | Modules / Targets | What Is Actually Tested | Assessment |
| :--- | :--- | :--- | :--- |
| **Behavioral C++17 Native (`25` CTest executables, incl. CI ASan/UBSan)** | `atomic_fs_test`, `fuzz_storage_test`, `rom_index_v3_test`, `save_state_format_test`, `battery_save_runtime_test`, `spi_arbiter_test`, `peanut_opcode_test`, `walnut_opcode_test`, `bmo_face_sdf_test`, `boot_safe_mode_test`, `core_registry_test`, `input_frame_test`, `pause_quit_policy_test`, `frame_pacing_test`, `frame_percentiles_test`, `profiler_test`, `psram_zone_budget_test`, `rom_file_filter_test`, `rom_path_match_test`, `box_art_lookup_test`, `console_catalog_test`, `harmonic_spring_test`, `theme_color_test`, `rom_key_parity_test`, `rom_index_layout_test` | Compiles and runs actual first-party C++17 headers/sources (and opcodes) under strict `-Werror` and ASan/UBSan (`HOST_TESTED`). | **High value** — real bug detection. |
| **Python Wrappers Driving C++ Native Runners (`13` modules, `30` methods)** | `test_battery_save` (`1`), `test_bmo_face_spring` (`1`), `test_box_art` (`7`), `test_frame_pacing` (`2`), `test_frame_percentiles` (`1`), `test_pause_quit_policy` (`1`), `test_platform_mapping` (`3`), `test_psram_budget` (`3`), `test_rom_file_filter` (`2`), `test_rom_index_layout` (`1`), `test_rom_key` (`6`), `test_spi_arbiter` (`1`), `test_theme_color` (`1`) | Compiles standalone C++ test harnesses (`g++`/`clang++`) and checks exit codes/stdout. | **High value** — real compiled C++ execution. |
| **Behavioral C/C++ WASI Core Execution (`1` module, `15` methods)** | `tests/test_native_cores_execution.py` (`host_cores_runner.c`, `host_cores_cpp_bridge.cpp`, `bridge_*.c`) | Compiles `11` genuine upstream C/C++ cores (`peanut_gb`, `walnut`, `agnes`, `smsplus`, `huexpress`, `stella`, `fake08`, `gwenesis`, `snes9x`, `oswan`, `ngp`, `handy`, `smsplus/colem`) to `.wasm` and runs `50` frames with synthetic test ROMs. | **High value (smoke)** — verifies compilation & 50-frame liveness, not full retail ROMs. |
| **Behavioral Python Tooling, CAD & Security (`10` modules, `50` methods)** | `test_sd_deploy` (`7`), `test_repo_tools` (`9`), `test_hardware_layout` (`4`), `test_enclosure_generator` (`6`), `test_simulator_security` (`6`), `test_build_and_tasks` (`5`), `test_validate_repo_strict_mode` (`6`), `test_telemetry_plotter` (`4`), `test_mutation_sanity` (`1`), `test_elf_analyzer` (`2`) | Runs actual production Python tools (`deploy_to_sd`, layout/CAD generators, HTTP path-traversal security server, ELF parser, mutation harness). | **High value** — real behavioral coverage. |
| **Static Contract & Spec-Mirror Checks (`7` modules, `27` methods)** | `test_pin_snapshot` (`3`), `test_wokwi_hardware_parity` (`4`), `test_apple_nintendo_ui` (`4`), `test_bmo_mascot_engine` (`4`), `test_host_test_support` (`1`), `test_save_manager` (`5` Python struct pack/unpack mirror), `test_rom_index` (`6` Python struct pack/unpack mirror) | Pins GPIO/Wokwi parity, WCAG UI contrast, enum/table order, and Python `struct` format parity (`save_state_format_test.cpp` and `rom_index_v3_test.cpp` cover the real C++ code). | **Medium value** — explicit structural contracts. |
| **Source-Text Regex / Inline Toy Checks (`7` modules, `63` methods)** | `test_tier1_validation` (`7`), `test_tier2_validation` (`6`), `test_all_tiers_validation` (`12`), `test_new_cores_and_qol` (`17`), `test_genesis_emu` (`7`), `test_pce_emu` (`6`), `test_wswan_emu` (`8`) | Asserts that `self.assertIn("execute", cpp_text)` passes or checks inline local Python toy functions (`def decode_region`, `def to_physical`) that never call firmware code. | **Low / Negative SNR** — creates an illusion of per-core coverage; candidate for retirement (**Decision 8**). |
| **Guardian Unit Tests (`1` module, `12` methods)** | `tools/guardian/tests/test_guardian.py` | Verifies `HardwareBusModel` wire math, `FirmwareAstLinter` stack/SPI detection, `CppcheckRunner`, `ClangTidyRunner`, and markdown link audit behavior. | **High value** — validates analyzer math. |

---

## 4. YAGNI & Optional Subsystem Audit

Evaluated against **Rule 0.3**: *"converge on a WORKING, POCKETABLE, SAFE physical device, and cut or quarantine whatever does not serve that."*

| Subsystem | Footprint | Direct Value to First Working Physical Handheld | Verdict | Recommendation (`Decision 8`) |
| :--- | :--- | :--- | :--- | :--- |
| **Standalone Bring-Up Sketch** (`firmware/bringup/`) | `3` files, `246` lines | **Critical** — tests TFT, SD, and every tactile button independently before touching emulators. | **KEEP** | Make this Step 1 after soldering Stage 1. |
| **Perfboard & Wiring Map** (`hardware/layout.json`, `scripts/generate_hardware_layout_artifacts.py`) | `4` files, `1,304` lines | **High** — gives exact `31×46` hole coordinates and wire-by-wire CSV matching `config.h`. | **KEEP** | Re-generate after any approved Decision #1/#3/#6 change. |
| **Parametric 3D Enclosure CAD** (`hardware/enclosure/`, `export_enclosure_web_meshes.py`) | `3` files, `832` lines | **Essential in Stage 3**, currently stale (`8.20×2.60 cm` vs `8.80×3.20 cm` layout). | **SIMPLIFY / HOLD** | Freeze until owner logs real caliper measurements (`Decision 3`). |
| **Design Token Parity** (`design/`, `scripts/verify_color_tokens.py`) | `2` files, `354` lines | Low-maintenance check keeping TFT BGR565 colors consistent with UI contrast rules. | **KEEP** | Nearly zero cost; prevents unreadable menus on the 2.4" LCD. |
| **Browser 2D/WASM Simulator** (`simulator/` minus 3D libs) | `32` files, `8,407` lines | Helpful UI/mascot preview for a non-coder owner, not real ESP32 hardware validation. | **QUARANTINE (Feature Freeze)** | Fix glaring bugs only; invest **zero** feature work here pre-bring-up. |
| **3D Three.js Internal Viewer** (`bmo_internals_3d.js`, `three.min.js`, meshes) | `5` files, `2,620` lines (`1.33 MB`) | Large JS payload (mostly vendored `three.min.js` + baked mesh JS); visual teardown aid only. | **ARCHIVE / FREEZE** | Keep frozen; do not expand alongside `hardware/layout.json` unless needed. |
| **Static Guardian Tooling** (`tools/guardian/`) | `12` files, `2,232` lines | Phase 2 pruned fabricated graphs; remaining bus math, PSRAM ledger, and ELF analyzer catch real memory risks. | **SIMPLIFY** | Keep `bus-calc`, `psram-budget`, `elf-analyzer`; consider trimming redundant regex checks later. |
| **Mutation Testing Harness** (`scripts/run_mutation_sanity.py`) | `1` file, `254` lines | Proves C++/Python host tests are non-tautological (`10` mutations killed). | **KEEP IN CI ONLY** | Already omitted from `--fast`; revisit only if CI times degrade. |
| **Console Museum UI** (`console_catalog.h`, `simulator/js/museum.js`) | `167` C++ lines + `421` JS lines | Nice-to-have encyclopedic info screen; zero runtime overhead in Flash/PSRAM. | **KEEP OR SIMPLIFY** | Low cost; no urgency to strip out unless owner wants a minimalist ROM launcher. |
| **Dormant HAL & On-Device Test Scaffolds** (`fram_save.*`, `battery.*`, `audio.*`, `unit_tests.cpp`, `profiler.*`) | `7` files, `832` lines | `fram_save.*` collides with UART (`GPIO 43/44`) and has zero hardware; `unit_tests.cpp` is dead firmware code superseded by host CTest; `battery.*` awaits Stage 2 (`ADR 0006`). | **DELETE `fram_save.*` & `unit_tests.cpp` / KEEP `battery.*` & `profiler.*` GATED** | Queued in `Decision 6` (`fram_save`) and `Decision 8` (`unit_tests.cpp`, test/simulator quarantine). |
