# Production-Readiness Assessment and Remediation Handoff — 2026-11-12

This record captures the 2026-11-12 production-readiness review, the user decisions made during the session, the native sanitizer test harness added in `WS0`, and the unexecuted remediation specifications (`WS1`–`WS4`) that were blocked when subagent invocations failed with `RESOURCE_EXHAUSTED` (HTTP 429 quota limit).

## Baseline verified at session start (`329162b` on `main`)

- `python scripts/validate_repo.py` passed all nine phases (Phases 0–8) in 199,280 ms, including `TARGET COMPILE: VERIFIED` via `arduino-cli`.
- Target compile metrics:
  - Flash: 5,774,632 / 16,777,216 bytes (34.4%, 65.6% headroom).
  - Static SRAM: 253,728 / 327,680 bytes (77.4%), accompanied by the toolchain warning `Low memory available, stability problems may occur.`
  - App image: 5,774,784 / 8,388,608 bytes (`app0`), leaving 2,613,824 bytes free.
  - Compiler output emitted `#pragma message: TFT_SPI_HZ exceeds the ST7789VW 62.5 MHz published serial-write limit; board timing is unmeasured` across translation units including [`config.h`](../../firmware/BmoGameboy/src/core/config.h).
- Repository tracking and ignore checks: `games/`, `roms/`, and `sdcard/` are excluded by `.gitignore` with 0 tracked ROM files inside those directories.
- Issue ledger baseline ([`04_known_issues.md`](../../.agents/rules/04_known_issues.md)):
  - Committed `HEAD` (`329162b`): 37 tracked rows (`VERIFIED_TARGET`: 1, `VERIFIED_HOST`: 1, `IN_PROGRESS`: 26, `OPEN`: 8, `DEBUNKED`: 1).
  - Working tree prior to this session (with uncommitted `3D-01` enclosure/simulator entry): 38 tracked rows (adding `FIXED_UNVERIFIED`: 1).
- Hardware environment at inspection: no ESP32-S3 device attached to the host (only Bluetooth serial ports `COM4` and `COM5` present).

## Production-readiness assessment and blockers

Verdict: **working prototype / development build; not ready for public or commercial production release.**

1. **Commercial game ROMs committed and compiled into firmware (`P2-02`, [ADR 0008](../decisions/0008-rom-policy-and-licensing.md))**:
   - [`sd_card.cpp`](../../firmware/BmoGameboy/src/core/sd_card.cpp) includes [`mario_deluxe.h`](../../firmware/BmoGameboy/src/assets/roms/mario_deluxe.h), [`zelda_ages.h`](../../firmware/BmoGameboy/src/assets/roms/zelda_ages.h), [`aladdin.h`](../../firmware/BmoGameboy/src/assets/roms/aladdin.h), and [`lego_racers.h`](../../firmware/BmoGameboy/src/assets/roms/lego_racers.h) alongside the gitignored `firmware/BmoGameboy/src/assets/roms/virtual_bmo.h`.
   - `git ls-files` confirms the four commercial 1 MB ROM headers are tracked in git and returned by `SDCard::loadRom()`. Because the GitHub remote (`origin`) is public, these assets are also present in repository history.
2. **Open-source licensing and third-party provenance conflicts (`P2-02`, [ADR 0008](../decisions/0008-rom-policy-and-licensing.md))**:
   - Root [`LICENSE`](../../LICENSE) is MIT.
   - Sources under `firmware/BmoGameboy/src/vendor/doom/` contain GNU General Public License notices.
   - None of the 13 `firmware/BmoGameboy/src/vendor/` subdirectories contains a `LICENSE` or `COPYING` file; only `doom` and `peanut_gb` surfaced license header strings in a source scan. Per-core license compatibility and third-party notices remain unverified.
3. **Zero physical hardware verification (`04_known_issues.md` constraints, `P3-04`)**:
   - Unverified on soldered hardware: ST7789 display timing and color accuracy, runtime PSRAM free/largest-block and FreeRTOS task stack high-water marks, battery-save and save-state durability across unexpected power cuts, SD card compatibility across vendors/capacities, and all wired physical buttons (`UP`, `DOWN`, `LEFT`, `RIGHT`, `A`, `B`, `START`, `SELECT`).
4. **Display SPI clock above datasheet specification (`P1-09`, [ADR 0014](../decisions/0014-display-spi-clock.md))**:
   - [`config.h`](../../firmware/BmoGameboy/src/core/config.h) defaults `TFT_SPI_HZ` to `80000000UL` (80 MHz), exceeding the ST7789VW v1.0 16 ns serial write cycle (62.5 MHz max).
