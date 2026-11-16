# Repository Health Scorecard (`refactor/principal-overhaul`)

- **Baseline Commit:** `d8d4b5ef5cb314072652992805fff600961b2920` (`main` @ `2026-11-15`) | Prior stabilization: `techlead/audit-2` (`0b32109`)
- **Active Branch:** `refactor/principal-overhaul`
- **Physical Hardware Measurements (`HARDWARE_TESTED`):** `0` (all electrical, thermal, SPI signal integrity, and real-device frame-time metrics remain `NOT_TESTED` until physical bring-up per [`docs/guides/bringup.md`](../guides/bringup.md)).

---

## 1. Before vs. After Quantitative Metrics (Equal Scopes)

| Metric | Baseline (`d8d4b5e`) | Prior Stabilization (`0b32109`) | Current (`refactor/principal-overhaul`) | Outcome vs Baseline | Evidence Level |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`default` Profile App Image (`app0 = 8,388,608 B`)** | `3,291,744 B` (`39.2%`) | `3,293,216 B` (`39.3%`) | `3,275,568 B` (`39.0%`, `5,113,040 B` free) | **`-16,176 B`** (`-17,648 B` vs `0b32109`) | `TARGET_COMPILED` |
| **`default` Profile Static DRAM (`327,680 B`)** | `192,280 B` (`58.7%`) | `185,272 B` (`56.5%`) | `185,456 B` (`56.6%`, `142,224 B` free) | **`-6,824 B`** freed overall (`+10,240 B` DMA bounce vs baseline; `0` WiFi/BT) | `TARGET_COMPILED` |
| **`default` Profile IRAM Used** | `87,040 B` | `87,040 B` | `87,552 B` | `+512 B` (`std::atomic` / task-notify hot paths) | `TARGET_COMPILED` |
| **`profiler-compile-only` DRAM (`-DBMO_PROFILE=1`)** | `—` | `190,312 B` (`pre-PSRAM`) | `185,472 B` (`3,278,272 B` App) | **`-4,840 B` DRAM** (ring buffer moved to PSRAM) | `TARGET_COMPILED` |
| **`no-doom` / `mit` Footprint** | `101,960 B` / `75,840 B` DRAM | `94,952 B` / `69,856 B` DRAM | `95,136 B` / `69,504 B` DRAM (`2,999,840 B` / `878,688 B` App) | **`-6,824 B`** (`no-doom`), **`-6,336 B`** (`mit`) | `TARGET_COMPILED` |
| **Standalone Bring-Up Sketch (`bringup.ino`)** | None | `390,880 B` Flash / `24,600 B` RAM | `390,880 B` Flash / `24,600 B` RAM (`73,472 B` IRAM) | Isolated LCD, SD & GPIO tester (`esp32s3`) | `TARGET_COMPILED` |
| **First-Party Compiler Warnings (`-Wall -Wextra -Wshadow`)** | `0` across 3 profiles | `0` across `7` profiles | `0` across all `7` profiles | `clang-tidy` `0` errors + `cppcheck` `0` findings | `TARGET_COMPILED` |
| **Full Verification Suite (`tasks.py test` + `bench`)** | `82.56 s` | `74.69 s` (`12` Guardian + `168` Py + `26` CTest) | `30` CTest + `149` Py + `12` Guardian + `10/10` mutations + `bench --check` | Added behavioral app/save/blit tests, `ASan/UBSan`, `ruff`, `mypy`, `tsc`, `lizard` | `HOST_TESTED` |
| **Subset Inner-Loop Gate (`python tasks.py check --fast`)** | None | `~30 s` (`93` Py) | `17.83 s` (`95` fast Py + `12` Guardian + `ruff` + `mypy` + `tsc`) | Rapid inner loop with strict static gates | `HOST_TESTED` |
| **Formatted First-Party C/C++ Sources** | Unchecked | `128` files | **`162` files** (`clang-format --dry-run`) | Expanded modularization + behavioral C++ tests | `HOST_TESTED` |
| **First-Party Code Files `> 1,000` Lines** | `6` files (`sd_catalog.cpp` `1,004` + `2` C++ + `3` JS) | `5` deferred (`bmo_app.cpp` `1,192`, `bmo_face.cpp` `1,145`, `3` JS baseline) | **`0` firmware/simulator files** (`generate_hardware_layout_artifacts.py` = `1,009`) | Split `bmo_app.cpp` (`166`), `bmo_face_*` (`416`/`344`), `sd_*`, `display_*`, `simulator.js` (`613`), `tasks.py` (`68`) | `HOST_TESTED` |
| **Dead Files & SPDX Status** | `19` unused files | `0` unused files; **`125` SPDX** | Duplicated SD deploy scripts merged into `scripts/deploy_sd.py` | **`19` GPL/NC core-coupled files await `Decision 9`** (unmerged `license/spdx-review`) | `HOST_TESTED` |

---

## 2. Explicit 1–5 Scoring Rubric & Per-Area Ratings

