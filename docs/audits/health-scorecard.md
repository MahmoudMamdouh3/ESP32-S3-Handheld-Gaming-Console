# Repository Health Scorecard (`techlead/audit-2`)

- **Baseline Commit:** `d8d4b5ef5cb314072652992805fff600961b2920` (`main` @ `2026-11-15`)
- **Audit Branch:** `techlead/audit-2` (`25` commits ahead of baseline)
- **Physical Hardware Measurements (`HARDWARE_TESTED`):** `0` (all electrical, thermal, SPI signal integrity, and real-device frame-time metrics remain `NOT_TESTED` until physical bring-up per [`docs/guides/bringup.md`](../guides/bringup.md)).

---

## 1. Before vs. After Quantitative Metrics

| Metric | Baseline (`d8d4b5e`) | Post-Remediation (`techlead/audit-2`) | Delta / Outcome | Evidence Level |
| :--- | :--- | :--- | :--- | :--- |
| **`default` Profile App Image (`app0 = 8,388,608 B`)** | `3,291,744 B` (`39.2%`) | `3,292,512 B` (`39.2%`, `5,096,096 B` headroom) | `+768 B` (`M6502` cycle table in `.flash.rodata` + crash breadcrumb) | `TARGET_COMPILED` |
| **`default` Profile Static DRAM (`327,680 B`)** | `192,280 B` (`58.7%`) | `185,272 B` (`56.5%`, `142,408 B` free) | **`-7,008 B` (`-6.84 KiB` freed for stacks/DMA)** | `TARGET_COMPILED` |
| **`default` Profile IRAM Used** | `87,040 B` | `87,040 B` | `0 B` | `TARGET_COMPILED` |
| **`no-doom` Profile App Image / Static DRAM** | `3,015,616 B` / `101,960 B` | `3,016,384 B` (`36.0%`) / `94,952 B` (`29.0%`) | **`-7,008 B` Static DRAM** | `TARGET_COMPILED` |
| **`mit` Profile App Image / Static DRAM** | `893,664 B` / `75,840 B` | `893,408 B` (`10.7%`) / `69,856 B` (`21.3%`) | **`-5,984 B` Static DRAM** | `TARGET_COMPILED` |
| **Standalone Hardware Bring-up Sketch (`bringup.ino`)** | None | `390,724 B` Flash / `24,600 B` RAM | Isolated LCD, SD & GPIO tester (`esp32s3`) | `TARGET_COMPILED` |
| **First-Party Compiler Warnings (`-Wall -Wextra -Wshadow`)** | `0` across 3 profiles | `0` across all `7` build configs + `bringup.ino` | Zero first-party warnings under strict checks | `TARGET_COMPILED` |
| **Fast Pre-Commit Gate (`python tasks.py check --fast`)** | None (`check` took `82.56 s`) | **`22.64 s`** (`12` Guardian + `89` fast host tests) | **`3.6x` faster inner-loop check** (`tasks.py doctor` `0.60 s`, `format --check` `0.81 s`) | `HOST_TESTED` |
| **Full Gate & Test Run (`check` / `test`)** | `82.56 s` / `631.71 s` | `84.45 s` (`check`: `164` Py + `12` Guardian + `25` CTest) / `540.17 s` (`test`: `185` Py/WASI + `12` Guardian + `25` CTest + `451.7 s` `validate_repo`) | `100%` passing (`10/10` mutations killed) | `HOST_TESTED` |
| **Fresh-Clone Check (Paths w/ & w/o Spaces)** | Verified (`0.78–0.80 s` clone) | `0.88–0.97 s` clone; `check --fast` `27.32–30.64 s` (`rc=0`, `skipped=10` tools absent from git) | Verified under both `"Bmo Fresh Space"` and `bmo_fresh_nospace` | `HOST_TESTED` |
| **Doc Claims Audited ([`claims-inventory.md`](claims-inventory.md))** | Unreconciled (`14` claims sampled: `1` `TRUE`, `1` `TRUE/STALE`, `7` `STALE`, `4` `FALSE`/`FALSE+STALE`, `1` `UNVERIFIABLE`) | `14` reconciled across ADRs `0003/0011/0012/0013`, `ROADMAP.md`, `architecture.md`, `THIRD_PARTY_NOTICES.md` | Remaining hardware-dependent claims explicitly marked `NOT_TESTED` | `HOST_TESTED` |
| **First-Party Code Files `> 1,000` Lines** | `6` files (`sd_catalog.cpp` `1,004`; `bmo_app.cpp` `1,197`; `bmo_face.cpp` `1,145`; `simulator.js`, `bmo_3d_console.js`, `bmo_internals_3d.js`) | `5` deferred (`bmo_app.cpp` `1,197`, `bmo_face.cpp` `1,145`, `3` simulator JS files); `sd_catalog.cpp` reduced to `919` | Extracted `popular_games_table.h`; larger C++/JS splits deferred until post-bring-up | `HOST_TESTED` |
| **Dead Build Files, One-Off Scripts & SPDX Hygiene** | `19` unreferenced files; `0` SPDX headers | `0` unreferenced files (`941509c`, `0b22efc`); **`131` files with SPDX headers** | Clean tree; `0` blobs `>= 1 MiB` / `0` secrets across history | `HOST_TESTED` |
| **GitHub Actions Pinning & Release Artifacts** | Tag refs (`@v4`); published non-commercial/GPL-3 binaries | 40-char commit SHAs; uploads only `mit` binary | Supply-chain & GPL/NC compliance enforced in CI/Release | `HOST_TESTED` |