5. **Open and incomplete audit items (`A1`–`P4-02`, see also [the 2026-11-07 remediation handoff](2026-11-07-remediation.md))**:
   - SaveManager failure-injection and power-cut testing (`F4`, `F5`, `P1-11`).
   - Seeded sanitizer mutation fuzzing across ROM/save/index/art parsers (`B3`, `D2`, `P3-01`).
   - Complete SPI lock-balance, unbounded-wait, RAII, and buffer-lifetime audit (`D1`, `P3-03`).
   - Per-core timing conformance tests, 64-bit absolute-deadline pacing, and CPU conformance (`E1`, `P3-02`, `P3-04`).
   - Remote CI workflow execution never observed (`F7`, `P3-05`).
6. **Unfitted / unsoldered hardware features**:
   - `FEATURE_BATTERY_MONITOR=0` (no divider on `GPIO1`), `FEATURE_AUDIO=0` (no I2S DAC), and buttons `X` (`GPIO14`), `Y` (`GPIO42`), `L` (`GPIO47`), `R` (`GPIO48`) configured in `config.h` but unsoldered.

## User decisions recorded (2026-11-12)

The user requested comprehensive fixes for items **3, 4, and 5** (hardware test kit, display clock policy, save fault injection, parser fuzzing, SPI lock/timeout audit, per-core timing tests, and remote CI) and resolved two design questions:

1. **Display SPI clock policy**:
   - Default `TFT_SPI_HZ` to **40 MHz** (in spec; fastest ESP32-S3 APB integer divider rate at or below 62.5 MHz).
   - Permit **80 MHz** only as an explicit **NVS-persisted opt-in** enabled after the operator confirms the on-device display stress pattern passes.
   - Support reverting to 40 MHz both by holding a physically wired button (e.g. `B`) during boot and via an explicit revert action in Diagnostics.
2. **Git branch and CI isolation**:
   - Commit remediation work only onto an isolated branch (`fix/production-readiness`), push to `origin`, and verify the GitHub Actions run in [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml), leaving pre-existing uncommitted 3D simulator, enclosure, and visualization work on `main` untouched.

## Workstream status and technical specifications

### WS0: Native C++17 sanitizer test harness (`VERIFIED_HOST` — implemented)

- **Delivered**:
  - [`tests/host_test_support.py`](../../tests/host_test_support.py): added `find_vcvars64()`, `find_native_cxx()`, and `build_and_run_native_cxx()`. Compiles C++17 fixtures natively with MSVC (`/fsanitize=address`, Visual Studio 18/2022 `vcvars64.bat`) on Windows or `g++`/`clang++` (`-fsanitize=address,undefined -fno-sanitize-recover=undefined`) on Linux/macOS, skipping (or failing under `BMO_REQUIRE_TARGET_COMPILE=1`) if no compiler is found.
  - [`tests/test_host_test_support.py`](../../tests/test_host_test_support.py): verifies clean execution (`returncode == 0`) and confirms AddressSanitizer aborts on an out-of-bounds heap write.

### WS1: 40 MHz display default and on-device hardware validation kit (`OPEN` — blocked by 429 quota)

Planned scope and requirements for the next session:

- **Clock policy (`config.h`, `display_emu.*`, `BmoGameboy.ino`)**:
  - Define `TFT_SPI_DEFAULT_HZ 40000000UL` and `TFT_SPI_FAST_HZ 80000000UL` (opt-in, out of spec) in [`config.h`](../../firmware/BmoGameboy/src/core/config.h), retaining `#if TFT_SPI_HZ > 62500000UL` pragma guard so the default 40 MHz build emits zero `#pragma message` warnings.
  - Store opt-in flag in Arduino `Preferences` (NVS namespace e.g. `bmo_hw`). Apply in `DisplayEmu::begin()`, log the active clock at boot, clear the NVS flag when a wired button (e.g. `B`) is held at boot, and expose a Diagnostics revert action.
  - Update `DisplayEmu::setDiagnosticSpiClock()`, `DisplayEmu::drawSpiStressPattern()`, Diagnostics labels, `tests/`, `tools/guardian` text, SDD, hardware rules, and [ADR 0014](../decisions/0014-display-spi-clock.md).
  - Document frame-time impact: synchronous Game Boy 240x216x2-byte SPI streaming takes ~20.7 ms at 40 MHz vs ~10.4 ms at 80 MHz, whereas NES (256x240) and SMS/PCE (320x200) dispatch through the Core 0 worker.