### General Scale Definition
- **1 / 5 (Critical Gap):** No execution evidence or unsafe/broken baseline blocking hardware bring-up.
- **2 / 5 (Early / Unverified):** Analytical models or partial guards exist, but physical validation or core checks are missing.
- **3 / 5 (Workable with Open Debt):** Safe to test cleanly, with documented deferred refactors or unapproved owner decisions.
- **4 / 5 (Strong / Verified):** Strong automated host/compile guarantees and accurate documentation, with specific scoped items intentionally deferred (e.g., Windows CI matrix, coverage, SBOM, or hardware-gated items).
- **5 / 5 (Complete):** **All** explicit per-area criteria satisfied with automated multi-OS enforcement and zero deferred hygiene gaps. *(Zero areas claim `5 / 5` today.)*

### 11-Area Audit Ratings

| # | Area | Rating | Explicit Criteria for `5 / 5` (and Why Not `5 / 5`) | Evidence & Top Remaining Action |
| :- | :--- | :---: | :--- | :--- |
| 1 | **Hardware Validation** | **1 / 5** | Bench power, `GPIO 15 <= 3.35 V`, buttons, LCD, SD, and 600-frame logs recorded as `HARDWARE_TESTED`. Fails because **`0` physical measurements exist** (`1/5`: bring-up artifacts ready only on host). | Execute [`docs/guides/bringup.md`](../guides/bringup.md) and log in [`docs/hardware_measurements.md`](../hardware_measurements.md). |
| 2 | **Repository Structure** | **4 / 5** | Zero dead scripts, clean script boundaries, upstream patch series + mesh regen CI check, and simulator/test quarantine resolved (`Decision 8`). Rated **`4/5`**: duplicate SD deploy scripts merged into `scripts/deploy_sd.py` (`deploy_to_sd.py` / `deploy_sd_complete.py` retired) and monoliths split, while `Decision 8` cleanup remains open. | Resolve [`Decision 8`](../owner-decisions.md) (`unit_tests.cpp` & weak regex tests). |
| 3 | **Naming** | **3 / 5** | Single canonical sketch/repo name (`BmoHandheld` vs `BmoGameboy`) plus automated `clang-tidy`/`ruff` naming checks in CI. Rated **`3/5`**: `conventions.md`, `convert_covers.py`, metric `cm`, and hype scrubbing done, **but** `Decision 7` rename and naming-check rules await approval. | Decide sketch/repo naming in [`Decision 7`](../owner-decisions.md). |
| 4 | **Code Structure** | **4 / 5** | Zero firmware/JS files `> 1,000` lines, safe atomic/task-notify Core 0/1 sync, non-blocking boot & main loop, N-reset Safe Mode counter, and `config.h` decomposition (`Decision 5`/`F4`). Rated **`4/5`**: split `bmo_app`/`bmo_face`/`sd_catalog`/`display_worker`/`simulator`/`tasks.py`, replaced `volatile` with `std::atomic` + `xTaskNotifyGive`, fixed diagnostics/Konami/`loadState`/`gb_error` bugs, eliminated JS `createImageData` churn & $O(N^2)$ lookups; `config.h` hard-stop & NVS reset counter await bring-up (`F4`, `F5`). | Complete hardware bring-up and resolve NVS Safe Mode wear policy (`F5`). |
| 5 | **Tests** | **4 / 5** | Behavioral C++/WASI tests plus automated coverage (`gcov`/`coverage.py`), hash-pinned conformance ROMs, full emulator fault injection, and automated mutation testing with weak regex tests removed. Rated **`4/5`**: `30` CTest targets (incl. `app_behavior_test`, `save_manager_behavior_test`, `pico_blit_test`, `fuzz_storage_test`) + `149` Python + `12` Guardian + `10/10` mutations pass, missing coverage/`Decision 8` test trim. | Approve weak-test cleanup (`Decision 8`) and add coverage/conformance ROMs. |
| 6 | **CI & Release** | **4 / 5** | Multi-OS CI (Linux + Windows), SHA-pinned Actions, SAST (`CodeQL`), secret scan (`gitleaks`), OSV-scanner, CycloneDX SBOM, and `-ffile-prefix-map` reproducible-build check. Rated **`4/5`**: CI enforces `--strict-lint`, `lizard`, `guardian --cppcheck`, `extract_claims --fail-on-false`, `bench --check`, ASan/UBSan CTest, 7-profile strict compile, pinned `requirements.lock`, and `mit`-only release `SHA256SUMS.txt`; missing Windows matrix, CodeQL, SBOM, and reproducibility check. | Add Windows CI matrix, `CodeQL`, CycloneDX SBOM, and `-ffile-prefix-map` check. |
| 7 | **Performance Evidence** | **2 / 5** | `600`-frame `p50/p95/p99`, `Supported` (`>= 99%`) table, real SPI stability (`40` vs `80 MHz`), and measured battery current recorded in `docs/hardware_measurements.md`. Rated **`2/5`**: host & simulator `bench --check` prove algorithmic/GC wins (`HOST_TESTED`), **zero physical ESP32-S3 telemetry exists yet**. | Run physical `BMO_PROFILE=1` captures ([`Decision 2`](../owner-decisions.md), `hardware_measurements.md`). |
| 8 | **Documentation Truthfulness** | **3 / 5** | `5/5`: automated checks show `0` `FALSE`/`STALE`, deep semantic verification covers all numerical claims, and `0` `UNVERIFIABLE` items remain after physical measurements. Rated **`3/5`**: `extract_claims.py --fail-on-false` passes cleanly (`0` `FALSE`/`STALE`) and benchmark tables strictly separate `HOST_TESTED` vs `TARGET_COMPILED` vs `0 HARDWARE_TESTED`, while physical figures remain `UNVERIFIABLE` until bench logs. | Run `esptool flash-id`, measure `SD_MISO` & calipers, and capture `HARDWARE_TESTED` logs. |
| 9 | **Licensing** | **3 / 5** | Explicit owner license approval, clean SPDX separation between purely original first-party files and upstream-coupled wrappers, and zero GPL/Non-Commercial conflicts across release profiles. Rated **`3/5`**: compatibility matrix & `mit`-only release fixed, **but** prior commit `addcc07` stamped `MIT OR GPL-2.0-or-later` across `19` GPL/non-commercial core files; awaits **`Decision 9`** (staged unmerged on `license/spdx-review`: `b6b195e`, `3fd5c05`, `d2a9d43`) and `Decision 4`. | Review & approve **Decision 9** (merge `license/spdx-review`) and **Decision 4**. |
| 10 | **AI Environment** | **4 / 5** | Automated Hard-Stop protocol, synchronized `STATE.md`/`CODEMAP.md`, multi-tool IDE guards, generated pin-map SSOT, and clean verification SNR. Rated **`4/5`**: added `.cursorrules`, `.github/copilot-instructions.md`, `.clang-tidy`, `sync_ai_codemap.py --check`, `clang_tidy_runner.py`, and `lizard` pre-commit/CI gates, while full `pinmap.yaml` codegen and `Decision 8` test trim await owner sign-off. | Approve `Decision 8` weak-test cleanup and optionally unify `pinmap.yaml` post-bring-up. |
| 11 | **Dev Environment** | **4 / 5** | Cross-platform `doctor`/hooks/VSCode/devcontainer/clangd **plus** hashed Python lockfile, `-ffile-prefix-map` reproducible builds, and CI-verified Windows parity. Rated **`4/5`**: `requirements.lock` pinned, `tasks.py doctor`, `.clang-tidy`, `.clangd`, hooks, and modular `tools/tasks/` work locally; missing hash verification (`--require-hashes`) and Windows CI runner. | Add Windows CI check and `--require-hashes` wheel pins if cross-platform wheels permit. |

