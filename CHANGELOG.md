# Changelog

## [Production-readiness assessment and workstream handoff] - 2026-11-12

### Added
- Documented the 2026-11-12 production-readiness review, recorded design decisions (40 MHz in-spec display clock default with NVS 80 MHz opt-in and boot-button revert; isolated `fix/production-readiness` CI branch), and full specifications for workstreams `WS1`–`WS4` in [the production-readiness handoff](docs/handoffs/2026-11-12-production-readiness.md).
- Added native C++17 AddressSanitizer/UBSan test runner `build_and_run_native_cxx()` (`find_vcvars64()`, `find_native_cxx()`) in `tests/host_test_support.py` and verified ASan detection in `tests/test_host_test_support.py`.
- Added static codebase visualization generator (`scripts/generate_code_visualizations.py`) and exported HTML/JSON/Mermaid views under `visualizations/` (`gitdiagram`, `understand_anything`, `codegraphcontext`, `codecharta`).

### Verification
- `python -m unittest discover -s tests` passed (`Ran 163 tests in 69.689s`, `OK`), including 5 enclosure geometry checks (`tests.test_enclosure_generator`) and 1 ASan native runner check (`tests.test_host_test_support`).
- `python -m tools.guardian verify` passed (`Ran 187 tests in 76.429s`, `OK`), `python scripts/check_markdown_links.py` passed (`0 issues`), and `python -m tools.guardian diff-audit` passed (`0` AST/drift issues).
- Full `python scripts/validate_repo.py` validation and target compile results are recorded in the commit verification notes and [the production-readiness handoff](docs/handoffs/2026-11-12-production-readiness.md). Physical fit, printing, thermal, and hardware tests remain unverified.

## [3D simulator and enclosure] - 2026-11-07

### Changed
- Reworked the WebGL console body, black display glass, button caps, shoulder legends, side BMO marks, speaker holes, and rear details to share the printable model's nominal envelope and feature coordinates.
- Added a parametric split-shell generator for seven printable STL parts and a metre-scale GLB assembly, with display rebate, button retainers, screw features, board placeholders, battery door tabs, ports, side cuts, and vents.
- Added generator tests and pinned mesh dependencies to host CI; documented print setup, export/import, clearances, and the required physical survey.

### Verification
- Generator tests, manifold imports, assembled-part intersection checks, and the five-angle browser review are recorded after final verification below. Physical fit, printing, and thermal tests are not claimed.

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
