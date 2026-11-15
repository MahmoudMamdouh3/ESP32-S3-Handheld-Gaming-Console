# Repository Health Scorecard (`techlead/audit-2`)

- **Baseline Commit:** `d8d4b5ef5cb314072652992805fff600961b2920` (`main` @ `2026-11-15`)
- **Audit Branch:** `techlead/audit-2`
- **Physical Hardware Measurements (`HARDWARE_TESTED`):** `0` (all electrical, thermal, SPI signal integrity, and real-device frame-time metrics remain `NOT_TESTED` until physical bring-up per [`docs/guides/bringup.md`](../guides/bringup.md)).

---

## 1. Before vs. After Quantitative Metrics (Equal Scopes)

| Metric | Baseline (`d8d4b5e`) | Post-Remediation (`techlead/audit-2`) | Delta / Outcome | Evidence Level |
| :--- | :--- | :--- | :--- | :--- |
| **`default` Profile App Image (`app0 = 8,388,608 B`)** | `3,291,744 B` (`39.2%`) | `3,293,225 B` (`39.3%`, `5,095,383 B` free) | `+1,481 B` (CRC32 `BootBreadcrumb`, `M6502` table moved from DRAM to flash, `doom_palette.h`) | `TARGET_COMPILED` |
| **`default` Profile Static DRAM (`327,680 B`)** | `192,280 B` (`58.7%`) | `185,272 B` (`56.5%`, `142,408 B` free) | **`-7,008 B` (`-6.84 KiB` freed)**; `0` WiFi/BT symbols in `20,277` ELF syms | `TARGET_COMPILED` |
| **`default` Profile IRAM Used** | `87,040 B` | `87,040 B` | `0 B` (`unit_tests.cpp` false `~10-15 KB` estimate corrected) | `TARGET_COMPILED` |
| **`no-doom` / `mit` Static DRAM** | `101,960 B` / `75,840 B` | `94,952 B` / `69,856 B` (`3,016,825 B` / `893,881 B` Flash) | **`-7,008 B`** (`-6.84 KiB`) in `no-doom`; **`-5,984 B`** in `mit` | `TARGET_COMPILED` |
| **Standalone Bring-Up Sketch (`bringup.ino`)** | None | `390,724 B` Flash / `24,600 B` RAM | Isolated LCD, SD & GPIO tester (`esp32s3`) | `TARGET_COMPILED` |
| **First-Party Compiler Warnings (`-Wall -Wextra -Wshadow`)** | `0` across 3 profiles | `0` across all `7` compile configs | Verified post-edit on `default`, `no-doom`, `mit`, `audio`, `battery`, `debug-log`, `profiler` | `TARGET_COMPILED` |
| **Full Gate (`python tasks.py check`)** | `82.56 s` | `74.69 s` (`12` Guardian + `165` Py + `26` CTest, `100%` pass) | Equal full-gate comparison (added `doom_palette_test`, CRC breadcrumb tests, pin parity) | `HOST_TESTED` |
| **Subset Inner-Loop Gate (`python tasks.py check --fast`)** | None | `29.31 s` (`12` Guardian + `90` fast Py tests) | **Different scope** from `check` (`-1` slow CTest/mutation/WASI set); rapid pre-commit inner loop | `HOST_TESTED` |
| **Claims Extracted ([`claims-inventory.md`](claims-inventory.md))** | Unreconciled (`14`-claim manual sample) | **`643` claims** scripted extraction (`scripts/extract_claims.py`) | **`TRUE=601` (`93.5%`), `UNVERIFIABLE=42`, `STALE=0`, `FALSE=0`** (`0.16 s`) | `HOST_TESTED` |
| **First-Party Code Files `> 1,000` Lines** | `6` files (`sd_catalog.cpp` `1,004` + `2` C++ + `3` JS) | `5` deferred (`bmo_app.cpp` `1,197`, `bmo_face.cpp` `1,145`, `3` JS files); `sd_catalog.cpp` -> `919` | Extracted `popular_games_table.h`; remaining splits deferred post-bring-up | `HOST_TESTED` |
| **Dead Files & SPDX Status** | `19` unused files; `0` SPDX headers | `0` unused files (`941509c`, `0b22efc`); **`125` first-party SPDX files** | **`19` GPL/NC core-coupled files await `Decision 9`** (unmerged `license/spdx-review`) | `HOST_TESTED` |

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
| 2 | **Repository Structure** | **4 / 5** | Zero dead scripts, clean script boundaries (`deploy_to_sd` merged), upstream patch series + mesh regen CI check, and simulator/3D quarantine resolved (`Decision 8`). Rated **`4/5`**: `19` dead files gone & SSOT layout created, but duplicate SD deploy scripts and `unit_tests.cpp` removal remain open. | Resolve [`Decision 8`](../owner-decisions.md) and merge duplicate deploy scripts post-bring-up. |
| 3 | **Naming** | **3 / 5** | Single canonical sketch/repo name (`BmoHandheld` vs `BmoGameboy`) plus automated `clang-tidy`/`ruff` naming checks in CI. Rated **`3/5`**: `conventions.md`, `convert_covers.py`, metric `cm`, and hype scrubbing done, **but** `Decision 7` rename and automated linter naming gates await approval/CI. | Decide sketch/repo naming in [`Decision 7`](../owner-decisions.md). |
| 4 | **Code Structure** | **3 / 5** | Files `<= 500–1,000` lines, safe atomic/queue Core 0/1 state, non-blocking main loop, N-reset Safe Mode counter, and `DisplayEmu`/`config.h` cleanly decomposed. Rated **`3/5`**: `-7,008 B` DRAM freed, CRC breadcrumb hardened, and `0` WiFi/BT linkage, **but** `F1..F6, F9..F15` remain partially deferred until post-bring-up. | Complete physical bring-up, then split `bmo_app.cpp` (`1,197`), `bmo_face.cpp` (`1,145`), and shared `s_*` state. |
| 5 | **Tests** | **4 / 5** | Behavioral C++/WASI tests plus automated coverage (`gcov`/`coverage.py`), hash-pinned conformance ROMs, full emulator fault injection, and automated mutation testing with weak regex tests removed. Rated **`4/5`**: `26` CTest targets (incl. cold-boot garbage & SD removal fault-injection) + `15` WASI + `10/10` mutations pass, missing coverage/ROM conformance/`Decision 8` test trim. | Approve weak-test cleanup (`Decision 8`) and add coverage/conformance ROMs in Phase 2 CI. |
| 6 | **CI & Release** | **4 / 5** | Multi-OS CI (Linux + Windows), SHA-pinned Actions, SAST (`CodeQL`), secret scan (`gitleaks`), workflow/doc linters (`actionlint`/`shellcheck`/`markdownlint`/`codespell`), OSV-scanner, CycloneDX SBOM, and `-ffile-prefix-map` reproducible-build check. Rated **`4/5`**: Linux CI, SHA pins, `permissions`, ASan/WASI, 3-profile strict compile, and `mit`-only release `SHA256SUMS.txt` are done, **but** Windows CI runner, SAST, coverage, SBOM, and reproducibility check are missing. | Add Windows CI matrix, `actionlint`/`gitleaks`/`CodeQL`, CycloneDX SBOM, and `-ffile-prefix-map` check. |
| 7 | **Performance Evidence** | **2 / 5** | `600`-frame `p50/p95/p99`, `Supported` (`>= 99%`) table, real SPI stability (`40` vs `80 MHz`), and measured battery current recorded in `docs/hardware_measurements.md`. Rated **`2/5`**: wire/scan/latency models & capture protocol are honest (`NOT_TESTED`), zero hardware telemetry exists yet. | Run physical `BMO_PROFILE=1` captures ([`Decision 2`](../owner-decisions.md), `hardware_measurements.md` Step `9`). |
| 8 | **Documentation Truthfulness** | **4 / 5** | Automated claim scanner shows `0` `FALSE` and `0` `STALE` claims AND caliper/physical measurements verified on real device (`0` `UNVERIFIABLE`). Rated **`4/5`**: `extract_claims.py` audits `643` doc/comment claims with `0` `FALSE` and `0` `STALE` (`42` `UNVERIFIABLE` physical figures stay labeled `NOT_TESTED` until calipers/bench tests). | Replace `UNVERIFIABLE` physical estimates with `HARDWARE_TESTED` captures and calipers ([`Decision 3`](../owner-decisions.md)). |
| 9 | **Licensing** | **3 / 5** | Explicit owner license approval, clean SPDX separation between purely original first-party files and upstream-coupled wrappers, and zero GPL/Non-Commercial conflicts across release profiles. Rated **`3/5`**: compatibility matrix & `mit`-only release fixed, **but** unapproved `addcc07` stamped `MIT OR GPL-2.0-or-later` across `19` GPL/non-commercial core files; awaits **`Decision 9`** (staged unmerged on `license/spdx-review`: `b6b195e`, `3fd5c05`, `d2a9d43`) and `Decision 4`. | Review & approve **Decision 9** (merge `license/spdx-review`) and long-term core strategy (**Decision 4**). |
| 10 | **AI Environment** | **4 / 5** | Lean single-source `AGENTS.md`, automated Hard-Stop protocol, compact `STATE.md`/`CODEMAP.md`, generated pin-map SSOT, zero hype naming, and clean verification SNR. Rated **`4/5`**: `AGENTS.md` (`~1,536` tokens), Hard-Stop Protocol, generated `CODEMAP.md` (`89` lines), and cross-file pin parity tests work well, while full `pinmap.yaml` codegen (frozen by hard stop) and deleting `test_tier1/2` / `test_apple_nintendo_ui` await `Decision 8`. | Approve `Decision 8` weak-test cleanup and optionally unify `pinmap.yaml` post-bring-up. |
| 11 | **Dev Environment** | **4 / 5** | Cross-platform `doctor`/hooks/VSCode/devcontainer/clangd **plus** hashed Python lockfile (`--require-hashes`), `-ffile-prefix-map` reproducible builds, and CI-verified Windows parity. Rated **`4/5`**: `tasks.py doctor`, `.devcontainer`, `.vscode`, `.clangd`, `LF` `.gitattributes`, `ADR 0016`, and hooks work locally, missing hashed pip lockfile and Windows CI verification. | Add pip hash lockfile (`pip-compile`/`uv`), `python scripts/install_git_hooks.py` onboarding, and Windows CI check. |