- **On-device self-test (`src/core/hw_selftest.h/.cpp`, entered from `STATE_DIAGNOSTICS`)**:
  - Emit line-delimited JSON on serial (`HWTEST {"test":"...","result":"PASS|FAIL|SKIP",...}`) and follow `DESIGN.md` tokens (`python -m tools.guardian ui-audit`).
  - **Buttons**: idle stuck-low check across all 12 GPIOs (failing on unexpected low on unwired `X`/`Y`/`L`/`R`), then interactive per-button prompt for the 8 wired buttons with detect latency and ~20 ms bounce transitions, plus optional `START`-skippable prompt for `X`/`Y`/`L`/`R`. Never read `GPIO1` or strapping/PSRAM pins (`0`, `3`, `33`–`37`, `45`, `46`).
  - **Display**: color bars, 32-step R/G/B ramps, and 1-px checkerboard at 40 MHz (`A`=pass / `B`=fail), then timed 80 MHz stress test; save NVS opt-in only after a pass plus second confirmation.
  - **Memory**: report internal DRAM / PSRAM free, largest free block, minimum-ever free, PSRAM address-in-address + inverted pattern test throughput (MB/s) on a safely margined allocation, and loop/display stack high-water marks in words and bytes (`DisplayEmu::getDisplayStackHighWaterMarkWords()`).
  - **SD card**: report card type/size (never log CID serial numbers), run a 256 KB write/read-back/CRC32 throughput test under `/hwtest/` at `{4, 10, 20, 25}` MHz while holding `SpiArbiter` after `DisplayEmu::waitForDisplay()`, and remount at `SD_SPI_HZ`.
  - **Save power-cut test**: use public [`save_manager.h`](../../firmware/BmoGameboy/src/core/save_manager.h) API only (`__hwtest__/powercut.gb` 32 KB sequence-derived payload), persist committed sequence and armed flag in NVS, verify `{last, last+1}` consistency in `HwSelfTest::onBoot()` called from `setup()`, and make `onBoot()` an immediate no-op when unarmed or when SD is absent.
- **Host record generator (`tools/hwtest/`)**:
  - Add `python -m tools.hwtest` CLI parsing serial capture files (and optional live serial when `pyserial` is installed, handling missing `pyserial` cleanly), validating `HWTEST` JSON lines against a schema, generating `docs/hardware-measurements/YYYY-MM-DD-<test>.md` + `.json` matching [`docs/hardware_measurements.md`](../hardware_measurements.md), and printing a summary table, with unit tests (`tests/test_hwtest_records.py`) writing only to temporary directories.

### WS2: `SaveManager` fault injection and parser mutation fuzzing (`OPEN` — blocked by 429 quota)

Planned scope and requirements (`F4`, `F5`, `P1-11`, `B3`, `D2`, `P3-01`):

- **Storage seam & transaction extraction**:
  - Introduce `src/core/save_storage.h` (exists, remove, rename, mkdir, open read/write, file read/write/seek/size/flush/close, staging alloc/free) and host-compilable `src/core/save_transaction.h` (no `Arduino.h`, `SD.h`, FreeRTOS, or `display_emu.h`) holding `recoverAtomicWrite`, `verifyFilePayload`, `finishAtomicWrite`, battery save/load, save-state write/load + header checks, legacy migration, and screenshot commit.
  - Keep [`save_manager.h`](../../firmware/BmoGameboy/src/core/save_manager.h) public signatures and on-disk formats (`BSS1`, `.tmp`, `.bak`) unchanged; [`save_manager.cpp`](../../firmware/BmoGameboy/src/core/save_manager.cpp) wraps the SD backend with `DisplayEmu::waitForDisplay()` and `SpiArbiter` lock/unlock balanced across all early returns.
- **Fault-injection & power-cut test suite (`tests/host/save_fault_injection_test.cpp`, `tests/test_save_fault_injection.py`)**:
  - Model FAT semantics in an in-memory fake FS: `rename()` fails if destination exists, `FILE_WRITE` truncates, and unflushed writes drop on power cut.
  - Inject failures (open, write, short write, flush, rename, remove, exists, mkdir, alloc) at every call index across `saveBatteryRam`, `loadBatteryRam`, `saveState`, `loadState`, and `saveScreenshotBmp`. Assert `false` return, no crash, intact prior save, and unmodified caller state buffer on failed load.
  - Enumerate power cuts at every operation index `k` (with and without an existing save), reboot from durable state, load, assert exact old or exact new payload, and verify a follow-up save recovers cleanly.
  - Test corrupt headers/payloads (bit flips, truncation, bad magic/version/coreId, oversized `stateSize`/`ramSize`).
