# Pull Request: Full-Scope Tech-Lead Audit & Remediation (`techlead/audit-2`)

## Summary

This pull request executes a full-scope principal engineering and tech-lead audit of the BMO Handheld repository on branch `techlead/audit-2`. Every seed hypothesis (`63` individual rows, zero batched rows) and newly discovered issue was audited against `HEAD`, Espressif ESP32-S3 datasheets, and upstream license files/datasheets (accessed `2026-11-15`), and logged with evidence in [`docs/audits/findings-register.md`](findings-register.md).

> [!CAUTION]
> **DO NOT MERGE `techlead/audit-2` INTO `main`** until the owner resolves **`Decision 9`** (first-party SPDX/license review & `license/spdx-review` branch) and open `P0/P1` hardware/architecture decisions in [`docs/owner-decisions.md`](../owner-decisions.md) (`Decisions 1–6`), plus completes initial read-only `esptool flash-id` (`C1` `OWNER-OBSERVED` `qio` conflict) and standalone `SD_MISO <= 3.35 V` bench checks per [`docs/guides/bringup.md`](../guides/bringup.md).

All changes within autonomous authority (`DO WITHOUT ASKING`) were implemented in atomic Conventional Commits with strict verification gates without altering any Hardware Hard Stops.

---

## Key Changes by Phase

1. **Phase 0 — Baseline & Inventory (`f083de0`):**
   - Recorded full git object, blob, directory, and LOC inventories (`0` blobs `>= 1 MiB` across all refs; `0` secrets across full history).
   - Created [`docs/audits/audit-plan.md`](audit-plan.md), [`docs/audits/findings-register.md`](findings-register.md), and [`docs/audits/progress.md`](progress.md).

2. **Phase 1 — Scripted Truth Reconciliation (`3b206d9`, `949df12`, `35ebb41`, `2520f13`):**
   - Built [`scripts/extract_claims.py`](../../scripts/extract_claims.py) generating [`docs/audits/claims-inventory.md`](claims-inventory.md) across `602` extracted factual/numeric claims (`TRUE=577`: `82 deep-verified` + `495 pattern-checked`, `UNVERIFIABLE=25`, `STALE=0`, `FALSE=0`).
   - Reconciled [`ROADMAP.md`](../../ROADMAP.md) (`15` platforms), [ADR 0003](../decisions/0003-rom-keys-and-index-v2.md) / [ADR 0011](../decisions/0011-index-path-storage.md) (`BMOIDX03`), [ADR 0012](../decisions/0012-save-state-schema.md) (`BSS2`), and [ADR 0013](../decisions/0013-partition-layout.md) (`app0 = 8 MiB` + `ffat = 7.875 MiB` `0x7E0000`).
   - Consolidated performance docs into [`docs/performance.md`](../performance.md), wired `TFT_RST=9` in [`hardware/diagram.json`](../../hardware/diagram.json), distinguished `ESP32-S3-DevKitC-1-N16R8` carrier board vs `ESP32-S3-WROOM-1-N16R8` module in [`docs/hardware.md`](../hardware.md), added [`scripts/convert_covers.py`](../../scripts/convert_covers.py), and removed local paths.

3. **Phase 2 — Licensing, SPDX Forensic Audit & Supply-Chain Security (`284b36e`, `b253b9b`, `addcc07`, `2520f13`):**
   - Added a complete 16-component license compatibility matrix in [`THIRD_PARTY_NOTICES.md`](../../THIRD_PARTY_NOTICES.md) and [ADR 0008](../decisions/0008-rom-policy-and-licensing.md).
   - Restricted [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) and [`.github/workflows/release.yml`](../../.github/workflows/release.yml) binary uploads to the license-clean `mit` profile while keeping strict multi-profile compilation and size checks active; pinned all GitHub Actions to 40-char commit SHAs.
   - Added **Decision 9** in [`docs/owner-decisions.md`](../owner-decisions.md), identified `19` GPL/Non-Commercial core-coupled files among `125` SPDX-stamped first-party files, and prepared **unmerged** branch `license/spdx-review` containing (`b6b195e`) a clean revert of `addcc07` plus (`3fd5c05`, `d2a9d43`) the corrected SPDX split.

