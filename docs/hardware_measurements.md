# Hardware Measurement Record

This document is the procedure and evidence format for measurements on the soldered handheld. As of 2026-11-12, no physical board measurements are recorded in this repository. Firmware configuration, host tests, target compilation, and simulators do not count as device measurements.

## Evidence requirements

Each result must include the board revision or a photo reference, date, firmware commit/build flags, instrument make/model, probe location, test procedure, acceptance criterion, raw value or captured log, and operator. Mark a result `NOT RUN`, `PASS`, `FAIL`, or `INCONCLUSIVE`; do not convert a design value into a measurement.

Store completed records under `docs/hardware-measurements/` using `YYYY-MM-DD-short-test-name.md`. Keep raw serial captures and oscilloscope exports beside the record. Do not include ROMs, save data, serial numbers, or personal information in repository commits unless explicitly reviewed.

## Safe measurement order

1. Record the unpowered wiring and module labels. Do not move pins or attach a battery pack while this is incomplete.
2. Confirm the ESP32-S3 module label and board power input limits from its exact manufacturer documentation.
3. Use a current-limited bench supply for first power checks. The chosen portable direction is 3×AA NiMH cells plus a boost converter, but the converter, output rail, wiring, charging method, fuse, and runtime have not been selected or validated. Do not connect cells directly to the board's 5 V or 3.3 V rail.
4. Measure shared SPI signals with both chip-selects observed. Begin with the diagnostics 40 MHz display comparison and the configured 25 MHz SD clock; leave the configured 80 MHz display setting unchanged until the physical signal and panel behavior are recorded.
5. Capture at least 600 consecutive emulator frames per selected core and keep the serial output. The firmware prints per-second FPS / execution summaries and a 600-sample p50/p95/p99 execution window.
6. Record the reported free PSRAM and largest free PSRAM block before launching a memory-heavy title and after exiting it. Record diagnostics stack high-water values with their units (words).
7. Probe SD MISO at the ESP32 input under idle, selected, and transfer states using a high-impedance instrument. The current module model and its output-voltage mechanism are unknown. Do not add a resistor, divider, or level shifter until the measured circuit and the module specification support that choice.
8. FRAM is not installed. GPIO43/44 remain the proposal in `config.h`; no FRAM probe or persistence claim is valid until the exact part, voltage, address, and wiring are recorded.

## Firmware telemetry

Capture the serial output at 115200 baud from boot through the test interval. Preserve lines beginning with `FPS:` and `Frame window:`. Analyze a capture with:

```powershell
python tools/telemetry_plotter.py --input path\to\serial-capture.txt --json-out path\to\summary.json
```

The `Frame window` percentiles cover unpaced emulator execution time. The per-second `FPS` is observed loop throughput, and `target` is the configured nominal pacing budget. Neither number measures panel signal integrity or input latency by itself.

## Record template

Copy [`hardware-measurement-template.md`](hardware-measurement-template.md) for each test. The machine-readable blank record is [`hardware-measurement-template.json`](hardware-measurement-template.json).
