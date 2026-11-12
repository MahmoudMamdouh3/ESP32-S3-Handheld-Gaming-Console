# Section 04 — Known Issues and Audit Findings (2/2)

Source: [`04_known_issues.md`](../../.agents/rules/04_known_issues.md).

## Remaining work from the A1–F16 audit

- Hardware-only: panel timing and color, PSRAM largest-block/headroom, save power-loss recovery, SD-card interoperability, stack high-water values, and all physical controls remain unverified. Do not flash during CI or claim `VERIFIED_HARDWARE`.
- Display/mascot: expression-level culling pixel fractions, on-device frame costs, and a full native/WASM golden-image parity suite are not available.
- Teardown/concurrency: audit every SPI lock/unlock path, finite wait, display queue timeout, and buffer lifetime; add fake backends and timeout/failure injection.
- Runtime pacing: integrate region-aware frame targets and absolute 64-bit deadlines; capture real 600-frame p50/p95/p99 windows and add measured profiler zones.
- ROM/index/save: implement FAT hidden-attribute handling, unsupported-name reporting, rescan progress/cancel, card identity/freshness checks, launch-time file validation, broader parser fuzzing, and atomic-write failure injection.
- Save UX/schema: test `SaveManager` failure cases end-to-end, validate state schema compatibility and timestamps, and cover SNES HiROM/ExHiROM plus PCE BRAM provenance.
- CI/release: observe a successful remote workflow before calling CI verified; complete ROM/vendor license provenance and clean-clone/tree hygiene review.