---

## 2. 11-Area Health Ratings (`1–5` Scale)

| # | Area | Rating | Evidence & Strengths | Top Remaining Risk / Action |
| :- | :--- | :---: | :--- | :--- |
| 1 | **Hardware Validation** | **1 / 5** | `bringup.ino` (`TARGET_COMPILED`), `pins_check.h`, `layout.json` 3D collision suite, and safety/BOM guides exist; however **`0` physical `HARDWARE_TESTED` measurements** exist. | Execute [`docs/guides/bringup.md`](../guides/bringup.md) Phases 1–4 and log measurements in [`docs/hardware_measurements.md`](../hardware_measurements.md). |
| 2 | **Repository Structure** | **4 / 5** | Clean top-level separation, `19` dead files removed, `.editorconfig`/`.gitattributes`, `hardware/layout.json` single source of truth. Optional simulator/3D tooling (`~11,000` lines) still sits alongside firmware. | Freeze simulator/3D enhancements and review YAGNI trimming in [Decision 8](../owner-decisions.md). |
| 3 | **Naming** | **4 / 5** | `convert_covers.py` replaces misleading `fetch_covers.py`, units codified in [`docs/conventions.md`](../conventions.md), metric `cm` enforced across docs. Sketch name `BmoGameboy` still carries trademarks. | Decide sketch/repo naming in [Decision 7](../owner-decisions.md). |
| 4 | **Code Structure** | **4 / 5** | `SpiArbiter::ScopedLock` + PSRAM allocators + `sd_catalog.cpp` (`919` lines) + `-7,008 B` DRAM freed + `esp_reset_reason()` breadcrumbs. `bmo_app.cpp` (`1,197`) and `bmo_face.cpp` (`1,145`) still exceed `1,000` lines. | Refactor `bmo_app.cpp` and `bmo_face.cpp` cleanly after hardware bring-up establishes a physical baseline. |
| 5 | **Tests** | **4 / 5** | `25` native C++17 `CTest` targets (with ASan/UBSan in CI), `15` WASI core smoke runs (`11` engines), `10/10` mutations killed, and security/CAD tests pass. However `7` legacy Python files (`63` tests) are source-regex/inline-toy checks. | Retire the `7` weak Python test files + dead `unit_tests.cpp` post-bring-up ([Decision 8](../owner-decisions.md); [`ai-and-tooling-review.md`](ai-and-tooling-review.md)). |
| 6 | **CI & Release** | **5 / 5** | Every GitHub Action pinned to 40-char SHAs, binary releases restricted to `mit`, 3-profile strict compile + budget gate, ASan/UBSan + WASI tests, working-cache paths fixed. | Keep binary releases restricted to `mit` unless non-commercial cores are removed ([Decision 4](../owner-decisions.md)). |
| 7 | **Performance Evidence** | **2 / 5** | Pure SPI wire math (`80` vs `40 MHz`) and Core 0 async architecture accurately modeled (`bus-calc`, [ADR 0014](../decisions/0014-display-spi-clock.md)); zero real SPI clock stability or FPS captures measured yet. | Verify `40` vs `80 MHz` ST7789 stability on perfboard wires and pick boot default ([Decision 2](../owner-decisions.md)). |
| 8 | **Documentation Truthfulness** | **4 / 5** | All `14` claims in [`claims-inventory.md`](claims-inventory.md) audited, ADRs `0003/0011/0012/0013` fixed, `architecture.md` verified line-for-line against C++, `owner-guide.md` & `owner-decisions.md` clear in `cm`. Enclosure CAD waits on physical calipers. | Run physical caliper survey and resolve [`Decision 3`](../owner-decisions.md) before 3D printing. |
| 9 | **Licensing** | **4 / 5** | Complete 16-component matrix in [`THIRD_PARTY_NOTICES.md`](../../THIRD_PARTY_NOTICES.md), `131` files have SPDX headers, public CI artifacts strictly `mit`. Upstream source still mixes GPL and non-commercial cores (`Snes9x`, `SMS Plus`, `Gwenesis`). | Owner chooses long-term policy in [`Decision 4`](../owner-decisions.md). |
| 10 | **AI Environment** | **5 / 5** | Lean root [`AGENTS.md`](../../AGENTS.md) (~`1,536` est. tokens) loaded automatically by Antigravity, `CLAUDE.md`/`.codex` are thin non-contradictory pointers, concise `STATE.md`/`CODEMAP.md`, allow/deny list documented. | Configure recommended Antigravity terminal deny rules for `upload`/`flash`/`esptool`. |
| 11 | **Dev Environment** | **5 / 5** | Cross-platform `python tasks.py doctor`, `check --fast` (`22.64 s`), `format --check` (`0.81 s`), `.devcontainer`, `.vscode`, `.clangd`, and [`.githooks`](../../.githooks/pre-commit) all working on Windows & Linux. | Run `python scripts/install_git_hooks.py` once per fresh clone. |