---

## 3. 30 / 60 / 90-Day Roadmap

| Window | Milestone Goal | Concrete Actions | Exit Criteria |
| :--- | :--- | :--- | :--- |
| **Days 1–30** | **Safe Bench Bring-Up & Owner Sign-Offs** | Wire USB-powered Stage 1 (`DevKitC-1` + ST7789 + SD + `10 kΩ` CS pull-ups + `8` buttons on perfboard/breadboard), flash `firmware/bringup/bringup.ino`, test `40` vs `80 MHz` SPI stability, record initial voltages & serial logs in [`docs/hardware_measurements.md`](../hardware_measurements.md), and approve Decisions `1–9` in [`docs/owner-decisions.md`](../owner-decisions.md) (including merging `license/spdx-review`). | LCD color fill, SD mount, and all `8` buttons marked `HARDWARE_TESTED`; Decisions `1–9` resolved. |
| **Days 31–60** | **Physical Caliper Survey & Enclosure Fit** | Measure finished perfboard thickness, header protrusion, `2×AA` / `3×AA` holder clearance, and display active window with calipers; log in `hardware_measurements.md`. Apply approved **Decision 3** to update `hardware/layout.json` & `generate_bmo_enclosure.py`, export STLs, and test-fit 3D shells. | Printed shells close cleanly around wired perfboard with zero Z-pinching or short circuits. |
| **Days 61–90** | **AA NiMH Power Stage & Real Core Measurements** | Assemble approved AA boost converter + switch + `1.5 A` polyfuse circuit (**Decision 1**), measure battery sag/runtime/temperature, flash main firmware, and capture 600-frame `p50/p95/p99` + Step `9` relocated-table telemetry into [`docs/compatibility.md`](../compatibility.md). | Battery-powered handheld runs stable `>= 2 h` with real FPS, heap/PSRAM, and thermal data (`HARDWARE_TESTED`). |
| **Day 90+** | **Post-Bring-Up Code, Test & CI Hardening** | Execute **Decision 8** (retire dead `unit_tests.cpp` & `7` weak Python regex tests), **Decision 6** (`fram_save.*` removal / `GPIO 2` PWM), split `bmo_app.cpp`/`bmo_face.cpp`/Core 0-1 `s_*` state, add Windows CI + coverage + SAST + CycloneDX SBOM + conformance ROMs. | First-party C++ files `<= 1,000` lines, multi-OS CI with coverage/SBOM passing, and zero deferred debt. |