4. **Phase 3 — Hardware, Power, Pins, Enclosure, Layout & Simulator Hygiene (`f564f96`, `76a968f`, `174733d`, `41339aa`):**
   - Added standalone [`firmware/bringup/bringup.ino`](../../firmware/bringup/bringup.ino) (`TARGET_COMPILED`: `390,880 B` flash, `24,600 B` RAM, `73,472 B` IRAM), safe ordered guides [`docs/guides/bringup.md`](../guides/bringup.md) / [`docs/hardware-validation-kit.md`](../hardware-validation-kit.md) (continuity -> disconnected `MISO <= 3.35 V` -> read-only `esptool` -> `bringup.ino` -> full firmware), [`docs/guides/safety.md`](../guides/safety.md), and datasheet-enforced [`docs/bom.md`](../bom.md) (`tests/test_bom_ratings.py`).
   - Rewrote [`ADR 0006`](../decisions/0006-portable-power.md) and Decision 1 withdrawing `Pololu S7V7F5` / `OS102011MA1QN1` / `IRF9540N` / `FQP27P06` / `MF-R150`, and added [`hardware/layout_3xaa_variant.json`](../../hardware/layout_3xaa_variant.json) + `check_layout_collisions()` proving `3×AA` (`5.80 × 4.80 × 1.65 cm`) collides in `Y/Z` without shell resize.
   - Added compile-time GPIO uniqueness and strapping/Flash/PSRAM guards in [`pins_check.h`](../../firmware/BmoGameboy/src/hal/pins_check.h) and [`tests/test_pin_snapshot.py`](../../tests/test_pin_snapshot.py).
   - Expanded ADRs [`0006`](../decisions/0006-portable-power.md), [`0007`](../decisions/0007-fram-wiring.md), [`0014`](../decisions/0014-display-spi-clock.md), and [`docs/wiring/02-sd-card.md`](../wiring/02-sd-card.md) (`10 kΩ` CS pull-ups).
   - Created [`hardware/layout.json`](../../hardware/layout.json) -> [`scripts/generate_hardware_layout_artifacts.py`](../../scripts/generate_hardware_layout_artifacts.py), SVGs, `31×46` perfboard map, `wire_list.csv`, and 3D collision checks (`test_hardware_layout.py`).
   - Fixed 2D simulator screen proportions, axis-aligned ABXY (`0deg`), `SELECT/START` contrast, and removed stale `LiPo` wording / incomplete `35–37` keep-out labels across `simulator/` and `battery.h` (`41339aa`, `6bc4a30`).

5. **Phase 4 — Repo Structure, Naming & Hygiene (`ed4fa96`, `941509c`, `0b22efc`, `8ce5091`):**
   - Added [`docs/conventions.md`](../conventions.md), [`.editorconfig`](../../.editorconfig), [`.gitattributes`](../../.gitattributes), [`.github/CODEOWNERS`](../../.github/CODEOWNERS), [`.github/dependabot.yml`](../../.github/dependabot.yml), and Conventional Commit hook [`.githooks/commit-msg`](../../.githooks/commit-msg).
   - Removed `11` unused upstream build files (`941509c`) and `8` unreferenced one-off scripts (`0b22efc`), and gated dormant `fram_save.cpp` (`FEATURE_FRAM`) and `unit_tests.cpp` (`ENABLE_UNIT_TESTS`).

6. **Phase 5 — Firmware Architecture, Memory & Safety Hardening (`8748508`, `445300b`, `6bc4a30`, `2065c54`, `97fe3d1`):**
   - Freed **`7,008 B` (`6.84 KiB`) of internal static DRAM** in `default` (`192,280 B` -> `185,272 B`, `56.5%`) and proven zero WiFi/BT linkage (`0` of `20,277` ELF symbols), extracted [`popular_games_table.h`](../../firmware/BmoGameboy/src/storage/popular_games_table.h) (`sd_catalog.cpp` `919` lines; `bmo_app.cpp` `1,192` & `bmo_face.cpp` `1,145` deferred post-bring-up), extracted [`doom_palette.h`](../../firmware/BmoGameboy/src/ui/doom_palette.h) with equivalence verified by [`doom_palette_test.cpp`](../../tests/host/doom_palette_test.cpp), and recorded `M6502` / palette moves as `NOT_TESTED` risks in [`hardware_measurements.md`](../hardware_measurements.md) Step `9`.
   - Hardened `esp_reset_reason()` and `RTC_NOINIT_ATTR` crash breadcrumb with magic + CRC32 validation (`boot_safe_mode.h`, `boot_safe_mode_test.cpp` incl. cold-boot garbage tests) and added mid-commit SD removal fault-injection in [`atomic_fs_test.cpp`](../../tests/host/atomic_fs_test.cpp).
   - Rewrote [`docs/architecture.md`](../architecture.md) and [`docs/reference/controls.md`](../reference/controls.md) verified directly from source.