- **Deterministic seeded mutation fuzzing under ASan/UBSan (`BMO_FUZZ_ITERS` env knob, fixed seeds, reproducible seed+iteration failure output, per-target accepted/rejected counts)**:
  - Targets using real firmware parsing headers: `SaveManager` state/header validation (via fake FS), `BMOIDX02` index parsing/validation (extracted from [`sd_card.cpp`](../../firmware/BmoGameboy/src/core/sd_card.cpp) + [`rom_index.h`](../../firmware/BmoGameboy/src/core/rom_index.h)), [`battery_save.h`](../../firmware/BmoGameboy/src/core/battery_save.h), `firmware/BmoGameboy/src/core/rom_file_filter.h` (currently untracked due to `.gitignore`), [`path_key.h`](../../firmware/BmoGameboy/src/core/path_key.h) (`RomKey::make`), and raw/BMP header decoding extracted from [`box_art.cpp`](../../firmware/BmoGameboy/src/core/box_art.cpp).

### WS3: SPI lock-balance and timeout audit (`OPEN` — blocked by 429 quota)

Planned scope and requirements (`D1`, `P3-03`, part of `P3-01`):

- **Guardian static analyzer (`tools/guardian/core/spi_audit.py` + CLI subcommand + `guardian verify` + Phase 2 in [`scripts/validate_repo.py`](../../scripts/validate_repo.py))**:
  - Scan `firmware/BmoGameboy/BmoGameboy.ino` and `firmware/BmoGameboy/src/**` (excluding `src/vendor/**`) using comment/string-aware brace tracking.
  - Flag unbalanced `SpiArbiter::lock()` / `unlock()`, `return`/`break`/`continue`/`goto` executed at lock depth > 0 without an RAII guard, `portMAX_DELAY` in `xSemaphoreTake`/`xQueueSend`/`xQueueReceive`/`ulTaskNotifyTake`/`SpiArbiter::lock`, and unbounded `while` flag-polling loops.
  - Support an explicit allowlist with per-entry justification (e.g. `DisplayEmu::startFrame`/`endFrame`, idle Core 0 `xQueueReceive` in `displayWorkerTask`), backed by unit tests on synthetic C++ snippets.
- **Firmware fixes (`spi_arbiter.*`, `display_emu.*`, `sd_card.cpp`, `box_art.cpp`, `src/emulators/*.cpp`)**:
  - Add `SpiArbiter::Guard` RAII helper in [`spi_arbiter.h`](../../firmware/BmoGameboy/src/core/spi_arbiter.h) and adopt it on early-return paths.
  - Replace unbounded waits where safe: bounded `xQueueSend` after `waitForDisplay()` with frame-drop counter; bounded loop inside `waitForDisplay()` logging stalls (elapsed ms) at 1 s intervals and incrementing a stall counter while still waiting for bus safety; expose counters (drops, stalls, max wait/hold µs) via getters for Diagnostics; verify all heap buffers passed to the display worker remain alive until done.

### WS4: Per-core timing conformance and 64-bit pacing (`OPEN` — blocked by 429 quota)

Planned scope and requirements (`E1`, `P3-02`, host side of `P3-04`):

- **64-bit absolute-deadline pacer (`frame_pacing.h` / `frame_pacer.h` + `BmoGameboy.ino` emulator loop)**:
  - Host-testable pacer using `esp_timer_get_time()` on target: advances next deadline by frame period (zero drift), handles 32-bit wrap safely, and resyncs after `N` missed frames instead of spiral catch-up.
  - Keep existing `FPS:` and `Frame window:` serial output formats compatible with [`tools/telemetry_plotter.py`](../../tools/telemetry_plotter.py).
  - Add a region parameter only for cores whose vendored source actually implements PAL line counts/timings; keep NTSC cores paced at NTSC rates.
- **Per-core timing conformance & execution tests**:
  - Verify master clock, cycles/line, lines/frame, or frame-rate constants in all 14 wrappers / 15 platforms against published platform specs (with cited derivations) and against [`frame_pacing.h`](../../firmware/BmoGameboy/src/core/frame_pacing.h); log any core-vs-hardware divergence without altering vendor cores.
  - Extend [`tools/host_cores_runner.c`](../../tools/host_cores_runner.c) (or add a sibling runner) to record relative host frame times and assert emulated cycles/frame are within tolerance where exposed.
  - Add a native headless runner for Peanut-GB (`src/vendor/peanut_gb/`) and Walnut-CGB (`src/engine/walnut_cgb/`) executing `firmware/BmoGameboy/src/assets/roms/tests/cpu_instrs.gb` via serial `SB`/`SC` link-port captures (asserting `"Passed all tests"` within budget, reporting any failing sub-tests, and skipping cleanly with a clear message when the untracked test ROM is absent).
