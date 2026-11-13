# Performance & Analytical Budgets

## Evidence Classification

All timing and memory figures in this repository distinguish:

1. `NOT_TESTED` — analytical calculation or source-configured constant not executed on hardware.
2. `HOST_TESTED` — measured or checked on x86_64/Windows/Linux CI runners (`tests/`, `host_cores_runner.c`). Does not predict ESP32-S3 speed.
3. `TARGET_COMPILED` — Flash and static SRAM numbers emitted by `xtensa-esp32s3` linking (`arduino-cli compile --profile esp32s3`).
4. `HARDWARE_TESTED` — physical serial logs, frame windows, or oscilloscope measurements from the soldered ESP32-S3 board. **None are recorded as of 2026-11.**

## Source-Configured Frame Pacing (`frame_pacing.h`)

`FramePacing::targetFrameUs(index)` sets outer loop pacing budgets (bypassed in fast-forward `SELECT + RIGHT`; `HOST_TESTED` via `tests/test_frame_pacing.py`):

| Wrapper Index | Platform | Target Period | Source Rationale |
| ---: | :--- | ---: | :--- |
| `0, 1` | GBC (`WalnutEmu`), GB (`PeanutEmu`) | `16,742 us` | Game Boy master clock `4,194,304 / 70,224` Hz |
| `3` | DOOM (`DoomEmu`, opt-in) | `28,571 us` | `TICRATE = 35` |
| `8` | PICO-8-style runtime (`PicoEmu`) | `33,333 us` | `30 Hz` nominal host presentation cadence |
| Other indices | All other cores | `16,667 us` | Nominal `60 Hz` NTSC-style default |

- PAL region detection and absolute 64-bit deadlines are not yet implemented.
- On device, the main loop records unpaced wrapper execution samples over 600-frame windows (`p50` / `p95` / `p99` parsed via nearest-rank in `frame_percentiles.h`, `HOST_TESTED`).

## Analytical Bus & Memory Commands

- **SPI Bus Transfer Estimate (`NOT_TESTED` physics model):** `python -m tools.guardian bus-calc` computes raw byte wire time (`bytes * 8 / SPI_Hz`) for each platform viewport at 80 MHz SPI. It excludes CS toggles, DC switching, pixel format expansion, SD contention, and CPU decode cost.
- **Static & PSRAM Allocation Ledger (`HOST_TESTED` parser):** `python -m tools.guardian psram-budget` derives persistent PSRAM allocations and DOOM `computeZoneBytes()` bounds (`2.5–6 MiB` with `512 KiB` margin). Runtime PSRAM fragmentation requires physical serial telemetry (`heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)`).
- **Compiled ELF Report (`TARGET_COMPILED` when freshly built):** `python -m tools.guardian profile-elf --elf <path-to.elf>` and `python -m tools.guardian budget --elf <path-to.elf>` report Xtensa section totals and top symbol sizes.

## Collecting Real Device Measurements

1. Flash a verified `TARGET_COMPILED` build onto the board and capture serial output at `115200` baud while running target consoles.
2. Preserve `FPS:` and `Frame window:` lines and analyze them with `python tools/telemetry_plotter.py --input capture.txt --json-out summary.json` (note: running `telemetry_plotter.py` without `--input` emits synthetic test data and must never be cited as hardware evidence).
3. Record electrical, SPI 80 vs 40 MHz panel stability, stack high-water marks (in words), and PSRAM measurements in [`docs/hardware_measurements.md`](hardware_measurements.md).
