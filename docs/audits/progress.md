# Audit & Remediation Progress Tracker (`techlead/audit-2`)

- **Current Phase:** Phase 10 — Final Verification, Health Scorecard & Handoff (Complete)
- **Baseline Commit:** `d8d4b5ef5cb314072652992805fff600961b2920`

## Completed Phases

- [x] **Phase 0 — Baseline and Inventory (`HOST_TESTED` / `TARGET_COMPILED`):**
  - Created branch `techlead/audit-2`.
  - Recorded git object counts (`382` loose / `3,215` packed, `0` blobs `>= 1 MiB` across all refs), secret scan (`0` secrets), and baseline timings (`check` `82.56 s`, `test` `631.71 s`).
  - Authored [docs/audits/audit-plan.md](audit-plan.md) and [docs/audits/findings-register.md](findings-register.md).
- [x] **Phase 1 — Truth Reconciliation (`C1–C11`) (`HOST_TESTED`):**
  - Authored [docs/audits/claims-inventory.md](claims-inventory.md) reconciling `14` claims across docs, ADRs, AGENTS, CHANGELOG, and ROADMAP.
  - Reconciled [ROADMAP.md](../../ROADMAP.md), ADRs [`0003`](../decisions/0003-rom-keys-and-index-v2.md), [`0011`](../decisions/0011-index-path-storage.md), [`0012`](../decisions/0012-save-state-schema.md), [`0013`](../decisions/0013-partition-layout.md), [docs/performance.md](../performance.md), [docs/hardware.md](../hardware.md), [hardware/diagram.json](../../hardware/diagram.json), and [scripts/convert_covers.py](../../scripts/convert_covers.py).
- [x] **Phase 2 — Licensing and Legal Hygiene (`L1–L5`) (`HOST_TESTED`):**
  - Built the 16-component license compatibility matrix in [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) and [ADR 0008](../decisions/0008-rom-policy-and-licensing.md).
  - Restricted CI/Release binary publishing to the license-clean `mit` profile; pinned GitHub Actions to 40-char commit SHAs.
  - Added SPDX headers across `131` first-party files (`addcc07`).
- [x] **Phase 3 — Hardware, Power, Pins, Enclosure, Layout & Simulator Hygiene (`H1–H6`) (`TARGET_COMPILED` / `HOST_TESTED`):**
  - Added [`firmware/bringup/bringup.ino`](../../firmware/bringup/bringup.ino) (`TARGET_COMPILED`: `390,724 B` flash, `24,600 B` RAM), [`docs/guides/bringup.md`](../guides/bringup.md), [`docs/guides/safety.md`](../guides/safety.md), [`docs/bom.md`](../bom.md), and [`pins_check.h`](../../firmware/BmoGameboy/src/hal/pins_check.h).
  - Expanded ADRs [`0006`](../decisions/0006-portable-power.md), [`0007`](../decisions/0007-fram-wiring.md), [`0014`](../decisions/0014-display-spi-clock.md), and [`docs/wiring/02-sd-card.md`](../wiring/02-sd-card.md).
  - Created [`hardware/layout.json`](../../hardware/layout.json) -> SVG top/side views, `31×46` perfboard map, `wire_list.csv`, and fixed 2D/3D simulator layout contrast, ABXY alignment, and NiMH/PSRAM text (`41339aa`, `6bc4a30`).
- [x] **Phase 4 — Repo Structure, Naming & Hygiene (`S1–S8`) (`HOST_TESTED`):**
  - Added [`docs/conventions.md`](../conventions.md), [`.editorconfig`](../../.editorconfig), [`.gitattributes`](../../.gitattributes), [`.github/CODEOWNERS`](../../.github/CODEOWNERS), and Conventional Commit hook [`.githooks/commit-msg`](../../.githooks/commit-msg).
  - Removed `19` dead files (`941509c`, `0b22efc`) and gated dormant `fram_save.cpp` and `unit_tests.cpp`.
- [x] **Phase 5 — Firmware Architecture & Code Quality (`F1–F15`) (`HOST_TESTED` / `TARGET_COMPILED`):**
  - Freed `7,008 B` static DRAM (`192,280 B` -> `185,272 B`, `56.5%`), extracted [`popular_games_table.h`](../../firmware/BmoGameboy/src/storage/popular_games_table.h) (`sd_catalog.cpp` `919` lines; `bmo_app.cpp` `1,197` & `bmo_face.cpp` `1,145` deferred post-bring-up).
  - Added `esp_reset_reason()` / `RTC_NOINIT_ATTR` crash breadcrumb logging (`boot_safe_mode.h`, `boot_safe_mode_test.cpp`), [`docs/reference/controls.md`](../reference/controls.md), and complete [`docs/architecture.md`](../architecture.md) (`2065c54`).
- [x] **Phase 6 — Performance & Analytical Honesty (`P1–P5`) (`HOST_TESTED`):**
  - Updated [`docs/compatibility.md`](../compatibility.md) with `Primary` vs `Experimental (NOT_TESTED on HW)` tiers and [ADR 0015](../decisions/0015-display-driver-and-ui-framework.md).
- [x] **Phase 7 — Tests, CI & Dev Environment (`T1–T6`) (`HOST_TESTED`):**
  - Added `tasks.py doctor`, `tasks.py check --fast` (`22.64 s`), `.devcontainer`, `.vscode`, `.clangd`, and [ADR 0016](../decisions/0016-build-system-evaluation.md).
- [x] **Phase 8 — AI Engineering & Tooling Environment (`A1–A7`) (`HOST_TESTED`):**
  - Updated [`AGENTS.md`](../../AGENTS.md) and [`firmware/AGENTS.md`](../../firmware/AGENTS.md), added [`docs/ai/STATE.md`](../ai/STATE.md) (`26` lines), `generate_codemap.py` -> [`docs/ai/CODEMAP.md`](../ai/CODEMAP.md) (`88` lines), and [`docs/audits/ai-and-tooling-review.md`](ai-and-tooling-review.md) (`4828e44`).
- [x] **Phase 9 — Owner Documentation (`HOST_TESTED`):**
  - Authored [`docs/guides/owner-guide.md`](../guides/owner-guide.md) and [`docs/owner-decisions.md`](../owner-decisions.md) (`8` decisions), updated [`README.md`](../../README.md).
- [x] **Phase 10 — Final Verification, Health Scorecard & Handoff (`HOST_TESTED` / `TARGET_COMPILED`):**
  - Completed [`docs/audits/health-scorecard.md`](health-scorecard.md) (`1–5` ratings across all `11` areas + `30/60/90`-day plan) and [`docs/audits/pr-description.md`](pr-description.md).
  - Verified `tasks.py format --check` (`0.81 s`), `tasks.py check --fast` (`22.64 s`), `tasks.py check` (`84.45 s`), `tasks.py test` (`540.17 s`), `tasks.py build --profile all --warnings strict --fail-on-warnings` (`543.20 s`, `0` warnings across all `7` configs), `tasks.py size --check-budgets`, and fresh clones with/without spaces (`rc=0`).

## Blockers / Owner Decisions Queued

- Zero blocking technical issues. All `8` items requiring human sign-off (`1–8`) are documented in plain English in [`docs/owner-decisions.md`](../owner-decisions.md).
