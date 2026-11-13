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

- **Core 0 Async Streaming for GB/GBC:** Route Peanut-GB and Walnut-CGB scanlines through Octal PSRAM staging buffers to the Core 0 display worker so Core 1 is not blocked on synchronous `DisplayEmu::streamPixelRow` SPI writes.
- **Versioned Save-State Schema (`BSS2`):** Design and implement `BSS2` versioned save-state metadata and core-specific header checks across SD save slots (ADR 0012).
- **Emulator Lifecycle & Fault Injection:** Add fake SD/display backend tests for power-loss save recovery, catalog card-swap/subdir freshness detection, and emulator teardown timeout bounds.
- **Full-Cartridge & Hardware Conformance:** Exercise commercial/homebrew ROM suites across the 13 genuine upstream cores in `firmware/BmoGameboy/src/third_party/` on physical ESP32-S3 hardware once bench measurements are captured.

### Already Implemented in Current Firmware (`TARGET_COMPILED` / `HOST_TESTED`)

- Genuine upstream third-party cores across all 15 platforms (`src/third_party/`, enabled by default via `BMO_EXPERIMENTAL_CORES=1` and `BMO_ENABLE_DOOM=1`) and all 11 host C/C++ WASI core execution tests (`tests/test_native_cores_execution.py`).
- RAII `SpiArbiter::ScopedLock` (`src/hal/spi_arbiter.h`) across SD, box-art, save, and display paths plus `[SAFE_MODE]` boot override (`SELECT` or `B` held at power-on forces `40 MHz` fallback SPI clock).
- SNES LoROM/HiROM/ExHiROM checksum-complement SRAM detection (`battery_save.h`, `emu_snes.cpp`) and PC Engine `"HUBM"` BRAM header gating (`emu_pce.cpp`).
