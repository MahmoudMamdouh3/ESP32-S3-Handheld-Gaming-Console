# Full-Scope Tech-Lead Audit Plan (`techlead/audit-2`)

- **Baseline Commit:** `d8d4b5ef5cb314072652992805fff600961b2920` (`main`)
- **Audit Branch:** `techlead/audit-2`
- **Date Started:** `2026-11-15`
- **Hardware Validation Status:** `NOT_TESTED` on physical device (`0` `HARDWARE_TESTED` records in [docs/hardware_measurements.md](../hardware_measurements.md))

---

## 1. Phase 0 Baseline & Repository Inventory

### 1.1 Git & Object Store Baseline

| Metric | Value | Evidence (`HOST_TESTED`) |
| :--- | :--- | :--- |
| `HEAD` | `d8d4b5ef5cb314072652992805fff600961b2920` | `git rev-parse HEAD` |
| Branches | `main`, `audit/tech-lead`, `techlead/audit-2` | `git branch -a` |
| Tags | `v0.1-display-buttons` | `git tag -l` |
| Remote `origin` | `https://github.com/MahmoudMamdouh3/ESP32-S3-Handheld-Gaming-Console.git` | `git remote -v` |
| Loose objects | `382` objects (`743.03 KiB`) | `git count-objects -vH` |
| Packed objects | `3,215` objects in `1` pack (`4.07 MiB`) | `git count-objects -vH` |
| Blobs `>= 1 MiB` across all refs | **`0`** (`main` max `618,001 B`, `audit/tech-lead` max `618,001 B`, `v0.1-display-buttons` max `1,703 B`) | `git rev-list --objects --all \| git cat-file --batch-check` |
| Secret scan (`HEAD` & all history) | `0` API keys, tokens, or private keys; `1` local path literal in [docs/hardware-validation-kit.md](../hardware-validation-kit.md) (`e:\BMO Gameboy`); `172` historical Windows path strings in commit history | Regex + full `git log --all -p` scan |

### 1.2 Top 20 Largest Blobs Across All Git History (`--all`)

| Rank | Size (Bytes) | Blob SHA | Path |
| ---: | ---: | :--- | :--- |
| 1 | `618,001` | `642c596aa5d1` | `firmware/BmoGameboy/src/third_party/stella2014/src/DefProps.h` |
| 2 | `616,280` | `e26f0b8009f0` | `simulator/bmo_enclosure_meshes.js` |
| 3 | `616,277` | `be35bcb6ab18` | `tools/bmo_simulator/bmo_enclosure_meshes.js` (historical path) |
| 4 | `603,445` | `b14f47c0908b` | `simulator/three.min.js` |
| 5 | `599,599` | `00191a133b8c` | `firmware/BmoGameboy/src/third_party/gwenesis/cpus/M68K/m68ki_instruction_jump_table_part1.h` |
| 6 | `586,710` | `54fb0dfee520` | `firmware/BmoGameboy/src/third_party/gwenesis/cpus/M68K/m68ki_instruction_jump_table_part2.h` |
| 7 | `515,227` | `a1c99b477783` | `firmware/BmoGameboy/src/third_party/gwenesis/cpus/M68K/m68kops.h` |
| 8 | `441,193` | `9251eaa7bd36` | `firmware/BmoGameboy/src/third_party/gwenesis/cpus/M68K/m68ki_cycles.h` |
| 9 | `401,435` | `800a846017db` | `firmware/BmoGameboy/src/third_party/smsplus/lut.h` |
| 10 | `272,061` | `59c41c3d6184` | `firmware/BmoGameboy/src/third_party/walnut_cgb/walnut_cgb.h` |
| 11–18 | `269,781–271,465` | `9757ef70..a1220279` | Historical revisions of `walnut_cgb.h` |
| 19 | `260,088` | `ee8cf33d2937` | `firmware/BmoGameboy/src/third_party/fake08/src/lodepng.cpp` |
| 20 | `222,307` | `5cb0165397ec` | `firmware/BmoGameboy/src/third_party/race/tlcs900h.c` |

### 1.3 Tracked Files & Bytes by Directory and Category (`HEAD`)

- **Total Tracked Files:** `1,017` files (`14,833,377 B` / `367,096` text lines)
  - **First-party code & docs:** `312` files (`43,539` lines; `0` first-party `TODO`/`FIXME` comments)
  - **Vendored third-party code (`src/third_party/` + Three.js/`OrbitControls.js`/WASM):** `704` files (`324,551` lines; `3` upstream `// TODO touch` comments in `simulator/OrbitControls.js`)
  - **Generated tracked bundles (`simulator/bmo_enclosure_meshes.js`):** `1` file (`616,280 B`, `6` lines)

