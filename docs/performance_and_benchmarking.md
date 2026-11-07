# Performance and benchmarking guide

## Evidence types

Keep four kinds of evidence distinct:

1. **Source configuration:** constants, allocations, and call paths in current firmware.
2. **Analytical model:** estimates such as `bytes × 8 / SPI_Hz`; excludes task timing, commands, conversion, bus gaps, and controller limits.
3. **Host execution:** Python tests, native C/C++ runners, and host microbenchmarks. These do not predict ESP32 timing.
4. **Physical measurement:** serial logs, scope/meter results, and panel behavior collected on the assembled board and recorded in `docs/hardware_measurements.md`.

## Current frame timing

`FramePacing::targetFrameUs()` gives nominal budgets of 16,742 us for GB/GBC, 28,571 us for DOOM, 33,333 us for the PICO-8-style runtime, and 16,667 us for other wrapper indices. PAL/region detection is not implemented. The main loop samples 600 execution durations per window; p50/p95/p99 cover wrapper/core execution before the pacing wait and do not isolate the display worker or scan-out.

The optional `BmoProfiler` CCOUNT service defaults off and is not called by the main frame loop. `tools/telemetry_plotter.py` accepts captured serial logs; no-input mode generates synthetic records and must not be described as a device measurement.

## Current memory and bus analysis

Run `python -m tools.guardian psram-budget` for a source-derived allocation ledger and DOOM zone bounds. Runtime free space, largest contiguous block, and transient emulator allocations require device readings. Run `python -m tools.guardian bus-calc` for analytical SPI transfer estimates. The display is configured for 80 MHz, above the ST7789VW's published 62.5 MHz serial-write timing; Diagnostics includes a 40 MHz comparison. No physical comparison has been recorded.

Run `python -m tools.guardian bench-host` for host-only timing. Preserve its platform and command context when reporting results. The old issue IDs PERF-01 through PERF-25 and associated numerical claims are historical and are not an active benchmark ledger.

## Measurement procedure

Collect boot and SD mount timing, largest supported ROM load, free/largest PSRAM before and during emulator launch, stack high-water marks, raw frame telemetry by core, and SPI observations. Use the capture template and record the board, power source, SD module, panel, firmware revision, and instrument. Never infer physical results from the source budget or the 3D simulator.

The prior benchmark report is preserved at `docs/archive/performance-report-2026-11-03.md`. It contains earlier claims and is not current evidence.
