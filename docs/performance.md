# Performance, Analytical Budgets, and Benchmarking Guide

- **Last Verified Commit:** `d8d4b5e` (`2026-11-15`)

## 1. Evidence Classification

All timing and memory figures in this repository strictly distinguish four levels:

1. `NOT_TESTED` — analytical calculation (such as `bytes * 8 / SPI_Hz`) or source-configured constant not executed on physical hardware.
2. `HOST_TESTED` — measured or checked on host/CI runners (`tests/`, `tests/host/`, `tests/test_native_cores_execution.py`, `python -m tools.guardian bench-host`). Host execution does **not** predict ESP32-S3 speed.
3. `TARGET_COMPILED` — Flash, IRAM, and static DRAM numbers emitted by `xtensa-esp32s3` linking (`python tasks.py build` + `python tasks.py size`).
4. `HARDWARE_TESTED` — physical serial logs, 600-frame telemetry windows, multimeter readings, or oscilloscope captures from the soldered ESP32-S3 perfboard recorded in [`docs/hardware_measurements.md`](hardware_measurements.md). **None are recorded as of 2026-11-15.**

> **Historical Note:** Legacy issue identifiers `PERF-01` through `PERF-25` from early development notes are retired and are not an active benchmark ledger.

## 2. Source-Configured Frame Pacing (`frame_pacing.h`)

`FramePacing::targetFrameUs(index)` sets outer loop pacing budgets on CPU core 1 (bypassed during fast-forward `SELECT + RIGHT`; `HOST_TESTED` via `tests/test_frame_pacing.py`):

| Wrapper Index | Platform | Target Period | Source Rationale |
| ---: | :--- | ---: | :--- |
| `0, 1` | GBC (`WalnutEmu`), GB (`PeanutEmu`) | `16,742 us` (`59.73 Hz`) | Game Boy master clock `4,194,304 / 70,224` Hz |
| `3` | DOOM (`DoomEmu`, `BMO_ENABLE_DOOM=1`) | `28,571 us` (`35.00 Hz`) | `TICRATE = 35` |
| `8` | PICO-8 (`PicoEmu` / `fake08`) | `33,333 us` (`30.00 Hz`) | `30 Hz` nominal PICO-8 cart presentation cadence |
| Other indices | All other emulator cores | `16,667 us` (`60.00 Hz`) | Nominal `60 Hz` NTSC-style default |

- PAL region detection is not implemented.
- All active emulator wrappers — including Game Boy (`PeanutEmu`) and Game Boy Color (`WalnutEmu`) via PSRAM ping-pong buffers (`DisplayEmu::acquireAsyncFrameBuffer()` / `submitGBFrameAsync()`) — queue finished frames to the CPU core 0 `BMO_Display` worker task (`TARGET_COMPILED`; `NOT_TESTED` on physical hardware).
- On device, the main loop records unpaced wrapper execution samples over 600-frame windows (`p50` / `p95` / `p99` parsed via nearest-rank in `frame_percentiles.h`, `HOST_TESTED`).

## 3. On-Device Profiler (`BmoProfiler`)

The optional `BmoProfiler` service (`python tasks.py build --profile profiler-compile-only`, which sets `-DBMO_PROFILE=1`) defaults off and compiles to zero overhead in default builds (`TARGET_COMPILED`). When enabled, it:

- Records CPU core 0 and CPU core 1 zone durations using `esp_timer_get_time()`,
- Maintains a 600-frame sliding ring (`min` / `avg` / `max` / `p50` / `p95` / `p99`),
- Displays live metrics in the on-screen Diagnostics Profiler screen (`SELECT`), and
- Emits `[PROFILE_CSV]` serial records alongside `[INFO] FPS:` and `[INFO] Frame window:` logs.

## 4. Analytical Bus & Memory Commands

- **SPI Bus Transfer Estimate (`NOT_TESTED` physics model):**
  ```powershell
  python -m tools.guardian bus-calc
  ```
  Computes raw byte wire time (`bytes * 8 / SPI_Hz`) for each platform viewport at `80 MHz` vs `40 MHz` SPI. It excludes CS toggles, DC switching, pixel format expansion, SD contention, and CPU decode cost.
- **Static & PSRAM Allocation Ledger (`HOST_TESTED` parser):**
  ```powershell
  python -m tools.guardian psram-budget
  ```
  Derives persistent PSRAM allocations and DOOM `computeZoneBytes()` bounds (`2.5–6 MiB` with `512 KiB` margin). Runtime PSRAM fragmentation requires physical serial telemetry (`heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)`).
- **Compiled ELF Report (`TARGET_COMPILED`):**
  ```powershell
  python tasks.py size --check-budgets
  python -m tools.guardian profile-elf --elf build/target-compile/BmoGameboy.ino.elf
  ```

## 5. Collecting Real Device Measurements (`HARDWARE_TESTED` Protocol)

1. Complete the unpowered and voltage checks in [`docs/hardware-validation-kit.md`](hardware-validation-kit.md).
2. Flash a verified `TARGET_COMPILED` build (or `profiler-compile-only` build) onto the board and capture serial output at `115200` baud while running each target console for at least 600 frames (`~10–20` seconds).
3. Parse `FPS:`, `Frame window:`, and `[PROFILE_CSV]` lines with:
   ```powershell
   python tools/telemetry_plotter.py --input capture.txt --json-out summary.json
   ```
   *(Never run `telemetry_plotter.py` without `--input` for reporting; no-input mode emits synthetic demonstration data only.)*
4. Record electrical readings, SPI `80 MHz` vs `40 MHz` panel stability, FreeRTOS stack high-water marks (in 4-byte words), and PSRAM measurements in [`docs/hardware_measurements.md`](hardware_measurements.md) and [`docs/benchmarks/README.md`](benchmarks/README.md).
