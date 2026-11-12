# Known Issues and Audit Findings

The active registry contains current constraints and findings. Historical logs are indexed in [the archive](../../docs/archive/known-issues-history.md). Evidence and handoff details for the 2026-11-07 audit are in [the remediation handoff](../../docs/handoffs/2026-11-07-remediation.md), and the 2026-11-12 assessment and deferred workstreams (`WS1`–`WS4`) are in [the production-readiness handoff](../../docs/handoffs/2026-11-12-production-readiness.md).

## Active constraints

- `CONSTRAINT`: Keep `FEATURE_BATTERY_MONITOR=0`; GPIO1 is not connected to a battery divider.
- `CONSTRAINT`: Keep `FEATURE_AUDIO=0`; the I2S DAC is not fitted.
- `CONSTRAINT`: Preserve the ESP32-S3-N16R8 QIO flash and OPI PSRAM configuration, partition table, pin map, and all user data under `games/`, `roms/`, and `sdcard/`.
- `CONSTRAINT`: Do not claim `VERIFIED_HARDWARE`; no physical hardware test was performed in this audit.

## Current findings

| ID | Status | Current state |
| :--- | :--- | :--- |
| A1 | VERIFIED_TARGET | Pinned ESP32-S3 profile clean-compiled at 80 MHz and in the 40 MHz comparison. The 80 MHz image is 5,774,784 B, with 2,613,824 B free in app0. Build and RAM warnings are recorded in the handoff. |
| A2 | VERIFIED_HOST | Repository validator passed all nine phases; 155 host tests, Guardian diff-audit, UI audit, color-token check, and Markdown link check passed. CI has not run. |
| B1 | IN_PROGRESS | Path-derived CRC32 keys and legacy migration are covered by host tests. The read-only `games/` audit found 31,326 supported files, no path at least 64 UTF-8 bytes, 1,966 case-folded 32-character prefix groups (5,181 files), and 834 exact case-folded box-art basename groups (1,888 files). Hardware migration and recovery remain unverified. |
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
| F7 | IN_PROGRESS | The local CI workflow pins actions and tool versions and uses strict compile mode. No CI run has been observed yet. |
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
| P2-07 | IN_PROGRESS | ADRs 0011–0014 were added; remaining ADR backfill is open. |
| P3-01 | IN_PROGRESS | Host C++ checks cover sizing, save gating, ROM keys, index layout, springs, pacing, quit policy, and palette parity; `tests/host_test_support.py` now provides `build_and_run_native_cxx()` (ASan/UBSan), while seeded mutation fuzzing and `WS1`–`WS4` remain open. |
| P3-02 | OPEN | Per-core CPU conformance suite is pending. |
| P3-03 | IN_PROGRESS | Guardian gained rule-size, banned-language, file-URI, unresolved Markdown-link, and large-local-array checks; duplicate symbol, flag, lock, and timeout analyzers remain. |
| P3-04 | OPEN | On-device measurement kit is pending. |
| P3-05 | IN_PROGRESS | Pinned GitHub Actions workflow and local strict validator exist. The knowledge index is deterministic and has a repeatability test; no remote CI result is claimed. |
| P3-06 | OPEN | Simulator server security review is pending. |
| P4-01 | OPEN | Emulator registry refactor is pending until earlier phases are green. |
| P4-02 | OPEN | Browser firmware-UI spike has not started. |

## Changelog

- 2026-11-07: Updated the active audit ledger and moved historical entries to bounded archive pages.
- 2026-11-07: Added path-key, battery-save, PSRAM-budget, and index-layout remediation; see the dated handoff for exact evidence and remaining work.
- 2026-11-07: Added explicit SPI clock constants and a Diagnostics clock comparison pattern. The 80 MHz display setting exceeds the ST7789VW datasheet's 62.5 MHz serial-write limit; a human perfboard check is still required.
- 2026-11-07: Fresh 80 MHz and 40 MHz profile builds passed; strict repository validation, all 155 host tests, diff-audit, UI audit, color parity, and Markdown links passed. RAM usage is 253,728/327,680 B (77.4%) and requires on-device headroom measurements. See the remediation handoff for exact output and open items.
- 2026-11-12: Recorded production-readiness blockers (tracked commercial ROMs, MIT vs GPL/vendor license gaps, unverified hardware, 80 MHz display clock risk, open audit items), user decisions (40 MHz default + NVS 80 MHz opt-in; isolated `fix/production-readiness` branch), `WS0` ASan/UBSan native test helper (`tests/host_test_support.py`), and `WS1`–`WS4` specifications blocked by subagent 429 quota exhaustion in `docs/handoffs/2026-11-12-production-readiness.md`.

## Remaining work from the A1–F16 audit

- Hardware-only: panel timing and color, PSRAM largest-block/headroom, save power-loss recovery, SD-card interoperability, stack high-water values, and all physical controls remain unverified. Do not flash during CI or claim `VERIFIED_HARDWARE`.
- Display/mascot: expression-level culling pixel fractions, on-device frame costs, and a full native/WASM golden-image parity suite are not available.
- Teardown/concurrency: audit every SPI lock/unlock path, finite wait, display queue timeout, and buffer lifetime; add fake backends and timeout/failure injection.
- Runtime pacing: integrate region-aware frame targets and absolute 64-bit deadlines; capture real 600-frame p50/p95/p99 windows and add measured profiler zones.
- ROM/index/save: implement FAT hidden-attribute handling, unsupported-name reporting, rescan progress/cancel, card identity/freshness checks, launch-time file validation, broader parser fuzzing, and atomic-write failure injection.
- Save UX/schema: test `SaveManager` failure cases end-to-end, validate state schema compatibility and timestamps, and cover SNES HiROM/ExHiROM plus PCE BRAM provenance.
- CI/release: observe a successful remote workflow before calling CI verified; complete ROM/vendor license provenance and clean-clone/tree hygiene review.
