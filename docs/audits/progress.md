# Audit & Remediation Progress Tracker (`techlead/audit-2`)

- **Current Phase:** Phase 5 — Firmware Architecture & Code Quality (Phases 0–4 complete)
- **Baseline Commit:** `d8d4b5ef5cb314072652992805fff600961b2920`

## Completed Phases

- [x] **Phase 0 — Baseline and Inventory (`HOST_TESTED` / `TARGET_COMPILED`):**
  - Created branch `techlead/audit-2`.
  - Recorded git object counts (`382` loose / `3,215` packed, `0` blobs `>= 1 MiB` across all refs), top 20 blobs across all history, directory/extension breakdown (`1,017` files, `14,833,377 B`), first-party vs vendored LOC (`312` first-party files / `43,539` lines vs `704` vendored files / `324,551` lines), secret scan (`0` secrets), and candidate dead files.
  - Timed baseline gates: `format --check` (`1.87 s`), `size --check-budgets` (`0.72 s`), `check` (`82.56 s`), `test` (`631.71 s`), fresh clones with and without spaces (`0.78–0.80 s` clone + `8.38–15.28 s` check).
  - Authored [docs/audits/audit-plan.md](audit-plan.md) and [docs/audits/findings-register.md](findings-register.md).
- [x] **Phase 1 — Truth Reconciliation (`C1–C11`) (`HOST_TESTED`):**
  - Authored [docs/audits/claims-inventory.md](claims-inventory.md) verifying claims across README, docs, ADRs, AGENTS, CHANGELOG, and ROADMAP.
  - Reconciled [ROADMAP.md](../../ROADMAP.md), [ADR 0003](../decisions/0003-rom-keys-and-index-v2.md), [ADR 0011](../decisions/0011-index-path-storage.md), [ADR 0012](../decisions/0012-save-state-schema.md), [ADR 0013](../decisions/0013-partition-layout.md), [docs/performance.md](../performance.md), [docs/hardware.md](../hardware.md), [hardware/diagram.json](../../hardware/diagram.json), and [scripts/convert_covers.py](../../scripts/convert_covers.py).
- [x] **Phase 2 — Licensing and Legal Hygiene (`L1–L5`) (`HOST_TESTED`):**
  - Built the full license compatibility matrix in [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) and [ADR 0008](../decisions/0008-rom-policy-and-licensing.md).
  - Restricted [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) and [`.github/workflows/release.yml`](../../.github/workflows/release.yml) binary artifact publishing to the license-clean `mit` profile while keeping `default` and `no-doom` compile and size gates active; pinned all GitHub Actions to 40-char commit SHAs.
  - Added `SPDX-License-Identifier: MIT OR GPL-2.0-or-later` headers to 89 first-party C/C++ files and 35 Python files (`addcc07`).
- [x] **Phase 3 — Hardware, Electrical, Power, Pins, Enclosure & Layout (`H1–H6`) (`TARGET_COMPILED` / `HOST_TESTED`):**
  - Added [`firmware/bringup/bringup.ino`](../../firmware/bringup/bringup.ino) (`TARGET_COMPILED`: `390,724 B` flash, `24,600 B` static RAM), [`docs/guides/bringup.md`](../guides/bringup.md), and [`docs/guides/safety.md`](../guides/safety.md).
  - Added compile-time GPIO uniqueness and strapping/PSRAM `static_assert` validation in [`firmware/BmoGameboy/src/hal/pins_check.h`](../../firmware/BmoGameboy/src/hal/pins_check.h) and host snapshot tests in [`tests/test_pin_snapshot.py`](../../tests/test_pin_snapshot.py).
  - Rewrote [ADR 0006](../decisions/0006-portable-power.md) (2×AA vs 3×AA NiMH power & protection), [ADR 0007](../decisions/0007-fram-wiring.md) (`GPIO 43/44` UART0 collision), [ADR 0014](../decisions/0014-display-spi-clock.md) (`80 MHz` vs `40 MHz` SPI wire budget & perfboard signal integrity), [`docs/wiring/02-sd-card.md`](../wiring/02-sd-card.md) (`10 kΩ` CS pull-ups & `GPIO 15` 3.3V check), and [`docs/bom.md`](../bom.md).
  - Created [`hardware/layout.json`](../../hardware/layout.json), [`scripts/generate_hardware_layout_artifacts.py`](../../scripts/generate_hardware_layout_artifacts.py), [`docs/assets/layout_top_view.svg`](../assets/layout_top_view.svg), [`docs/assets/layout_side_section.svg`](../assets/layout_side_section.svg), [`docs/wiring/perfboard-layout.md`](../wiring/perfboard-layout.md), [`docs/wiring/wire_list.csv`](../wiring/wire_list.csv), and [`tests/test_hardware_layout.py`](../../tests/test_hardware_layout.py).

- [x] **Phase 4 — Repo Structure, Naming & Hygiene (`S1–S8`) (`HOST_TESTED`):**
  - Added [`docs/conventions.md`](../conventions.md), [`.editorconfig`](../../.editorconfig), [`.gitattributes`](../../.gitattributes), [`.git-blame-ignore-revs`](../../.git-blame-ignore-revs), [`.github/CODEOWNERS`](../../.github/CODEOWNERS), [`.github/dependabot.yml`](../../.github/dependabot.yml), and Conventional Commit validation in [`.githooks/commit-msg`](../../.githooks/commit-msg).
  - Removed 11 unused upstream build files (`Makefile*`, `.vcxproj*`, `.m4`) in `941509c` and 8 unreferenced one-off migration/purge scripts in `0b22efc`.
  - Gated dormant [`fram_save.cpp`](../../firmware/BmoGameboy/src/storage/fram_save.cpp) (`FEATURE_FRAM`) and [`unit_tests.cpp`](../../firmware/BmoGameboy/src/tests/unit_tests.cpp) (`ENABLE_UNIT_TESTS`) out of default production compilation.

## Upcoming Phases

- [ ] **Phase 5 — Firmware Architecture & Code Quality (`F1–F15`)**
- [ ] **Phase 6 — Performance & Analytical Honesty (`P1–P5`)**
- [ ] **Phase 7 — Tests, CI & Dev Environment (`T1–T6`)**
- [ ] **Phase 8 — AI Engineering Environment (`A1–A7`)**
- [ ] **Phase 9 — Owner Documentation (`owner-guide.md`, `safety.md`, `README.md`, `architecture.md`)**
- [ ] **Phase 10 — Final Verification, Health Scorecard & Handoff**

## Blockers / Owner Decisions Queued

- None blocking execution; items requiring owner sign-off (`C9`, `C10`, `L1`, `L3`, `L4`, `H2`, `H5`, `H6`, project rename, subsystem retention) are being compiled in `docs/owner-decisions.md`.
