# Known Issues and Audit Findings

Current constraints and findings are listed here. See the [archive](../../docs/archive/known-issues-history.md), [remediation handoff](../../docs/handoffs/2026-11-07-remediation.md), and [readiness handoff](../../docs/handoffs/2026-11-12-production-readiness.md).

## Active constraints

- `CONSTRAINT`: Keep `FEATURE_BATTERY_MONITOR=0`; GPIO1 is not connected to a battery divider.
- `CONSTRAINT`: Keep `FEATURE_AUDIO=0`; the I2S DAC is not fitted.
- `CONSTRAINT`: Preserve the ESP32-S3-N16R8 QIO flash and OPI PSRAM configuration, partition table, pin map, and all user data under `games/`, `roms/`, and `sdcard/`.
- `CONSTRAINT`: Do not claim `VERIFIED_HARDWARE`; no physical hardware test was performed in this audit.

## Current findings

| ID | Status | Current state |
| :--- | :--- | :--- |
| A1 | VERIFIED_TARGET | ESP32-S3 builds passed at 80 MHz and 40 MHz; 80 MHz image 5,774,784 B, app0 free 2,613,824 B. Build/RAM caveats are in the handoff. |
| A2 | VERIFIED_HOST | 164 host tests, 188 Guardian tests, all nine validator phases, audits, and post-merge CI passed. Physical hardware is unverified. |
| B1 | IN_PROGRESS | Path-key/migration contracts pass on host. SD audit found 31,326 supported ROMs and case-folded collision groups; device migration/recovery remain unverified. |
| B2 | IN_PROGRESS | Battery saves are header-gated for GB/NES/SNES, volatile-RAM wrappers are disabled, and host contracts pass. Device round-trip and power-loss tests remain. |
| B3 | OPEN | Seeded sanitizer mutation campaign and per-console unsupported-format counts remain; P1-04(f) counts are not measured. |
| C1 | IN_PROGRESS | Source timing, spring, speech, particles, dance, and palette claims were reconciled. The expression-level culled-pixel fraction and on-device render timings remain unmeasured. |
| C2 | IN_PROGRESS | Color-token values, BGR565 words, contrast rows, and registered face-renderer colors are checked across firmware, CSS, DESIGN, and SDD. Coral-on-deep-teal contrast remains a human design decision; panel color is unmeasured. |
| D1 | IN_PROGRESS | Teardown waits for display completion and only destroys the active wrapper. A complete bounded-timeout, SPI lock-balance, RAII, and buffer-lifetime audit remains. |
| D2 | IN_PROGRESS | Index size/offset assertions, path keys, save gating, and frame helpers have host coverage. Media fingerprinting, variable per-core frame geometry, and broader synthetic filesystem coverage remain. |
| E1 | IN_PROGRESS | Per-core nominal budgets and nearest-rank 600-frame percentile parsing are present. Region-aware pacing, 64-bit absolute deadlines, runtime capture, and integrated profiler zones remain. |
| F1 | IN_PROGRESS | ROM filtering rejects known unsupported extensions, dot paths, OS metadata names, and zero/tiny files; the browser shows a skipped count. FAT hidden attributes, per-name diagnostics, persisted counts, and a fake filesystem are not implemented. |
| F2 | IN_PROGRESS | `.a78` is no longer routed to Atari 2600 and the host test covers it. Full header/mapper compatibility and mutation coverage remain. |
| F3 | IN_PROGRESS | Index validation, nested path keys, and SD retry/rescan UI are covered by host/source checks. Stale subdirectory identity, media-change detection, and launch-time file revalidation remain. |
| F4 | IN_PROGRESS | Save staging and post-write verification cover saves, states, screenshots, and box art. Favorites/history/index migration and fault-injection matrices remain. |
| F5 | IN_PROGRESS | Pause quit now handles failed battery saves with retry, resume, or explicit quit-anyway; the pure policy test passes. SaveManager failure injection remains. |
| F6 | IN_PROGRESS | ADRs 0011–0014 record path key, save-state schema, partition layout, and SPI-clock decisions. Remaining ADR backfill and provenance/license review are open. |
| F7 | VERIFIED_HOST | GitHub Actions run `43238623840` passed target compile, host tests, strict validation, Guardian, Markdown, color, UI, and generated-artifact checks. Live budget metrics remain available through Guardian; physical hardware is not covered. |
| P1-08 | IN_PROGRESS | Firmware, simulator CSS, DESIGN, and SDD color values now agree and pass host/UI checks. Coral on deep teal still needs a human design decision; panel color remains unverified. |
| P1-09 | IN_PROGRESS | `TFT_SPI_HZ=80 MHz`, `SD_SPI_HZ=25 MHz`, and a 40 MHz diagnostics comparison are in place; both firmware profiles compile. ST7789VW v1.0 lists a 16 ns serial write cycle (62.5 MHz), so the default 80 MHz setting remains a hardware timing risk. |
| P1-10 | DEBUNKED | With monitoring disabled, `battery.cpp` returns 0 for voltage and percentage; the launcher shows USB/N/A and Diagnostics says the battery is not fitted. |
| P1-11 | IN_PROGRESS | SaveManager staging and verification cover saves, states, screenshots, and box art; favorites/history/index and fault-injection coverage remain. |
| P1-12 | IN_PROGRESS | Diagnostics shows loop/display task stack high-water marks in words and logs them every 5 seconds while open. Guardian flags non-static local arrays over 1,024 B outside vendor code; direct device stack measurements remain. |
| P2-01 | IN_PROGRESS | The root README links to two bounded detail pages; changelog, issue history, and SDD are archived or split into bounded pages. Guardian reports 0 unresolved Markdown links, and no Windows file-URI text remains; tree checks and other hygiene items remain. |
| P2-02 | OPEN | ROM provenance, third-party notices, policy ADR, and vendor pinning audit remain. No ROM data was changed. |
| P2-03 | IN_PROGRESS | Active logs are shortened, Rule 10 points to generated symbol pages, and Guardian checks rule size, banned language, and Markdown targets. Other rule cross-reference and stale-content checks remain. |
| P2-04 | IN_PROGRESS | SDD claims were corrected against firmware and pinned core sources; a broader contradiction sweep across every hardware document remains. |
| P2-05 | IN_PROGRESS | `python -m tools.guardian symbols` generated 11 header-derived pages and `docs/generated/symbols.md`; API and declaration coverage review remains. |
| P2-06 | OPEN | Hardware-document truthfulness review and physical measurements are pending. |
| 3D-01 | FIXED_UNVERIFIED | The WebGL model and parametric shell have aligned outer and feature coordinates. Seven printable meshes pass closure, winding, positive-volume, Manifold3D, and assembled-part intersection checks; GLB lens-to-rebate overlap is checked. Physical board/module fit, fastener selection, print calibration, switch travel, and thermal performance remain `WAITING`. |
| P2-07 | IN_PROGRESS | ADRs 0011–0014 were added; remaining ADR backfill is open. |
| P3-01 | IN_PROGRESS | Host C++ checks cover sizing, saves, keys, index, springs, pacing, quit, and palette; mutation fuzzing and `WS1`–`WS4` remain. |
| P3-02 | OPEN | Per-core CPU conformance suite is pending. |
| P3-03 | IN_PROGRESS | Guardian gained rule-size, banned-language, file-URI, unresolved Markdown-link, and large-local-array checks; duplicate symbol, flag, lock, and timeout analyzers remain. |
| P3-04 | OPEN | On-device measurement kit is pending. |
| P3-05 | VERIFIED_HOST | GitHub Actions run `43238623840` passed generated-artifact checks. Live ELF values remain excluded from indexes; hardware is unverified. |
| P3-07 | IN_PROGRESS | Fixed null-mutex, scan sorting lock hold, DMA/memory-report claims, and invalid DOOM ammo indexing. Cppcheck: 0 errors, 43 vendor warnings remain; recursive SD traversal/index I/O still serialize with display. |
| P3-06 | OPEN | Simulator server security review is pending. |
| P4-01 | OPEN | Emulator registry refactor is pending until earlier phases are green. |
| P4-02 | OPEN | Browser firmware-UI spike has not started. |