| Top-Level Directory | Files | Bytes | Share of Repo Bytes | Lines |
| :--- | ---: | ---: | ---: | ---: |
| `firmware/` | `794` | `12,196,026` | `82.22%` | `338,827` |
| `simulator/` | `37` | `1,613,966` | `10.88%` | `11,030` |
| `tests/` | `76` | `317,876` | `2.14%` | `7,835` |
| `docs/` | `36` | `299,599` | `2.02%` | `1,038` |
| `scripts/` | `21` | `129,269` | `0.87%` | `3,397` |
| `(root)` | `20` | `118,259` | `0.80%` | `2,055` |
| `tools/` | `14` | `105,180` | `0.71%` | `2,578` |
| `hardware/` | `4` | `33,437` | `0.23%` | `743` |
| `.github/` | `8` | `11,595` | `0.08%` | `348` |
| `.githooks/`, `design/`, `resources/`, `.claude/`, `.codex/` | `7` | `8,170` | `0.06%` | `245` |

### 1.4 Candidate Dead or Unreferenced Files Identified in Phase 0

1. `firmware/BmoGameboy/src/storage/fram_save.cpp` + `fram_save.h` — compiled into the firmware image by Arduino's recursive `src/` glob, but `FramSave::begin()`, `readSaveData()`, and `writeSaveData()` have zero call sites anywhere in the repository (`HOST_TESTED` via grep + ELF check).
2. `firmware/BmoGameboy/src/tests/unit_tests.cpp` + `unit_tests.h` — compiled into `src/` by Arduino (`UnitTests::runAll()` has zero call sites in `BmoGameboy.ino` or `src/app/`).
3. `scripts/purge_history_plan.sh` + `scripts/purge_history_paths.txt` — one-off historical `git filter-repo` artifacts from a completed purge.
4. `scripts/audit_all_games.py`, `scripts/convert_roms.py`, `scripts/extract_atari.py`, `scripts/fix_genesis_smd.py`, `scripts/process_games.py` — one-off local ROM migration scripts referencing user-local directories (`games/`, `roms/`).
5. `scripts/test_runner.py` — legacy serial hardware test runner superseded by `tasks.py`.
6. `scripts/deploy_to_sd.py` vs `scripts/deploy_sd_complete.py` — duplicate SD deployment scripts.
7. `firmware/BmoGameboy/src/third_party/doom/src/Makefile*` (8 files), `doomgeneric.vcxproj*` (2 files), `stella2014/src/M6502.m4` (1 file) — 11 dead upstream build files inside vendored emulator cores.

### 1.5 Baseline Gate & Fresh-Clone Timings (`HOST_TESTED` / `TARGET_COMPILED`)

| Command | Result | Duration | Notes |
| :--- | :--- | ---: | :--- |
| `python tasks.py format --check` | `PASS (rc=0)` | `1.87 s` | `124` first-party C/C++ files clean |
| `python tasks.py size --check-budgets` | `PASS (rc=0)` | `0.72 s` | Flash `3,291,744 / 8,388,608 B` (`39.2%`), Static DRAM `192,280 / 327,680 B` (`58.7%`), IRAM `87,040 B` |
| `python tasks.py check` | `PASS (rc=0)` | `82.56 s` | Exceeds 60 s fast-gate target because `_run_host_ctest` invokes CMake + MSVC build every run |
| `python tasks.py test` | `PASS (rc=0)` | `631.71 s` | `25` CTest targets + `177` Python tests (`tests/`) + `12` Guardian tests + `validate_repo.py` |
| `python -m tools.guardian audit` | `PASS (rc=0)` | `2.41 s` | `0` Critical, `13` Warnings |
| Fresh clone (no spaces in path) + `check --no-tests` | `PASS (rc=0)` | `0.78 s` + `15.28 s` | Note: `git clone --local` fails across Windows drive letters (`E:` -> `C:`); requires `--no-hardlinks` |
| Fresh clone (with spaces in path) + `check --no-tests` | `PASS (rc=0)` | `0.80 s` + `8.38 s` | Passed in path with spaces, though ELF reports embed raw workspace paths with spaces |

### 1.6 Baseline Static Memory Consumers (`TARGET_COMPILED` ELF Analysis)

