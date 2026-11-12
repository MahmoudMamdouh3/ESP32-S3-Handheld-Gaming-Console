# Changelog

## [Clean-checkout CI fixes] - 2026-11-12

### Fixed
- Added the ROM file filter header to version control despite the generated-ROM-header ignore rule.
- Made local-only ROM headers optional in firmware and test compilation so clean checkouts do not require ignored content.
- Skip installed-game integrity checks when the gitignored `games/` directory is absent.
- Pin Pillow in CI, normalize both path separators in the box-art test helper, and keep the sanitizer probe focused on AddressSanitizer rather than an earlier UBSan report.

### Verification
- Clean-checkout compile passed with `Sketch uses 5192960 bytes`; the installed-games test skipped because its gitignored data directory was absent.
- `python -m unittest discover -s tests -v`: `Ran 158 tests in 50.313s`, `OK`.
- `python -m tools.guardian verify`, `ui-audit`, and `diff-audit` passed; diff-audit reported 0 critical violations and 0 documentation/rule drift issues.
- `python scripts/validate_repo.py` passed all nine phases in 184897 ms. Target compile used 5,774,632 / 16,777,216 bytes flash and 253,728 / 327,680 bytes SRAM.
- Targeted host tests for the sanitizer probe, box-art paths, image pipeline, and native cores passed after the first remote run exposed Linux-only failures.
- The sanitizer, box-art, image-pipeline, and native-core test selectors passed after the CI fixes.
- The first remote GitHub Actions run failed in host tests; the causes and follow-up are recorded in the issue ledger. Physical hardware verification is not claimed.

## [Production-readiness assessment and workstream handoff] - 2026-11-12

### Added
- Documented the 2026-11-12 production-readiness review, recorded design decisions (40 MHz in-spec display clock default with NVS 80 MHz opt-in and boot-button revert; isolated `fix/production-readiness` CI branch), and full specifications for workstreams `WS1`–`WS4` in [the production-readiness handoff](docs/handoffs/2026-11-12-production-readiness.md).
- Added native C++17 AddressSanitizer/UBSan test runner `build_and_run_native_cxx()` (`find_vcvars64()`, `find_native_cxx()`) in `tests/host_test_support.py` and verified ASan detection in `tests/test_host_test_support.py`.

### Verification
- `python -m unittest tests.test_host_test_support -v` passed (1 test, ASan out-of-bounds detection confirmed). Full branch validation is recorded in the handoff and commit message.

## [Audit remediation and repository handoff] - 2026-11-07

### Changed
- Added path-derived CRC32 keys for save files, states, screenshots, and box art, with matching Python tooling and deployment handling for nested game paths.
- Gated Game Boy, NES, and SNES battery persistence on cartridge metadata and disabled ordinary battery-RAM writes for volatile-memory cores.
- Added source-derived PSRAM budget reporting, bounded DOOM-zone sizing, index layout assertions, and validation of cached index entries.
- Added compact generated firmware-symbol pages, Guardian size and banned-language checks, and a bounded rules index generator.
- Updated firmware diagnostics and the browser simulator to show that the battery monitor is not fitted.
- Added `TFT_SPI_HZ`, `SD_SPI_HZ`, and a Diagnostics full-screen pattern to compare the configured 80 MHz display clock with a 40 MHz fallback.
- Corrected color-token values and the 14-core save description, archived oversized engineering histories and the SDD, and converted Windows-absolute Markdown links to repository-relative links.
- Added Guardian checks for Markdown links and regenerated the compact firmware symbol index and consolidated rules pages.
- Added Diagnostics stack high-water display/logging and a Guardian rule for local arrays over 1,024 bytes; split the root README into two bounded detail pages.
- Made the generated knowledge graph reproducible by excluding generation timestamps, self-indexed output files, and live host microbenchmark samples; current host measurements remain available through `python -m tools.guardian bench-host`.

### Verification
- Python and host-compiled check results, repository validator output, and compile status are recorded in [the remediation handoff](docs/handoffs/2026-11-07-remediation.md).
- Fresh pinned ESP32-S3 builds passed at the configured 80 MHz and the 40 MHz comparison clock. The 80 MHz app image is 5,774,784 bytes and leaves 2,613,824 bytes in app0; global data is 253,728 bytes (77.4% of SRAM).
- `python scripts/validate_repo.py --require-target-compile` passed all nine phases; `python -m unittest discover -s tests -v` passed all 157 tests; Guardian diff-audit/UI audit, color-token verification, and Markdown-link verification passed.
- The local CI workflow is defined, but no remote CI run or physical board test is claimed. The handoff lists remaining hardware and runtime work.
- Earlier entries are preserved in [the changelog archive](docs/archive/changelog-history.md).
