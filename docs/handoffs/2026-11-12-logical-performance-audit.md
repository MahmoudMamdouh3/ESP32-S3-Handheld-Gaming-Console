# Logical and Performance Audit — 2026-11-12

## Scope and baseline

This is the initial evidence log for the repository-wide logical-correctness and performance review. It separates confirmed source defects from modeled or hardware-dependent risks; it does not claim that every possible defect has been found.

- Branch: `audit/performance-logical-fixes-2026-11`
- Initial commit: `496820a` (`main`)
- `python -m tools.guardian audit`: 0 critical AST violations, 9 warnings, 16 bus modes modeled, 7 flagged for more than 60% sequential bus occupancy/low CPU headroom. The warnings include five plain-allocation findings in vendored DOOM sources; those allocations need size/platform triage before any allocator changes.
- ELF: 77,312 B IRAM, 253,728 / 327,680 B DRAM (77.4%), and 5,774,741 B flash. These are local build measurements, not physical-device measurements.
- Guardian host benchmarks are host measurements and must not be presented as ESP32 timing.
- Deep Cppcheck reports 0 errors and 43 remaining warnings/portability findings, all in vendored DOOM sources. The initial 47 included four range warnings in `P_GiveAmmo`; an invalid-ammo path could continue if recursive `I_Error()` returned, so an explicit `return false` was added. The remaining vendor findings are recorded in the generated report and need individual upstream/target triage.

## Confirmed findings

| ID | Severity | Evidence | Status |
|---|---|---|---|
| AUD-01 | HIGH | `SpiArbiter::lock()` returned `true` when its mutex handle was null; all firmware callers ignored the result. Allocation failure/use before initialization could therefore allow shared-bus operations without exclusion. | Fixed with fail-stop init/take/give and a native regression test. |
| AUD-02 | MEDIUM | The report labeled ideal parallel compute as a “Parallel DMA Budget” and claimed DMA was mathematically required. The display worker uses blocking `SPI.writeBytes()`; the model cannot prove DMA use/need, achieved FPS, or dropped frames. | Fixed model labels, classifications, docs, and tests; no device timing claimed. |
| AUD-03 | MEDIUM | `scanRoms()` held the bus mutex for sorting and final pool sizing, and `begin()` retained an outer lock across loading steps. These CPU-only sections unnecessarily blocked the display worker; filesystem enumeration still needs the lock. | Released the outer setup lock and moved sorting/final sizing outside; recursive SD traversal and index I/O remain locked. |
| AUD-04 | MEDIUM | `P_GiveAmmo()` called `I_Error()` for an invalid ammo index and then continued to array access if the recursive error path returned. | Added an explicit failure return; Cppcheck's four corresponding range findings disappeared. |
| AUD-05 | MEDIUM | The report hard-coded stale SRAM/flash health percentages, mislabeled whole-chip flash capacity as app0, and presented a fixed ROM-slot PSRAM estimate as runtime allocation. | Derive health percentages, label the 16 MB physical-flash budget correctly, and mark dynamic PSRAM as unmeasured without device telemetry. |

## Work in progress

1. Triage the remaining 43 Cppcheck findings in vendored DOOM individually; do not blanket-suppress or change upstream code without a target-safe rationale.
2. The cold SD traversal and index I/O still serialize with display traffic. Measure scan latency, frame pacing, and SPI reliability on hardware before attempting finer-grained SD locking.

## Verification record

Initial evidence was gathered using `python -m tools.guardian audit` and `python -m tools.guardian report --cppcheck --output docs/performance_report.md`. No pin configuration was changed during this audit.

- `python -m tools.guardian verify`: 190 tests passed; 0 critical AST violations; 0 documentation/rule drift.
- `python -m tools.guardian diff-audit`: passed.
- Deep Cppcheck: 0 errors; 43 warnings/portability findings remain, all in vendored DOOM sources. The generated report lists the findings with repository-relative paths.
- `python scripts/validate_repo.py`: all nine phases passed in 192,968 ms. Target compile used 5,774,672 / 8,388,608 B app0 and 253,728 / 327,680 B SRAM.
- No physical hardware test was performed; host benchmarks and compile/analysis results are not device runtime measurements.
