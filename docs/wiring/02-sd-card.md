# Wiring — MicroSD and Shared SPI

Board target: ESP32-S3-N16R8. The pin assignments and feature flags in [`config.h`](../../firmware/BmoGameboy/src/core/config.h) are authoritative. This page records configured signal routing; it does not claim measured electrical levels or device performance.

## Signal map

| Signal | GPIO | Configuration | Notes |
| :--- | ---: | :--- | :--- |
| SCK | 12 | `TFT_SCK` | Shared by display and SD |
| MOSI | 11 | `TFT_MOSI` | Shared by display and SD |
| Display CS | 10 | `TFT_CS` | Display select |
| Display DC | 8 | `TFT_DC` | Display command/data |
| Display reset | 9 | `TFT_RST` | Display reset |
| SD MISO | 15 | `SD_MISO` | SD return data; display is write-only in this design |
| SD CS | 13 | `SD_CS` | SD select |

The configured display clocks are 80 MHz by default and 40 MHz in the Diagnostics comparison. The configured SD clock is 25 MHz. Both devices share FSPI and must be protected by `SpiArbiter`; do not use a separate SPI object or bypass the arbiter.

The physical SD breakout model, VCC rail, level shifter, and MISO high level are not recorded. A boot failure with the breakout powered from 5 V was reported, but its cause has not been measured. See [`hardware.md`](../hardware.md) for the measurement procedure. Do not assume that GPIO15 sees 5 V, and do not change the permanent wiring based only on that hypothesis.

## Firmware initialization

The current initialization in `DisplayEmu::begin()` and `SDCard::begin()` is the implementation source of truth. The display and SD use the shared `SPI` instance, with `SpiArbiter` serializing bus access. If the pin configuration changes, update `config.h`, wiring documentation, the manifest, and the hardware tests together.

## Checks before a hardware change

1. Record the SD breakout model or schematic and its supply wiring.
2. With the board powered safely, measure GPIO15 idle and during a card read. Record the measured range and instrument used.
3. Compare the reset log with the SD card removed and inserted.
4. Run the firmware Diagnostics SPI pattern at 80 MHz and 40 MHz after the board is stable.
5. Record results in [`hardware_measurements.md`](../hardware_measurements.md).

No physical test result is currently recorded.