- **App Image (`BmoGameboy.ino.bin`):** `3,291,744 B` (`39.2%` of `8,388,608 B` `app0`, `5,096,864 B` headroom)
- **Static DRAM (`.dram0.data` `86,277 B` + `.dram0.bss` `106,000 B`):** `192,280 B` (`58.7%` of `327,680 B`)
  - Top first-party static DRAM consumers:
    - `s_recentHistory` (`games_menu.cpp`): `2,560 B` in `.bss`
    - `s_framePercentileSamples` (`bmo_app.cpp`): `2,400 B` in `.bss`
    - `s_lastColors` (`bmo_face.cpp`): `1,024 B` in `.bss`
    - `DMG_ON_GBC_PAL_256` (`emu_walnut.cpp`, `512 B`) + `PAL_256` (`emu_peanut.cpp`, `512 B`) + `rowBuffer[240]` (`960 B` across both)
  - Top vendored static DRAM consumers:
    - DOOM `states` (`27,076 B`), `mobjinfo` (`12,604 B`), `S_sfx` (`5,232 B`), `extra_defaults_list` (`2,856 B`), `doom_defaults_list` (`1,824 B`), `S_music` (`1,088 B`) in `.dram0.data`
    - Stella 2014 `M6502::ourInstructionCycleTable` (`1,024 B`) and six `Console::our*Palette` tables (`6,144 B`) in `.dram0.data`
    - FAKE-08 `charset::to_utf8` (`2,048 B`), `charset::to_utf32` (`2,048 B`), `defaultFontBinaryData` (`2,048 B`), `utf32_storage` (`1,056 B`)
- **IRAM (`.iram0.vectors` + `.iram0.text`):** `87,040 B`

---

## 2. Risk-Ranked Remediation Order (Phases 1–10)

All work proceeds sequentially through Phases 1–10 on `techlead/audit-2`, running the verification gate after every phase and logging every decision requiring human approval in `docs/owner-decisions.md`:

1. **Phase 1 — Truth Reconciliation (`P0`/`P1`):**
   - Audit and reconcile every factual/numeric claim across `README.md`, `ROADMAP.md`, `AGENTS.md`, `firmware/AGENTS.md`, `CHANGELOG.md`, `docs/`, and ADRs `0001–0014`.
   - Fix `ROADMAP.md` (C3: Core 0 async GB/GBC streaming and `BSS2` save states are already implemented).
   - Reconcile ADR 0003, ADR 0011 (`BMOIDX03` is already implemented, not proposed `BMOIDX02`), and ADR 0012 (`BSS2` is already implemented, not proposed `BSS1`).
   - Consolidate duplicate [docs/performance.md](../performance.md) and [docs/performance_and_benchmarking.md](../performance_and_benchmarking.md) (C6).
   - Document `hardware/diagram.json` Wokwi limitations (C7: `wokwi-ili9341` stand-in and unsoldered `X/Y/L/R` buttons).
   - Remove hardcoded local workspace path in `docs/hardware-validation-kit.md` (L5) and update `AGENTS.md` to reference cross-platform `python tasks.py` commands alongside PowerShell examples (C11).

2. **Phase 2 — Licensing, Provenance & Legal Hygiene (`P0`):**
   - Produce the complete component-by-component license compatibility matrix (`GPL-2.0-only` vs `GPL-2.0-or-later` vs `GPL-3.0` in `gwenesis` vs Snes9x/MAME Z80 non-commercial restrictions).
   - Correct `THIRD_PARTY_NOTICES.md`, `README.md`, `CONTRIBUTING.md`, and `ADR 0008` so they do not falsely claim a redistributable `GPL-2.0-or-later` combined binary when `gwenesis` (GPL-3.0) and `snes9x` / MAME Z80 (non-commercial) are linked in the `default` profile.
   - Stop auto-publishing non-redistributable `default` / `no-doom` combined binaries on public GitHub Releases in `.github/workflows/release.yml` and `.github/workflows/ci.yml` (restrict binary release artifacts to the license-clean `mit` profile until the owner decides on L1 in `docs/owner-decisions.md`).
   - Add SPDX license headers to first-party source files.