---

## 3. 30 / 60 / 90-Day Roadmap

| Window | Milestone Goal | Concrete Actions | Exit Criteria |
| :--- | :--- | :--- | :--- |
| **Days 1–30** | **Safe Bench Bring-Up & Owner Sign-Offs** | Wire USB-powered Stage 1 (`DevKitC-1` + ST7789 + SD + `10 kΩ` CS pull-ups + `8` buttons on perfboard/breadboard), flash `firmware/bringup/bringup.ino`, test `40` vs `80 MHz` SPI stability, record initial voltages & serial logs in [`docs/hardware_measurements.md`](../hardware_measurements.md), and approve Decisions `1–9` in [`docs/owner-decisions.md`](../owner-decisions.md) (including merging `license/spdx-review`). | LCD color fill, SD mount, and all `8` buttons marked `HARDWARE_TESTED`; Decisions `1–9` resolved. |
| **Days 31–60** | **Physical Caliper Survey & Enclosure Fit** | Measure finished perfboard thickness, header protrusion, `2×AA` / `3×AA` holder clearance, and display active window with calipers; log in `hardware_measurements.md`. Apply approved **Decision 3** to update `hardware/layout.json` & `generate_bmo_enclosure.py`, export STLs, and test-fit 3D shells. | Printed shells close cleanly around wired perfboard with zero Z-pinching or short circuits. |
| **Days 61–90** | **AA NiMH Power Stage & Real Core Measurements** | Assemble approved AA boost converter + switch + `1.5 A` polyfuse circuit (**Decision 1**), measure battery sag/runtime/temperature, flash main firmware, and capture 600-frame `p50/p95/p99` + hardware DMA/SPI telemetry into [`docs/compatibility.md`](../compatibility.md). | Battery-powered handheld runs stable `>= 2 h` with real FPS, heap/PSRAM, and thermal data (`HARDWARE_TESTED`). |
| **Day 90+** | **Post-Bring-Up Governance & CI Polish** | Execute **Decision 8** (retire dead `unit_tests.cpp` & `7` weak Python regex tests), **Decision 6** (`fram_save.*` removal / `GPIO 2` PWM), evaluate NVS Safe Mode auto-counter (`F5`), and add Windows CI + coverage + SAST + CycloneDX SBOM + conformance ROMs. | Multi-OS CI with coverage/SBOM passing and all owner-gated items resolved. |
