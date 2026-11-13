# Hardware Benchmarks & Telemetry Capture Guide

> **Evidence Status: `NOT_TESTED` on physical ESP32-S3 hardware.**  
> No on-device frame-time measurements, serial captures, or SPI oscilloscope captures are recorded yet. Do not copy analytical figures (`python -m tools.guardian bus-calc`) or host/WASM test runs into the CSV table below as device results.

## 1. Build with `BMO_PROFILE=1`

Default firmware compiles with `-DBMO_PROFILE=0` (`FEATURE_PROFILER=0`), so profiling macros compile out cleanly with zero Flash, DRAM, or CPU overhead.

To compile firmware with `esp_timer_get_time()` microsecond telemetry, the 600-frame sliding ring buffer, Diagnostics `SELECT` profiler page, and `[PROFILE_CSV]` Serial output enabled:

```powershell
python tasks.py build --profile profiler-compile-only --warnings strict --fail-on-warnings
```

Or pass `-DBMO_PROFILE=1` explicitly during compile/upload:

```powershell
.\arduino-cli.exe compile --profile esp32s3 `
  --build-property compiler.c.extra_flags=-DBMO_PROFILE=1 `
  --build-property compiler.cpp.extra_flags=-DBMO_PROFILE=1 `
  firmware/BmoGameboy
```

## 2. How Pipeline Telemetry Works (`NOT_TESTED` on Device)

- **Core 1 Emulator Frame (`ZONE_EMU_FRAME`):** Measures unpaced emulator execution inside `handleEmulator()` before frame pacing waits. For Game Boy (`PeanutEmu`) and Game Boy Color (`WalnutEmu`), scanlines now scale into the inactive 16-bit PSRAM ping-pong buffer (`DisplayEmu::acquireAsyncFrameBuffer()`) and hand off to Core 0 via `DisplayEmu::submitGBFrameAsync()`, matching NES/DOOM/16-bit cores.
- **Core 0 SPI Display Worker (`ZONE_SPI_STREAM`):** Measures `SPI.writeBytes(...)` transfer duration in `displayWorkerTask` on Core 0.
- **600-Frame Ring & Percentiles:** Nearest-rank `min`, `avg`, `max`, `p50`, `p95`, `p99` over the trailing 600 frames (`HOST_TESTED` in [`profiler_test.cpp`](../../tests/host/profiler_test.cpp)).
- **On-Device View:** Press `START` in the Console Menu to open Hardware Diagnostics, then press `SELECT` to toggle the **Frame Profiler** screen.

## 3. Capture & Parse Real Serial Logs

1. Flash the `profiler-compile-only` (`-DBMO_PROFILE=1`) firmware to the soldered ESP32-S3 board.
2. Open Serial at `115200` baud and capture >= 60 seconds per console core after gameplay starts.
3. Preserve lines starting with `[PROFILE_CSV_HDR]`, `[PROFILE_CSV]`, `[INFO] FPS:`, and `[INFO] Frame window:`.
4. Parse into summary JSON/text with [`telemetry_plotter.py`](../../tools/telemetry_plotter.py):

```powershell
python tools/telemetry_plotter.py --input capture.txt --output report.txt --json-out summary.json
```

5. Record verified rows in a dated copy of [`template.csv`](template.csv) alongside [`../hardware_measurements.md`](../hardware_measurements.md). Never pass `--input`-less `telemetry_plotter.py` output (`SYNTHETIC`) as hardware telemetry.
