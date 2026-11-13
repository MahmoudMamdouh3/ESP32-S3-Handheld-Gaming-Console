# Roadmap

Items below reflect verified open work after the Phase 1–3 repository audit and cleanup. No timeline guarantees are implied.

## Hardware Measurements (Blocking Hardware Verification)

- Record initial bench measurements in [`docs/hardware_measurements.md`](docs/hardware_measurements.md):
  - Identify fitted MicroSD breakout schematic and measure `SD_MISO` (`GPIO 15`) voltage under idle and 5 V vs 3.3 V supply before modifying wiring.
  - Compare ST7789 stability on the physical perfboard at `80 MHz` (`TFT_SPI_HZ`) vs `40 MHz` (`TFT_SPI_FALLBACK_HZ`) using Diagnostics.
  - Capture 600-frame serial telemetry (`p50`/`p95`/`p99`), free/largest contiguous PSRAM blocks, and FreeRTOS task stack high-water marks on physical hardware.
- Complete physical caliper survey in `docs/enclosure-3d-printing.md` (`WAITING` -> measured values) before slicing final STL shells.
- Select boost converter, switch, power-path isolation, and Z-clearance resolution for the planned `3xAA NiMH` battery holder (ADR 0006).

## Firmware & Core Concurrency

- **SPI Bus Lock Safety & Clock Recovery:** Add an RAII `SpiArbiter::ScopedLock` guard across SD and display callers and a boot-button safe mode (`SELECT` or `B` held at power-on) forcing 40 MHz display fallback.
- **Core 0 Async Streaming for GB/GBC:** Route Peanut-GB and Walnut-CGB scanlines through Octal PSRAM staging buffers to the Core 0 display worker so Core 1 is not blocked on synchronous SPI writes.
- **Save Format & Battery Persistence Hardening:** Support SNES HiROM/ExHiROM checksum-complement header detection and PC Engine `"HUBM"` BRAM header verification; design `BSS2` versioned save-state schema (ADR 0012).
- **Emulator Lifecycle & Fault Injection:** Add fake SD/display backend tests for power-loss save recovery, catalog card-swap/subdir freshness detection, and emulator teardown timeout bounds.
- **Core Verification:** Graduate experimental first-party cores (`BMO_EXPERIMENTAL_CORES`) through structured ROM conformance tests before enabling them by default.
