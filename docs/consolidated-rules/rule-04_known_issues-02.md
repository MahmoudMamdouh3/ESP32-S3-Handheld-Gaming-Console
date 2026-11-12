# Section 04 — Known Issues and Audit Findings (2/2)

Source: [`04_known_issues.md`](../../.agents/rules/04_known_issues.md).

## Changelog

- 2026-11-12: Fixed clean-checkout blockers and addressed the first remote CI failures: track `rom_file_filter.h`, make ignored local ROM assets optional, skip absent gitignored `games/`, install pinned Pillow, normalize Windows path separators in the box-art test helper, and isolate the ASan probe from UBSan instrumentation. The clean-checkout compile, local test suite, and validator passed; remote rerun remains pending.
- 2026-11-12: Addressed the next remote CI failure by excluding downloaded `.tools` SDK files from repository indexes, keeping compiled WASM test output in a temporary directory, and regenerating bounded rules artifacts. The latest remote run passed every step before generated-artifact drift; a rerun is pending.
- 2026-11-12: Added Arduino CLI data-directory discovery to `ElfAnalyzer`. Ignored ROMs still change ELF metrics, so indexes now omit live values; Guardian retains live budget reporting. Rerun pending.
- 2026-11-12: GitHub Actions run `43238623840` passed all steps, including generated-artifact verification. `F7` and `P3-05` are verified on host; physical hardware and other open workstreams remain.
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