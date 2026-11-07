# Audit Remediation Log — 2026-11-07

## Continuation verification

This continuation supersedes the initial “no fresh target compile” and “validator did not pass” state recorded below. The older baseline remains as a historical record of the first inspection.

- Pinned profile in `firmware/BmoGameboy/sketch.yaml`: `esp32:esp32:esp32s3:CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi,USBMode=default`; Arduino-ESP32 3.3.11 and Adafruit BusIO 1.17.4, GFX 1.12.6, ST7735/ST7789 1.11.0. The generated `build.options.json` confirms this FQBN.
- Fresh configured 80 MHz compile through `python scripts/validate_repo.py --require-target-compile`: `Sketch uses 5,774,632 bytes`; globals 253,728 / 327,680 bytes (77.4%); app image 5,774,784 / 8,388,608 bytes, leaving 2,613,824 bytes in app0. Final validator result: all nine phases passed in 314,080 ms. It emitted the configured-panel warning: 80 MHz is above the ST7789VW 62.5 MHz published serial-write limit.
- Fresh 40 MHz comparison compile: `Sketch uses 5,774,580 bytes`; globals 253,728 / 327,680 bytes (77%). This command was `.\arduino-cli.exe compile --clean --profile esp32s3 --build-path build/target-compile-40mhz --build-property compiler.cpp.extra_flags=-DTFT_SPI_HZ=40000000UL firmware/BmoGameboy`.
- Fresh 80 MHz ELF report: 5,774,741 flash bytes; 253,728 DRAM bytes; 77,312 IRAM bytes. `guardian budget` reports 2,613,867 bytes of app0 headroom using ELF size; the validator's `.bin` partition check is the release-fit result.
- `python -m unittest discover -s tests -v`: 157 tests passed in 35.679 s. The run includes host C/C++ contracts and selected native/WASM emulator checks, plus byte-for-byte repeatability checks for generated knowledge artifacts.
- `python -m tools.guardian verify`: 181 tests passed in 56.499 s, with zero critical AST violations and zero doc-truth drift.
- `python -m tools.guardian diff-audit`: PASS, zero critical firmware AST violations, zero doc truth drift. `python -m tools.guardian ui-audit`: PASS across 47 checks. `python scripts/verify_color_tokens.py`: PASS. `python scripts/check_markdown_links.py`: PASS, zero issues.
- `.github/workflows/ci.yml` now pins the GitHub Actions SHAs, Arduino libraries/core, and WASI SDK; it includes strict compile, host tests, Guardian, link, color, and generated-artifact drift checks. A remote workflow has not yet run.
- Knowledge-graph generation now excludes wall-clock timestamps, its own generated outputs from the file-stat index, and live host microbenchmarks. `tests/test_generated_artifacts_deterministic.py` runs generation twice and checks byte-for-byte output stability; `bench-host` remains the explicit host-only measurement command.
- No physical device was flashed or measured. The 80 MHz panel timing, actual PSRAM headroom, stack high-water values, SD behavior, and persistence after power loss remain hardware checks. SRAM use is 77.4% at link time, and Arduino warns that local memory may be low; measure runtime headroom before treating it as safe for all workloads.
- Two repository assertions were corrected to match the intended behavior: telemetry labels its synthetic report source, and `.a78` is not routed to the Atari 2600 core.

### Remaining scope

- Culling pixel fractions, device rendering timings, and full native/WASM golden-frame parity remain unmeasured.
- Display teardown waits for the active display operation and destroys the selected wrapper, but all SPI lock balances, finite timeouts, RAII paths, and buffer-lifetime failure cases still need review and fakes.
- Pacing helpers and nominal per-core frame budgets exist; region-aware targets, 64-bit absolute deadlines, runtime 600-frame capture, and integrated profiler zones remain.
- ROM scanning has extension/name/size filtering and a visible skipped count, but FAT hidden attributes, per-name logging, persisted counts, rescan progress/cancel, media identity/freshness, launch-time revalidation, and broader fake-filesystem coverage remain.
- Save failure UX has retry/resume/quit-anyway policy coverage, while SaveManager fault injection, save-state compatibility/timestamp display, and on-device power-cycle validation remain.
- Sanitizer mutation campaigns, unsupported-format counts by core, SNES HiROM/ExHiROM coverage, PCE BRAM provenance, full lock/timeout audits, and remaining ADR/license provenance work remain.

## Baseline and constraints

