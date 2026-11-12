# Logical and Performance Audit — 2026-11-12

## Scope and baseline

This is the initial evidence log for the repository-wide logical-correctness and performance review. It separates confirmed source defects from modeled or hardware-dependent risks; it does not claim that every possible defect has been found.

- Branch: `audit/performance-logical-fixes-2026-11`
- Initial commit: `496820a` (`main`)
- `python -m tools.guardian audit`: 0 critical AST violations, 9 warnings, 16 bus modes modeled, 7 flagged for more than 60% sequential bus occupancy/low CPU headroom. The warnings include five plain-allocation findings in vendored DOOM sources; those allocations need size/platform triage before any allocator changes.
- ELF: 77,312 B IRAM, 253,728 / 327,680 B DRAM (77.4%), and 5,774,741 B flash. These are local build measurements, not physical-device measurements.
- Guardian host benchmarks are host measurements and must not be presented as ESP32 timing.
- Initial deep Cppcheck reported 47 findings. The invalid-ammo path in `P_GiveAmmo()` was fixed first; the remaining vendor findings were subsequently triaged and fixed. The final deep scan reports 0 findings.

## Confirmed findings

| ID | Severity | Evidence | Status |
|---|---|---|---|
| AUD-01 | HIGH | `SpiArbiter::lock()` returned `true` when its mutex handle was null; all firmware callers ignored the result. Allocation failure/use before initialization could therefore allow shared-bus operations without exclusion. | Fixed with fail-stop init/take/give and a native regression test. |
| AUD-02 | MEDIUM | The report labeled ideal parallel compute as a “Parallel DMA Budget” and claimed DMA was mathematically required. The display worker uses blocking `SPI.writeBytes()`; the model cannot prove DMA use/need, achieved FPS, or dropped frames. | Fixed model labels, classifications, docs, and tests; no device timing claimed. |
| AUD-03 | MEDIUM | `scanRoms()` held the bus mutex for sorting and final pool sizing, and `begin()` retained an outer lock across loading steps. These CPU-only sections unnecessarily blocked the display worker; filesystem enumeration still needs the lock. | Released the outer setup lock and moved sorting/final sizing outside; recursive SD traversal and index I/O remain locked. |
| AUD-04 | MEDIUM | `P_GiveAmmo()` called `I_Error()` for an invalid ammo index and then continued to array access if the recursive error path returned. | Added an explicit failure return; Cppcheck's four corresponding range findings disappeared. |
| AUD-05 | MEDIUM | The report hard-coded stale SRAM/flash health percentages, mislabeled whole-chip flash capacity as app0, and presented a fixed ROM-slot PSRAM estimate as runtime allocation. | Derive health percentages, label the 16 MB physical-flash budget correctly, and mark dynamic PSRAM as unmeasured without device telemetry. |
| AUD-06 | MEDIUM | Vendored DOOM had allocation/error-return paths that could continue with null file handles, buffers, IWAD names, or sprite columns if `I_Error()` returned. | Added explicit guards, cleanup, and returns; Cppcheck's null-pointer and bounds findings are cleared. |
| AUD-07 | MEDIUM | Vendored DOOM passed incorrectly typed values to `scanf`/`printf` and variadic string-join sentinels, and right-shifted a negative fixed-point value. | Matched format/argument types, used typed null sentinels, and replaced the signed shift with equivalent defined arithmetic. |
| AUD-08 | MEDIUM | Memory-file seeking narrowed signed offsets to unsigned before validating them, allowing overflow-sensitive positions. | Validate signed offsets and bounds before updating the cursor; added range and overflow checks. |
| AUD-09 | LOW | Cppcheck 2.17.1 can exit nonzero with only `normalCheckLevelMaxBranches` informational output when a scan finds no issues. | Guardian now accepts that known informational-only result; a regression test covers the zero-finding case. |

## Work in progress

1. The cold SD traversal and index I/O still serialize with display traffic. Measure scan latency, frame pacing, and SPI reliability on hardware before attempting finer-grained SD locking.

## Verification record

Initial evidence was gathered using `python -m tools.guardian audit` and `python -m tools.guardian report --cppcheck --output docs/performance_report.md`. No pin configuration was changed during this audit.

- `python -m tools.guardian verify`: 190 tests passed; 0 critical AST violations; 0 documentation/rule drift.
- `python -m tools.guardian diff-audit`: passed.
- Deep Cppcheck: 0 findings after remediation; see the generated report.
- `python -m unittest discover -s tools/guardian/tests -p test_guardian.py`: 26 tests passed, including Cppcheck informational-output handling.
- `python scripts/validate_repo.py`: all nine phases passed in 192,968 ms. Target compile used 5,774,672 / 8,388,608 B app0 and 253,728 / 327,680 B SRAM.
- No physical hardware test was performed; host benchmarks and compile/analysis results are not device runtime measurements.