- **Telemetry budget gate (`tools/telemetry_plotter.py`)**:
  - Add `--check-budget <core>` checking that the 600-frame window `p95 <= nominalFramePeriodUs` with unit tests.

### WS5: Documentation, branch isolation, and clean-clone CI findings (`IN_PROGRESS`)

- Documented all findings, decisions, and workstream specifications in this handoff, [`04_known_issues.md`](../../.agents/rules/04_known_issues.md), and [`CHANGELOG.md`](../../CHANGELOG.md).
- **Clean-clone / CI blockers discovered during isolated worktree verification (`fix/production-readiness` at `329162b`)**:
  1. `.gitignore` line 50 (`rom_*.h`) ignores `firmware/BmoGameboy/src/core/rom_file_filter.h` (only `!firmware/BmoGameboy/src/core/rom_index.h` is negated), so `rom_file_filter.h` is missing from git. In the clean worktree, `tests/test_rom_file_filter.py::test_synthetic_sd_listing_samples` errored (`FileNotFoundError`/missing header) and `sd_card.cpp` (`#include "rom_file_filter.h"`) cannot compile on a clean checkout.
  2. `.gitignore` line 45 (`roms/`) ignores `firmware/BmoGameboy/src/assets/roms/virtual_bmo.h`, yet `sd_card.cpp` unconditionally `#include`s `../assets/roms/virtual_bmo.h` and references `virtual_bmo_rom` / `virtual_bmo_rom_size` in `loadRom()` and `freeRom()`. Clean checkouts (including GitHub Actions) cannot compile `sd_card.cpp` without either an optional include guard or a ROM policy fix.
  3. `tests/test_tier1_validation.py::test_07_installed_games_integrity` failed in the clean worktree with `AssertionError: False is not true : games/ directory does not exist` because `games/` is gitignored and absent in CI.
- Committed isolated changes (`tests/host_test_support.py`, `tests/test_host_test_support.py`, documentation/handoff updates, and synchronized index/rules artifacts) onto branch `fix/production-readiness` and pushed to `origin`, leaving the user's pre-existing uncommitted 3D simulator, enclosure, and visualization edits on `main` untouched.

## Remaining open items for next session

1. Execute `WS1` (40 MHz display default + `hw_selftest.*` + `tools/hwtest/`), `WS2` (`SaveManager` fault injection + ASan/UBSan parser fuzzing), `WS3` (Guardian SPI analyzer + `SpiArbiter::Guard` + bounded waits), and `WS4` (64-bit pacer + per-core timing & blargg conformance).
2. Unblock clean-clone CI in [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml): negate `!firmware/BmoGameboy/src/core/rom_file_filter.h` in `.gitignore`, skip `test_07_installed_games_integrity` when `games/` is absent (like `scripts/validate_repo.py` Phase 5), and resolve the unconditional `virtual_bmo.h` include in `sd_card.cpp` per [ADR 0008](../decisions/0008-rom-policy-and-licensing.md).
3. Run on-device physical tests (`docs/hardware_measurements.md`) once a board is connected: 40 vs 80 MHz display stress test, runtime PSRAM/stack headroom, SD clock sweep, physical buttons, and power-cut save verification.
4. Resolve `P2-02` / [ADR 0008](../decisions/0008-rom-policy-and-licensing.md): remove the four commercial ROM headers from git history and `sd_card.cpp`, and audit `src/vendor/` licenses against root `LICENSE`.

## Follow-up — clean-checkout CI fixes (2026-11-12)

- Added the source-only `rom_file_filter.h` exception to `.gitignore` so the firmware include is present in version control.
- Made the ignored, local-only `virtual_bmo.h` an optional include and omitted its catalog entry when unavailable; no ROM assets or policy were changed.
- Made the ignored `rom_data.h` test fixture optional, skipping only the fixture-dependent ROM integrity test when it is absent.
- Changed the installed-game integrity test to skip with an explicit reason when the gitignored `games/` directory is absent.
- Clean-checkout unit checks passed (7 tests, 1 intentional skip) and compile passed (`Sketch uses 5192960 bytes`). The full suite passed (`Ran 182 tests in 63.405s`, `OK`), and `python scripts/validate_repo.py` passed all nine phases in 184897 ms (5,774,632 / 16,777,216 bytes flash; 253,728 / 327,680 bytes SRAM).
- Remote GitHub Actions and physical hardware verification are not claimed; WS1–WS4 and licensing/provenance findings remain unresolved.