- At first inspection the worktree was already dirty. Current branch is `main`, one commit ahead of `origin/main`; `HEAD` is `2da4848 fix(core): truthful battery status`. Existing user work was preserved, and no commit was created.
- Latest `git log -5 --oneline`: `2da4848`, `91cbb24`, `83ba47c`, `0d19d1b`, `a21c713` (see the repository log for full subjects).
- Tag: `v0.1-display-buttons`. Remote: `origin https://github.com/MahmoudMamdouh3/ESP32-S3-Handheld-Gaming-Console.git`. `.github` is absent.
- Tracked `firmware/BmoGameboy/src/assets`: `bmo_assets.h`, `roms/aladdin.h`, `roms/lego_racers.h`, `roms/mario_deluxe.h`, `roms/zelda_ages.h`. `git ls-files firmware/03_emulator` returned no paths.
- Current `config.h` still has `FEATURE_AUDIO=0`, `FEATURE_BATTERY_MONITOR=0`, the existing GPIO assignments, and the planned FRAM pins unchanged. No flash, partition, ROM, game, or SD-card user data was changed. No physical hardware test was performed.
- No new target ELF metrics are available. The earlier FQBN compile attempt could not install/resolve the Espressif platform because DNS access to `downloads.arduino.cc` failed. The validator also encountered a protected context-index write before the approved index update below. Do not reuse the stale 2026-11-03 ELF metrics as current results.

## Verified checks and measured budgets

- `python -m tools.guardian index` completed after its initial filesystem denial: `AI Knowledge Base compilation COMPLETE`; it generated the root knowledge graph (`15090 files, 1557 symbols`), decision tree, manifest, and context index.
- `python -m tools.guardian verify`: `Ran 163 tests in 38.732s / OK`; Guardian reported zero critical AST violations and zero documentation/rule drift.
- `python -m tools.guardian diff-audit`: `DIFF-AUDIT RESULT: PASS -- Ready for validation and commit.`
- `python -m tools.guardian patch-check --strict`: `[PASS] PATCH-CHECK CLEAN: Zero critical issues detected across checked files.`
- `python -m tools.guardian ui-audit`: `Audit completed across 47 checks` and `[PASS] UI-AUDIT CLEAN`.
- `python scripts/verify_color_tokens.py`: `[PASS] Firmware, simulator CSS, DESIGN.md, and SDD color values agree.` The host color test and focused Guardian size/link tests passed after correction.
- Markdown link check: `Markdown link issues: 0; oversized rules: 0`; there are no remaining Windows-absolute links in Markdown.
- `python -m tools.guardian symbols`: `[PASS] Generated 11 symbol pages and docs\generated\symbols.md`.
- ROM path audit used `scripts.deploy_to_sd.ROM_EXTENSIONS`, read-only:

| Directory | Supported files | Paths >=64 UTF-8 bytes | Case-folded 32-char prefix groups / files | Box-art basename groups / files |
| :--- | ---: | ---: | ---: | ---: |
| `games/` | 31,326 | 0 | 1,966 / 5,181 | 834 / 1,888 |
| `sdcard/` | 1 | 0 | 0 / 0 | 0 / 0 |

- P1-04(f) unsupported-format counts by console were not measured. No filenames were recorded.
- `python -m tools.guardian psram-budget` source ledger:

| ROM pool slots | Persistent allocations | Aggregate PSRAM left | DOOM zone upper bound |
| ---: | ---: | ---: | ---: |
| 256 | 796,672 B | 7,591,936 B | 6,291,456 B |
| 2,500 | 976,192 B | 7,412,416 B | 6,291,456 B |
| 36,000 | 3,656,192 B | 4,732,416 B | 4,194,304 B |

The running DOOM allocation uses the measured largest contiguous PSRAM block, subtracts a 524,288 B margin, and clamps to 2,621,440..6,291,456 B. The ledger cannot predict fragmentation.

## Findings ledger

Verdict values describe how much of each source hypothesis was confirmed. Commit is `uncommitted` throughout because the starting worktree was dirty and the required firmware build/validator gates did not complete.