7. **Phase 6–10 — Performance, Tooling, AI Review & Handoff (`47d125b`, `17c7e28`, `342655b`, `591f8d3`, `4828e44`, `97fe3d1`, `2520f13` + final):**
   - Updated [`docs/compatibility.md`](../compatibility.md) with `Primary` vs `Experimental` tiers + [ADR 0015](../decisions/0015-display-driver-and-ui-framework.md) and [ADR 0016](../decisions/0016-build-system-evaluation.md), and added boot/catalog/input latency models and adaptive-frameskip / dirty-rect / power evaluations in [`docs/performance.md`](../performance.md).
   - Added `tasks.py doctor`, `tasks.py check --fast` (`29.31 s`), `.devcontainer`, `.vscode`, `.clangd`, `docs/ai/STATE.md` (`26` lines), `scripts/generate_codemap.py` -> [`docs/ai/CODEMAP.md`](../ai/CODEMAP.md) (`89` lines), and cross-file pin parity checks (`tests/test_pin_snapshot.py`).
   - Authored [`docs/guides/owner-guide.md`](../guides/owner-guide.md), [`docs/owner-decisions.md`](../owner-decisions.md) (`9` decisions), [`docs/audits/ai-and-tooling-review.md`](ai-and-tooling-review.md), `63`-row individual [`findings-register.md`](findings-register.md), and explicit `1–5` rubric [`docs/audits/health-scorecard.md`](health-scorecard.md).

---

## Verification Evidence

- **`HOST_TESTED`:**
  - `python tasks.py doctor`, `python tasks.py format --check` (`128` files), `python tasks.py check --fast` (`12` Guardian + `93` fast host tests), `python tasks.py check` (`12` Guardian + `168` Python tests + `26` native C++17 `CTest` targets + `10/10` mutations, `100%` passed).
  - `python scripts/extract_claims.py --fail-on-false` (`602` claims: `TRUE=577` [`82` `deep-verified`, `495` `pattern-checked`], `UNVERIFIABLE=25`, `STALE=0`, `FALSE=0`), `python scripts/check_markdown_links.py` (`0` issues), `python scripts/generate_hardware_layout_artifacts.py --check-variant hardware/layout_3xaa_variant.json`.
  - Earlier Phase 10 full `python tasks.py test` (`185` Python/WASI tests; `10/10` mutations killed) and fresh-clone checks (`git clone --no-hardlinks`, `rc=0`, `skipped=10` requiring local `.tools/`) ran before follow-up electrical/doc/BOM-test remediation; `tasks.py check` (`26` CTest + `168` Py + `12` Guardian + `10/10` mutations), `tasks.py check --fast` (`93` Py + `12` Guardian), `format --check`, `extract_claims --fail-on-false`, `check_markdown_links`, `test_bom_ratings`, `test_hardware_layout` (incl. `layout_3xaa_variant.json`), and `tasks.py size --check-budgets` all ran after our final edits. *(Firmware compile `tasks.py build --profile all --warnings strict --fail-on-warnings` passed with `0` first-party warnings across all `7` build configs before doc/test/Python-only edits; no C/C++ source files were modified afterwards.)*
- **`TARGET_COMPILED`:** `python tasks.py build --profile all --warnings strict --fail-on-warnings` (`0` first-party warnings across all `7` build configs), `python tasks.py size --check-budgets` (`default` `3,293,216 B` Flash / `185,272 B` DRAM), and standalone `firmware/bringup/bringup.ino` (`390,880 B` flash / `24,600 B` RAM).
- **`HARDWARE_TESTED`:** `0` records (requires physical assembly and bring-up by the owner per [`docs/guides/bringup.md`](../guides/bringup.md)).