3. **Phase 3 — Hardware, Electrical, Power, Pins, Enclosure & Layout (`P0`/`P1`):**
   - Write beginner-friendly `docs/guides/bringup.md` (5-step pass/fail checklist) and `docs/guides/safety.md`.
   - Create compile-only minimal hardware bring-up sketch under `firmware/bringup/bringup.ino` (color bars, button echo, SD probe, PSRAM memtest, heap report).
   - Create single source of truth `hardware/layout.json` (in centimetres at the doc boundary, matching the owner's reference layout with collision analysis, switch spacing check, 0.254 cm perfboard grid snap, top/side SVG diagrams, 31×46 hole perfboard placement map, and CSV wire list).
   - Write `docs/bom.md` and update proposed ADRs (`0006` AA NiMH power budget, `0007` FRAM vs UART0 GPIO 43/44 collision, `0013` partition/coredump proposal, `0014` ST7789 80 MHz vs 40 MHz SPI wire budget & series termination).
   - Add compile-time `static_assert` pin-collision / reserved-GPIO guards in `firmware/BmoGameboy/src/hal/pins_check.h` and pin-value snapshot tests.

4. **Phase 4 — Repository Structure, Naming & Hygiene (`P1`/`P2`):**
   - Write `docs/conventions.md`, `.editorconfig`, and `.git-blame-ignore-revs`.
   - Remove dead upstream build leftovers (`.vcxproj`, `.filters`, `Makefile.*`, `.m4`) from `firmware/BmoGameboy/src/third_party/`.
   - Remove one-off historical scripts (`purge_history_plan.sh`, `purge_history_paths.txt`, `fix_genesis_smd.py`, `extract_atari.py`, `process_games.py`, `convert_roms.py`, `audit_all_games.py`, `test_runner.py`), merge `deploy_to_sd.py` + `deploy_sd_complete.py`, and alias `fetch_covers.py` -> `convert_covers.py`.
   - Gate or relocate uncalled `firmware/BmoGameboy/src/tests/unit_tests.cpp` and `firmware/BmoGameboy/src/storage/fram_save.cpp` so dead code is not linked into production firmware.
   - Pin GitHub Actions to commit SHAs, fix stale `hardware/enclosure/requirements.txt` cache paths in `ci.yml`, add `.github/dependabot.yml` and `.github/CODEOWNERS`, and enforce Conventional Commits in `.githooks/commit-msg`.

5. **Phase 5 — Firmware Architecture & Code Quality (`P1`/`P2`):**
   - Reduce static DRAM footprint (move `s_framePercentileSamples`, `s_recentHistory`, `s_lastColors`, and palette scratch buffers to PSRAM or `const` flash `.rodata`; mark read-only Stella/DOOM tables `const` where safe and verified by tests + target compile).
   - Extract `popular_games_table.h` data table from `sd_catalog.cpp` (`isPopularGame`).
   - Add boot reset-reason logging (`esp_reset_reason()`) and RTC/NVS crash breadcrumb reporting on boot.
   - Write `docs/reference/controls.md`.
   - Document Dual-Core (`Core 0` vs `Core 1`) shared state ownership and synchronization primitives.

6. **Phase 6 — Performance & Analytical Honesty (`P1`/`P2`):**
   - Reconcile [docs/compatibility.md](../compatibility.md) so unmeasured 16-bit/handheld cores (`SNES`, `Genesis`, `NGP`, `WonderSwan`, `PC Engine`, `Lynx`) are explicitly labeled `Experimental (NOT_TESTED on hardware)`.
   - Document display DMA vs `SPI.writeBytes` evaluation and close the open LVGL/UI-framework question via ADR 0015.

7. **Phase 7 — Tests, CI & Developer Environment (`P1`/`P2`):**
   - Add `python tasks.py doctor` environment diagnostic subcommand.
   - Make `python tasks.py check --fast` run in `< 30 s` for pre-commit hooks.
   - Add `.devcontainer/devcontainer.json` and `.vscode/settings.json` + `.clangd` configuration.
   - Write build-system evaluation ADR (`arduino-cli` vs `PlatformIO` vs `ESP-IDF`).

8. **Phase 8 — AI Engineering Environment (`P1`/`P2`):**
   - Update `AGENTS.md` with the Hard-Stop Change Protocol, Definition of Done, file/function size limits, and cross-platform `python tasks.py` commands.
   - Create `docs/ai/STATE.md` (`<= 60` lines) and auto-generated `docs/ai/CODEMAP.md` (`<= 200` lines).
   - Document YAGNI & check signal-to-noise audit.

9. **Phase 9 — Owner Documentation (`P0`/`P1`):**
   - Write plain-English `docs/guides/owner-guide.md` (with centimetre dimensions, beginner glossary, SD card setup, flashing guide, and troubleshooting table).
   - Update [README.md](../../README.md) and [docs/architecture.md](../architecture.md).

10. **Phase 10 — Final Verification, Scorecard & Handoff (`P0`):**
    - Run full gate (`check`, `test`, `build --profile all --warnings strict --fail-on-warnings`, `size --check-budgets`, `format --check`).
    - Complete `docs/owner-decisions.md`, `docs/audits/health-scorecard.md`, and `docs/audits/pr-description.md`.