| ID | Verdict | Commit | Evidence and current state |
| :--- | :--- | :--- | :--- |
| P1-01 | PARTIAL | uncommitted | Source-derived PSRAM ledger, cold-scan pool sizing, bounded DOOM zone, host sizing checks. Target build and largest-block runtime check remain. |
| P1-02 | CONFIRMED | uncommitted | Measured path collisions above; shared CRC32 path key, script parity, and legacy migration added. Hardware save/art migration and power-loss recovery remain. |
| P1-03 | CONFIRMED | uncommitted | Volatile-memory persistence was removed; GB/NES/SNES battery saves are metadata-gated. Runtime power-cycle round trips remain. |
| P1-04 | PARTIAL | uncommitted | Fuzzer, sanitizer/ILP32 core runner, compatibility pre-check, DOOM host smoke test, and P1-04(f) counts remain. Per-console unsupported counts are explicitly not measured. |
| P1-05 | PARTIAL | uncommitted | Shared 16 ms pacing is present in the main loop; per-core rates and 600-frame p50/p95/p99 instrumentation remain. GB async A/B is not enabled. |
| P1-06 | PARTIAL | uncommitted | Lock/wait sites were enumerated; complete bounded-timeout, early-return balance, and AST positive/negative checks remain. |
| P1-07 | CONFIRMED | uncommitted | Index header/entry sizes and offsets are guarded; invalid cached entries are checked. Fingerprint and rescan UI remain. |
| P1-08 | CONFIRMED | uncommitted | Source-derived token words are aligned across firmware/CSS/DESIGN/SDD and pass host/UI checks. Coral text on deep teal is only 2.2:1; physical panel color review remains. |
| P1-09 | CONFIRMED | uncommitted | MADCTL initialization and SPI constants are documented. Diagnostics now compares 80 MHz and 40 MHz. ST7789VW v1.0 specifies a 16 ns serial write cycle (62.5 MHz), so 80 MHz is an open hardware timing risk; default was not lowered. |
| P1-10 | DEBUNKED | uncommitted | With the monitor disabled, getters return zero; launcher and Diagnostics show USB/N/A and NOT FITTED. No `analogRead(1)` path is enabled. |
| P1-11 | PARTIAL | uncommitted | Keyed staged writes cover saves, states, screenshots, and box art. Favorites/history/index and fault-injection coverage remain. |
| P1-12 | PARTIAL | uncommitted | Diagnostics displays loop/display task high-water marks in words and logs them every 5 seconds while open. Guardian flags non-static local arrays over 1,024 B outside vendor code; direct device measurement remains. |
| P2-01 | PARTIAL | uncommitted | The root README links to two bounded detail pages; changelog, issue history, and SDD are archived or split into bounded pages; Markdown link check found 0 issues. Tree truth, other hygiene checks, and clean-clone proof remain. |
| P2-02 | OPEN | uncommitted | ROM/IP policy, licenses/notices, provenance, vendor pins, and third-party attribution remain. No ROM was modified. |
| P2-03 | PARTIAL | uncommitted | Ruleset version is 8; oversized history and symbol files are split; size, banned-language, file-URI, and unresolved-link checks are wired into Guardian. Remaining rule cross-reference checks are open. |
| P2-04 | PARTIAL | uncommitted | SDD memory, display, battery, index, and color claims were corrected; the cross-document contradiction sweep remains. |
| P2-05 | PARTIAL | uncommitted | Header-derived symbol generator creates 11 bounded pages and Guardian has a `symbols` command. Declaration/API coverage review remains. |
| P2-06 | OPEN | uncommitted | Full hardware-document truth review remains. |
| P2-07 | OPEN | uncommitted | ADR backfill remains. |
| P3-01 | PARTIAL | uncommitted | Host checks cover ROM keys, save gating, PSRAM sizing, index layout, and color conversion. Core-runner sanitizers remain. |
| P3-02 | OPEN | uncommitted | Per-core CPU conformance suite remains. |
| P3-03 | PARTIAL | uncommitted | Rule-size, banned-language, Markdown-link, and large-local-array checks were added. Duplicate-symbol, feature-flag, lock, and timeout analyzers remain. |
| P3-04 | OPEN | uncommitted | On-device measurement kit and human measurements remain. |
| P3-05 | OPEN | uncommitted | No CI workflow or single local CI entry point was added. |
| P3-06 | OPEN | uncommitted | Simulator server security review remains. |
| P4-01 | OPEN | uncommitted | Emulator registry refactor remains deferred. |
| P4-02 | OPEN | uncommitted | Browser firmware-UI spike remains unstarted. |
| DOOM-ALLOC-001 | CONFIRMED | uncommitted | Guardian flagged unsafe allocation/fatal error paths; zeroed allocation and non-fatal failure handling were added. Target runtime is unverified. |

## Human decisions

1. Select the baked commercial ROM policy and project LICENSE.
2. Decide the battery pack design.
3. Decide whether any pin reassignment is planned for FRAM.
4. Choose the index layout if future media has paths at least 64 bytes.
5. Decide whether to flip `GB_ASYNC_BLIT` after measured A/B results.
6. Decide whether `SELECT + UP` needs a hold or confirmation; the current chord may exit without saving.
7. Decide whether to continue P4-02 beyond its initial spike.

## Initial inspection commit state (historical)

At first inspection, the worktree contained pre-existing user changes and `main` was one commit ahead of `origin/main`. The initial pass made no commit because the required target compile and strict validator had not yet passed. The continuation results above supersede that initial verification status. Physical hardware checks remain pending.