## Changelog

- 2026-11-12: Fixed clean-checkout issues in ROM filters/assets, Pillow, Windows paths, and sanitizer probing; local compile, tests, and validator passed.
- 2026-11-12: Excluded `.tools` SDK files from indexes, isolated WASM test output, and regenerated bounded rules; fixed later generated-artifact drift.
- 2026-11-12: `ElfAnalyzer` now discovers Arduino CLI data; indexes omit live ELF values. GitHub Actions run `43238623840` passed all steps; hardware remains unverified.
- 2026-11-12: Audited and fixed bus-lock fail-open behavior, scan lock duration, DMA reporting, and invalid DOOM ammo indexing. Validator/target compile, 190 Guardian tests, diff-audit, and Cppcheck (0 errors) passed; 43 vendor warnings remain. See the [audit handoff](../../docs/handoffs/2026-11-12-logical-performance-audit.md); hardware remains untested.
- 2026-11-07: Updated the active audit ledger and moved historical entries to bounded archive pages.
- 2026-11-07: Added path-key, battery-save, PSRAM-budget, and index-layout remediation; see the dated handoff for exact evidence and remaining work.
- 2026-11-07: Added explicit SPI clock constants and a Diagnostics clock comparison pattern. The 80 MHz display setting exceeds the ST7789VW datasheet's 62.5 MHz serial-write limit; a human perfboard check is still required.
- 2026-11-07: Fresh 80 MHz and 40 MHz profile builds passed; strict repository validation, all 155 host tests, diff-audit, UI audit, color parity, and Markdown links passed. RAM usage is 253,728/327,680 B (77.4%) and requires on-device headroom measurements. See the remediation handoff for exact output and open items.
- 2026-11-07: Added the 3D simulator/enclosure update, STL/GLB generator, tests, and survey guide. `3D-01` is `FIXED_UNVERIFIED`; physical validation remains open.
- 2026-11-12: Recorded readiness blockers, display-clock decision, `WS0` sanitizer support, and open `WS1`–`WS4` in the handoff.
- 2026-11-12: Added static codebase visualizations (`scripts/generate_code_visualizations.py`, `visualizations/`).

## Remaining work from the A1–F16 audit

- Hardware-only: panel timing and color, PSRAM largest-block/headroom, save power-loss recovery, SD-card interoperability, stack high-water values, and all physical controls remain unverified. Do not flash during CI or claim `VERIFIED_HARDWARE`.
- Display/mascot: expression-level culling pixel fractions, on-device frame costs, and a full native/WASM golden-image parity suite are not available.
- Teardown/concurrency: audit every SPI lock/unlock path, finite wait, display queue timeout, and buffer lifetime; add fake backends and timeout/failure injection.
- Runtime pacing: integrate region-aware frame targets and absolute 64-bit deadlines; capture real 600-frame p50/p95/p99 windows and add measured profiler zones.
- ROM/index/save: implement FAT hidden-attribute handling, unsupported-name reporting, rescan progress/cancel, card identity/freshness checks, launch-time file validation, broader parser fuzzing, and atomic-write failure injection.
- Save UX/schema: test `SaveManager` failure cases end-to-end, validate state schema compatibility and timestamps, and cover SNES HiROM/ExHiROM plus PCE BRAM provenance.
- CI/release: observe a successful remote workflow before calling CI verified; complete ROM/vendor license provenance and clean-clone/tree hygiene review.
