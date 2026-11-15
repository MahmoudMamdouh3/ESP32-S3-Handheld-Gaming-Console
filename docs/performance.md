# Performance, Analytical Budgets, and Benchmarking Guide

- **Updated:** `2026-11-15` (`HOST_TESTED` / `TARGET_COMPILED` analytical model; physical capture pending)

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
4. Record electrical readings, SPI `80 MHz` vs `40 MHz` panel stability, FreeRTOS stack high-water marks (in 4-byte words), and PSRAM measurements in [`docs/hardware_measurements.md`](hardware_measurements.md) and [`docs/benchmarks/README.md`](benchmarks/README.md). Look closely at the pass criteria (`>= 99%` of `>= 600` frames within `target_us` and `p99_us <= target_us`) before promoting any platform from `Experimental` to `Supported` in [`docs/compatibility.md`](compatibility.md).

## 6. Analytical Models & Deferred Optimizations (`NOT_TESTED` Until Bring-Up)

- **Cold Boot & Catalog Scan Scaling (`BMOIDX03`):**
  - *Cold boot to menu:* Currently dominated by a blocking `3,000 ms` boot splash (`bmo_app.cpp`) plus ST7789 init (`~150–250 ms`), SD mount, and `BMOIDX03` load. Post-bring-up target budget on a warm index is **`<= 3.0 s`** total boot-to-menu (requires converting the splash delay to a non-blocking timer, `F11`).
  - *Catalog scan estimates (`25 MHz` SD SPI; `~56 B/entry` in `BMOIDX03` vs FAT32 traversal):*
    - `256` ROMs (`~14 KiB` index): `~5 ms` indexed read vs `~100–300 ms` cold FAT32 scan (analytical estimate, `NOT_TESTED`).
    - `2,500` ROMs (`~140 KiB` index): `~45 ms` indexed read vs `~1–3 s` cold FAT32 scan (analytical estimate, `NOT_TESTED`).
    - `36,000` ROMs (worst-case large collection, `~2.0 MiB` index): `~0.65 s` raw wire read + parse (`~1.0–1.5 s` estimated total) vs `~15–45 s` cold FAT32 directory walk (analytical estimate, `NOT_TESTED`).
- **Input-to-Display Latency (`NOT_TESTED`):** `20 ms` debounce (`buttons.h`) + up to `16.7 ms` emulation step (`60 Hz`) + `10.4–15.4 ms` Core 0 SPI write (`80 MHz`) gives an analytical `~47–67 ms` button-to-glass window (`+10–15 ms` if running at `40 MHz` Safe Mode).
- **Menu Frame Cost, Dirty-Rectangles & Power-Aware Scaling (`NOT_TESTED`):**
  - *Full-screen menu push (`320×240` = `153,600 B`):* `15.36 ms` wire time at `80 MHz` leaves `7.8%` wire headroom inside a `16.67 ms` `60 Hz` budget (`30.72 ms` at `40 MHz`, max `~32.5 FPS`).
  - *Dirty-rect + `30 FPS` menu cap (Deferred):* Restricting idle mascot draws to the `160×160` central region (`51,200 B`, `5.12 ms` @ `80 MHz`) and capping menu loops at `30 FPS` (`33.33 ms`) cuts menu SPI duty cycle by `~3×–6×`.
  - *Power-aware scaling (Deferred / `Decision 6`):* Lowering ESP32-S3 CPU clock (`setCpuFrequencyMhz`) during static menus/mascot sleep and dimming/gating `BLK` via a future `GPIO 2` transistor stage (**Decision 6**) are queued until USB/bench current draw is measured.
- **Adaptive Frameskip Evaluation:** Firmware currently implements **no** automatic frameskip (`SELECT + RIGHT` fast-forward only skips frame-pacing sleep). Implementing render-skip (`skipDisplay`) for heavy 16-bit cores (`SNES`, `Genesis`, `PCE`, `OSWAN`, `NGP`) involves trading visual `30 FPS` smoothness against audio/timing determinism and is deferred until `BMO_PROFILE=1` traces show which cores miss `16,667 us` on silicon.
- **Display SPI `vs.` Future Revision SDMMC Note:** Sharing FSPI (`GPIO 11/12/15`) between display and SD serializes reads/writes via `SpiArbiter`. Future PCB revisions could move microSD onto a dedicated 4-bit `SDMMC` bus (freeing FSPI exclusively for the panel) without changing emulator interfaces.