---

## 3. 30 / 60 / 90-Day Roadmap

| Window | Milestone Goal | Concrete Actions | Exit Criteria |
| :--- | :--- | :--- | :--- |
| **Days 1–30** | **Safe Bench Bring-Up & Owner Sign-Offs** | Wire USB-powered Stage 1 (`DevKitC-1` + ST7789 + SD + `10 kΩ` CS pull-ups + `8` buttons on breadboard/perfboard), flash `firmware/bringup/bringup.ino`, test `40` vs `80 MHz` SPI stability, and record initial voltages & serial logs in [`docs/hardware_measurements.md`](../hardware_measurements.md). Review & check off Decisions `1–8` in [`docs/owner-decisions.md`](../owner-decisions.md). | LCD color fill, SD mount, and all `8` buttons marked `HARDWARE_TESTED`; Decisions `1–8` signed off. |
| **Days 31–60** | **Physical Caliper Survey & Enclosure Fit** | Measure finished perfboard thickness, header protrusion, `2×AA` / `3×AA` holder clearance, and display active window with digital calipers; log in `hardware_measurements.md`. Apply approved **Decision 3** to update `hardware/layout.json` & `generate_bmo_enclosure.py`, export STLs, and 3D-print test shells. | Printed front/back shells close cleanly around wired perfboard with zero Z-pinching or short circuits. |
| **Days 61–90** | **AA NiMH Power Stage & Real Core Measurements** | Assemble approved AA boost converter + switch + `1.5 A` polyfuse circuit (**Decision 1**), measure battery sag/runtime/temperature on real hardware, flash main `BmoGameboy` firmware, and benchmark real FPS across `GB`, `GBC`, `NES`, `DOOM`, and Experimental cores into [`docs/compatibility.md`](../compatibility.md). | Battery-powered handheld runs stable `>= 2 h` with real FPS & thermal data (`HARDWARE_TESTED`). |
| **Day 90+** | **Post-Bring-Up Code & Tooling Cleanups** | Execute **Decision 8** (retire dead `unit_tests.cpp` and `7` low-signal Python source-regex tests), **Decision 6** (remove unused `fram_save.*` / add optional `GPIO 2` PWM backlight), and split `bmo_app.cpp` (`1,197` lines) & `bmo_face.cpp` (`1,145` lines) below `1,000` lines using real hardware regression tests. | All first-party files `<= 1,000` lines and zero dormant/noisy legacy files remaining. |
