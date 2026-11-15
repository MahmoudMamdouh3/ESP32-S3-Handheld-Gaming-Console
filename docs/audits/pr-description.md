# Pull Request: Full-Scope Tech-Lead Audit & Remediation (`techlead/audit-2`)

## Summary

This pull request executes a full-scope principal engineering and tech-lead audit of the BMO Handheld repository across Phases 0–10 on branch `techlead/audit-2`. Every seed hypothesis and newly discovered issue was audited against `HEAD`, Espressif ESP32-S3 datasheets, and upstream license files, and logged with evidence in [`docs/audits/findings-register.md`](findings-register.md).

All changes within autonomous authority (`DO WITHOUT ASKING`) were implemented in atomic Conventional Commits with strict verification gates. All `8` architectural, hardware, licensing, naming, and YAGNI scoping choices requiring owner sign-off (`ASK`) are documented in plain English in [`docs/owner-decisions.md`](../owner-decisions.md) without altering any Hardware Hard Stops.

---

## Key Changes by Phase

1. **Phase 0 — Baseline & Inventory (`f083de0`):**
   - Recorded full git object, blob, directory, and LOC inventories (`0` blobs `>= 1 MiB` across all refs; `0` secrets across full history).
   - Created [`docs/audits/audit-plan.md`](audit-plan.md), [`docs/audits/findings-register.md`](findings-register.md), and [`docs/audits/progress.md`](progress.md).

2. **Phase 1 — Truth Reconciliation (`3b206d9`, `949df12`, `35ebb41`):**
   - Built [`docs/audits/claims-inventory.md`](claims-inventory.md) auditing `14` factual claims (`1` `TRUE`, `1` `TRUE/STALE`, `7` `STALE`, `4` `FALSE`/`FALSE+STALE`, `1` `UNVERIFIABLE`).
   - Reconciled [`ROADMAP.md`](../../ROADMAP.md) (`15` platforms), [ADR 0003](../decisions/0003-rom-keys-and-index-v2.md) / [ADR 0011](../decisions/0011-index-path-storage.md) (`BMOIDX03`), [ADR 0012](../decisions/0012-save-state-schema.md) (`BSS2`), and [ADR 0013](../decisions/0013-partition-layout.md) (`app0 = 8 MiB` + `ffat = 7.875 MiB` `0x7E0000`).
   - Consolidated performance docs into [`docs/performance.md`](../performance.md), wired `TFT_RST=9` in [`hardware/diagram.json`](../../hardware/diagram.json), distinguished `ESP32-S3-DevKitC-1-N16R8` carrier board vs `ESP32-S3-WROOM-1-N16R8` module in [`docs/hardware.md`](../hardware.md), added [`scripts/convert_covers.py`](../../scripts/convert_covers.py), and removed local paths.

3. **Phase 2 — Licensing & Supply-Chain Security (`284b36e`, `b253b9b`, `addcc07`):**
   - Added a complete 16-component license compatibility matrix in [`THIRD_PARTY_NOTICES.md`](../../THIRD_PARTY_NOTICES.md) and [ADR 0008](../decisions/0008-rom-policy-and-licensing.md).
   - Restricted [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) and [`.github/workflows/release.yml`](../../.github/workflows/release.yml) binary uploads to the license-clean `mit` profile while keeping 3-profile strict compilation and size checks active; pinned all GitHub Actions to 40-char commit SHAs.
   - Added `SPDX-License-Identifier` headers across `131` first-party files (`addcc07`, listed in [`.git-blame-ignore-revs`](../../.git-blame-ignore-revs)).

4. **Phase 3 — Hardware, Power, Pins, Enclosure, Layout & Simulator Hygiene (`f564f96`, `76a968f`, `174733d`, `41339aa`):**
   - Added standalone [`firmware/bringup/bringup.ino`](../../firmware/bringup/bringup.ino) (`TARGET_COMPILED`: `390,724 B` flash, `24,600 B` RAM) and guides [`docs/guides/bringup.md`](../guides/bringup.md), [`docs/guides/safety.md`](../guides/safety.md), and [`docs/bom.md`](../bom.md).
   - Added compile-time GPIO uniqueness and strapping/Flash/PSRAM guards in [`pins_check.h`](../../firmware/BmoGameboy/src/hal/pins_check.h) and [`tests/test_pin_snapshot.py`](../../tests/test_pin_snapshot.py).
   - Expanded ADRs [`0006`](../decisions/0006-portable-power.md), [`0007`](../decisions/0007-fram-wiring.md), [`0014`](../decisions/0014-display-spi-clock.md), and [`docs/wiring/02-sd-card.md`](../wiring/02-sd-card.md) (`10 kΩ` CS pull-ups).
   - Created [`hardware/layout.json`](../../hardware/layout.json) -> [`scripts/generate_hardware_layout_artifacts.py`](../../scripts/generate_hardware_layout_artifacts.py), SVGs, `31×46` perfboard map, `wire_list.csv`, and 3D collision checks (`test_hardware_layout.py`).
   - Fixed 2D simulator screen proportions, axis-aligned ABXY (`0deg`), `SELECT/START` contrast, and removed stale `LiPo` wording / incomplete `35–37` keep-out labels across `simulator/` and `battery.h` (`41339aa`, `6bc4a30`).

