# Roadmap

Items below reflect verified open work as of commit `d8d4b5e` (`2026-11-15`). No timeline guarantees are implied.

## 1. Hardware Bring-Up & Physical Measurements (Blocking Hardware Verification)

- Follow `docs/guides/bringup.md` and record initial bench measurements in [`docs/hardware_measurements.md`](docs/hardware_measurements.md):
  - Verify unpowered continuity across `GND`, `3V3`, `5V`, and `GPIO 15` (`SD_MISO`).
  - Identify fitted MicroSD breakout schematic and measure `SD_MISO` (`GPIO 15`) voltage under idle and active SD reads before modifying wiring.
  - Flash the minimal bring-up sketch (`firmware/bringup/bringup.ino`) to verify ST7789 color bars, 8 wired buttons, SD mount, and 8 MiB Octal PSRAM before flashing full firmware.
  - Compare ST7789 stability on the physical perfboard at `80 MHz` (`TFT_SPI_HZ`) vs `40 MHz` (`TFT_SPI_FALLBACK_HZ`) using Diagnostics.
  - Capture 600-frame serial telemetry (`p50`/`p95`/`p99`), free/largest contiguous PSRAM blocks, and FreeRTOS task stack high-water marks on physical hardware.
- Complete physical caliper survey in [`docs/enclosure-3d-printing.md`](docs/enclosure-3d-printing.md) (`WAITING` -> measured centimetre/millimetre values) before slicing final STL shells.
- Select boost converter, slide switch, polyfuse, power-path isolation, and 2×AA vs 3×AA NiMH cell holder per [`docs/decisions/0006-portable-power.md`](docs/decisions/0006-portable-power.md).

## 2. Firmware & Host Verification

- **Emulator Lifecycle & Fault Injection (`HOST_TESTED`):** Expand fake SD/display backend tests for power-loss save recovery during atomic rename, catalog card-swap/subdir freshness detection, and emulator teardown timeout bounds.
- **Full-Cartridge & Hardware Conformance (`HARDWARE_TESTED` pending):** Exercise redistributable homebrew/conformance ROM suites across the 13 upstream cores in `firmware/BmoGameboy/src/third_party/` on physical ESP32-S3 hardware once bench measurements are captured.

## 3. Already Implemented in Current Firmware (`TARGET_COMPILED` / `HOST_TESTED`)

- **Core 0 Async Frame Streaming Across All Wrappers (Including GB/GBC):** `PeanutEmu` (`src/cores/emu_peanut.cpp`) and `WalnutEmu` (`src/cores/emu_walnut.cpp`) scale scanlines into PSRAM ping-pong buffers (`DisplayEmu::acquireAsyncFrameBuffer()`) and submit frames asynchronously (`DisplayEmu::submitGBFrameAsync()`) to the Core 0 display worker.
- **Versioned Save-State Schema (`BSS2`):** `SaveStateFormat::HeaderV2` (`72` bytes, `MAGIC_V2 = "BSS2"`, `coreId`, `pathKeyHash`, `romSize`, `stateSize`, `ramSize`, `crc32`) in `src/storage/save_state_format.h` and `src/storage/save_manager.cpp`, with backward-compatible `BSS1` (`60`-byte) v1 loading (`ADR 0012`).
- **Variable-Length String-Pool Catalog Index (`BMOIDX03`):** `44`-byte `BmoIndexHeader` + `20`-byte `BmoIndexEntry` records + UTF-8 path string pool + 4 KiB ROM prefix CRC32 in `src/storage/rom_index.h` and `src/storage/sd_catalog.cpp` (`ADR 0011`).
- **Genuine Upstream Emulator Cores (15 Platforms):** 13 upstream engines in `src/third_party/` with host WASI/CTest execution suites (`tests/test_native_cores_execution.py`).
- **Shared SPI Bus Arbitration & Boot Safe Mode:** RAII `SpiArbiter::ScopedLock` (`src/hal/spi_arbiter.h`) across SD, box-art, save, and display paths plus `[SAFE_MODE]` boot override (`SELECT` or `B` held at power-on, or 3 consecutive unconfirmed boots via NVS `bmo_boot`, forces `40 MHz` fallback SPI clock).
- **Cartridge Battery Persistence Gating:** SNES LoROM/HiROM/ExHiROM checksum-complement SRAM detection (`battery_save.h`, `emu_snes.cpp`) and PC Engine `"HUBM"` BRAM header gating (`emu_pce.cpp`).
