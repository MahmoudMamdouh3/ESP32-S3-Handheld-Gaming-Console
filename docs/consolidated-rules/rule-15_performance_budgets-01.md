# Section 15 — Performance Budgets and Measurements

Source: [`15_performance_budgets.md`](../../.agents/rules/15_performance_budgets.md).

# Performance Budgets and Measurements

Performance claims must identify whether they are source configuration, an analytical model, host-only timing, or physical measurement. Never copy a host or bus-model number into a hardware result.

## Current pacing configuration

FramePacing::targetFrameUs() currently returns 16,742 us for wrapper indices 0 and 1 (GB/GBC), 28,571 us for index 3 (DOOM), 33,333 us for index 8 (PICO-8-style runtime), and 16,667 us for remaining indices. These are nominal outer-loop targets. PAL/region timing detection is not implemented.

The loop records pre-wait core execution durations in a static 600-sample window and logs p50/p95/p99. It does not separate CPU, SPI, display worker, or panel scan-out time. The optional CCOUNT profiler defaults disabled and is not called in the main frame loop.

## Memory sizing

Use python -m tools.guardian psram-budget to regenerate the source-derived PSRAM allocation report. It includes pool capacity, visible-menu arrays, persistent display buffers, and a DOOM zone bound. Runtime fragmentation and transient core allocations require a device reading. The firmware calculates DOOM's request from the largest free PSRAM block, subtracts a fixed margin, and clamps to configured floor/cap values.

Do not maintain a second hand-entered allocation total here. Add new persistent allocations to the analyzer and update its generated report/test when source changes.

## Measurement requirements

Before claiming a speed or memory improvement, record the target or host, tool, input, number of frames/iterations, and measured values. Physical timing and power values belong in docs/hardware_measurements.md. Current known open items are in 04_known_issues.md and docs/performance_and_benchmarking.md.