5. **Phase 4 — Repo Structure, Naming & Hygiene (`ed4fa96`, `941509c`, `0b22efc`, `8ce5091`):**
   - Added [`docs/conventions.md`](../conventions.md), [`.editorconfig`](../../.editorconfig), [`.gitattributes`](../../.gitattributes), [`.github/CODEOWNERS`](../../.github/CODEOWNERS), [`.github/dependabot.yml`](../../.github/dependabot.yml), and Conventional Commit hook [`.githooks/commit-msg`](../../.githooks/commit-msg).
   - Removed `11` unused upstream build files (`941509c`) and `8` unreferenced one-off scripts (`0b22efc`), and gated dormant `fram_save.cpp` (`FEATURE_FRAM`) and `unit_tests.cpp` (`ENABLE_UNIT_TESTS`).

6. **Phase 5 — Firmware Architecture & Memory Optimization (`8748508`, `445300b`, `6bc4a30`, `2065c54`):**
   - Freed **`7,008 B` (`6.84 KiB`) of internal static DRAM** in `default` (`192,280 B` -> `185,272 B`, `56.5%`), extracted [`popular_games_table.h`](../../firmware/BmoGameboy/src/storage/popular_games_table.h) (bringing `sd_catalog.cpp` from `1,004` to `919` lines; `bmo_app.cpp` @ `1,197` and `bmo_face.cpp` @ `1,145` remain deferred safely until post-bring-up).
   - Added `esp_reset_reason()` and `RTC_NOINIT_ATTR` crash breadcrumb logging (`bmo_app.cpp`, `boot_safe_mode.h`, `boot_safe_mode_test.cpp`).
   - Rewrote [`docs/architecture.md`](../architecture.md) and [`docs/reference/controls.md`](../reference/controls.md) verified directly from source.

7. **Phase 6–10 — Performance, Tooling, AI Review & Handoff (`47d125b`, `17c7e28`, `342655b`, `591f8d3`, `4828e44` + final):**
   - Updated [`docs/compatibility.md`](../compatibility.md) with `Primary` vs `Experimental` tiers + [ADR 0015](../decisions/0015-display-driver-and-ui-framework.md) and [ADR 0016](../decisions/0016-build-system-evaluation.md).
   - Added `tasks.py doctor`, `tasks.py check --fast` (`22.64 s`), `.devcontainer`, `.vscode`, `.clangd`, `docs/ai/STATE.md` (`26` lines), `scripts/generate_codemap.py` -> [`docs/ai/CODEMAP.md`](../ai/CODEMAP.md) (`88` lines).
   - Authored [`docs/guides/owner-guide.md`](../guides/owner-guide.md), [`docs/owner-decisions.md`](../owner-decisions.md) (`8` decisions), [`docs/audits/ai-and-tooling-review.md`](ai-and-tooling-review.md), and [`docs/audits/health-scorecard.md`](health-scorecard.md).

---

## Verification Evidence

- **`HOST_TESTED`:**
  - `python tasks.py doctor` (`0.60 s`), `python tasks.py format --check` (`0.81 s`, `126` files), `python tasks.py check --fast` (`22.64 s`: `12` Guardian + `89` fast host tests), `python tasks.py check` (`84.45 s`: `12` Guardian + `164` Python tests + `25` native C++17 `CTest` targets, `100%` passed).
  - `python tasks.py test` (`540.17 s`: `185` Python/WASI tests + `12` Guardian + `25` `CTest` + full `validate_repo`; `10/10` mutations killed).
  - Fresh-clone checks via `git clone --no-hardlinks` to temp dirs with and without spaces (`27.32–30.64 s`, `rc=0`, `skipped=10` tests requiring external `.tools/` binaries).
- **`TARGET_COMPILED`:** `python tasks.py build --profile all --warnings strict --fail-on-warnings` (`543.20 s`, `0` first-party warnings across all `7` build configs), `python tasks.py size --check-budgets`, and standalone `firmware/bringup/bringup.ino` (`390,724 B` flash / `24,600 B` RAM).
- **`HARDWARE_TESTED`:** `0` records (requires physical assembly and bring-up by the owner per [`docs/guides/bringup.md`](../guides/bringup.md)).
