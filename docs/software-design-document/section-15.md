# SDD Section 15: Performance configuration and measurement limits

[Back to the SDD index](../software-design-document.md)

## 15.1 Source-configured frame targets

`FramePacing::targetFrameUs(index)` returns these nominal host-loop delays. This is the canonical pacing table; Section 5.6 links here.

| Wrapper index | Target | Source rationale |
| ---: | ---: | :--- |
| 0, 1 (GBC, GB) | 16,742 us | `4,194,304 / 70,224` Hz from the Game Boy core timing constants |
| 3 (DOOM) | 28,571 us | `TICRATE=35` in the vendored DOOM timer |
| 8 (PICO-8-style runtime) | 33,333 us | Configured 30 Hz presentation target |
| Other indices | 16,667 us | Default 60 Hz pacing |

`tests/test_frame_pacing.py` parses `frame_pacing.h` and checks the returned values and rate derivations.

These are not region-aware. They are not a measurement of the emulated core, display queue, SPI transfer, panel scan-out, or end-to-end latency. The host loop's frame execution time is measured separately by firmware and the 600-sample p50/p95/p99 logger. Fast-forward bypasses the pacing wait.

## 15.2 Memory and SPI models

`python -m tools.guardian psram-budget` derives persistent PSRAM allocations from the source declarations and reports the DOOM zone bounds. It cannot account for runtime fragmentation or every transient core allocation; the firmware logs free and largest-free PSRAM and derives the requested DOOM zone from the measured largest block at launch.

The configured display frequency and datasheet timing are compared in [Section 7.1](section-07.md#71-panel-configuration-and-published-timing). A 40 MHz diagnostics comparison and 25 MHz SD clock are configured. No physical bus waveform, panel stability, or mounted-card speed measurement is recorded. Bus transfer calculations are analytical models only.

## 15.3 Available measurements and missing evidence

- Firmware serial telemetry reports FPS intervals and unpaced core execution timing.
- `tools/telemetry_plotter.py` parses captured logs and labels its built-in sample stream as synthetic.
- `python -m tools.guardian bench-host` is a host benchmark and does not predict ESP32 timing.
- `profiler.h/.cpp` is optional and is not called from the main loop.
- `docs/hardware_measurements.md` is waiting for user-collected device logs and electrical observations.

No benchmark numbers from archived reports should be treated as current measurements. The previous report is preserved as a dated archive in `docs/archive/performance-report-2026-11-03.md` for historical context only.
