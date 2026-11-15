# Architecture Decision Records

These records explain decisions that affect hardware, firmware behavior, storage formats, or project policy. An ADR describes the reason and evidence; the active source remains authoritative for the implemented value (`TARGET_COMPILED` / `HOST_TESTED` as of `d8d4b5e`). `Accepted` means the repository behavior is implemented and active. `Proposed` means the direction is recorded as a design proposal awaiting physical hardware measurements or explicit owner approval in `docs/owner-decisions.md`.

| ADR | Status | Decision |
| :--- | :--- | :--- |
| [0001](0001-hardware-configuration.md) | Accepted | Keep the soldered ESP32-S3 pin map and safety feature flags (`FEATURE_BATTERY_MONITOR=0`, `FEATURE_AUDIO=0`, `FlashMode=qio,PSRAM=opi`) as hard stops. |
| [0002](0002-shared-spi-bus.md) | Accepted | Display and SD share FSPI; all transactions pass through `SpiArbiter::ScopedLock`. |
| [0003](0003-rom-keys-and-index-v2.md) | Accepted | Use `<sanitized_stem>_<8_hex_crc32>` relative-path keys for sidecar metadata (`path_key.h`, `scripts/rom_key.py`). |
| [0004](0004-battery-persistence.md) | Accepted | Persist battery-backed RAM (`.sav`) only when cartridge header metadata and runtime state confirm battery RAM exists. |
| [0005](0005-psram-and-doom-budget.md) | Accepted | Size catalog and DOOM allocations from measured free/largest PSRAM block at runtime with fixed safety margins. |
| [0006](0006-portable-power.md) | Proposed | Rechargeable AA NiMH boost power architecture (2×AA vs 3×AA analysis, protection, and ADC divider proposal). |
| [0007](0007-fram-wiring.md) | Proposed | Document `GPIO 43/44` (`U0TXD`/`U0RXD`) collision with proposed FRAM I2C; keep FRAM disabled (`FEATURE_FRAM=0`). |
| [0008](0008-rom-policy-and-licensing.md) | Accepted | Track zero commercial ROMs or scrapers (`BMO_ENABLE_BAKED_ROMS=0`); separate personal-use `default` build from redistributable `mit` build (`THIRD_PARTY_NOTICES.md`). |
| [0009](0009-emulator-exit-confirmation.md) | Accepted | `SELECT+UP` opens the quit prompt; two deliberate `A` presses within 3 seconds exit cleanly after flushing battery RAM. |
| [0010](0010-simulator-boundary.md) | Accepted | Browser WASM / Wokwi simulation and ESP32-S3 hardware execution are separate environments; simulator results never establish `HARDWARE_TESTED`. |
| [0011](0011-index-path-storage.md) | Accepted | Store catalog entries in `BMOIDX03` (`44`-byte header, `20`-byte `BmoIndexEntry`, variable-length UTF-8 path string pool, 4 KiB prefix CRC32). |
| [0012](0012-save-state-schema.md) | Accepted | Store save states in `BSS2` (`72`-byte `SaveStateFormat::HeaderV2` with `coreId`, `pathKeyHash`, `romSize`, and payload CRC32) with legacy `BSS1` read support. |
| [0013](0013-partition-layout.md) | Proposed | Preserve `partitions.csv` hard stop; propose allocating the unpartitioned `64 KiB` flash tail (`0xFF0000`) to `coredump`. |
| [0014](0014-display-spi-clock.md) | Proposed | Analyze `80 MHz` vs `40 MHz` ST7789 SPI wire time, perfboard series termination, and Safe Mode fallback pending bench measurements. |

New records must state the status, context, decision, alternatives, consequences, evidence level, and any owner approval required.
