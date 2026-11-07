# SDD Section 11: Disabled peripherals, diagnostics, and telemetry

[Back to the SDD index](../software-design-document.md)

## 11.1 Hardware feature gates

`config.h` is the source of truth for optional hardware. `FEATURE_BATTERY_MONITOR` and `FEATURE_AUDIO` are both `0`. GPIO1 is not wired to a battery divider, and no I2S DAC is installed. The battery UI therefore reports `NOT FITTED`; audio functions are unavailable. Do not enable either feature based on the presence of its source module.

GPIO43/44 appear as proposed I2C assignments, but FRAM is not installed and those signals have not been measured against the board's USB-UART bridge. The current portable power direction is three AA NiMH cells with a boost converter; no converter, protection circuit, or enclosure wiring has been selected. See [hardware notes](../hardware-notes.md) and ADRs 0006 and 0007.

## 11.2 Diagnostics screen

See the canonical Diagnostics description in [Section 5.5](section-05-part-02.md#55-diagnostics). Firmware logs the stack high-water values every five seconds while the screen is open.

## 11.3 Frame telemetry

The main loop logs per-second FPS and elapsed core-frame execution average/minimum/maximum. After each non-overlapping 600-sample window it sorts a fixed array and logs nearest-rank p50/p95/p99 (ranks 300/570/594) plus the selected emulator index and nominal target. Sort and print work occur after the sample's elapsed time is captured and before the next frame starts. These percentiles measure wrapper/core execution before the outer pacing wait. They do not isolate SPI, display worker time, panel scan-out, or input latency. `python -m unittest discover -s tests -p 'test_frame_percentiles.py' -v` tests equal, ascending, descending, outlier, and invalid-size cases against the shared helper.

`profiler.h/.cpp` provides an optional CCOUNT zone profiler with `FEATURE_PROFILER` defaulting to `0`; the main loop does not invoke its zones. The live 600-frame telemetry is separate. `tools/telemetry_plotter.py` parses the two serial record formats; when run without an input file it uses labeled synthetic data, not device captures.
