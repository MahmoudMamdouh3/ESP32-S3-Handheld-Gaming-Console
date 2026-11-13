# Hardware Notes

This page separates configuration facts from observations and proposed wiring. The current board is a permanent perfboard build, so firmware must follow the existing wiring. The pin map in [`config.h`](../firmware/BmoGameboy/src/core/config.h) and [hardware rule](../.agents/rules/01_hardware.md) is the source of truth. This page was consistency-reviewed on 2026-11-12; no new electrical measurements were taken.

## Current assembly

| Item | Recorded state | Evidence level |
| :--- | :--- | :--- |
| MCU module | ESP32-S3-WROOM-1 N16R8; QIO flash and OPI PSRAM settings | Board configuration and project build target |
| Power | USB-C only; no battery or voltage divider installed | Physical state recorded in Rule 01; not re-measured in this session |
| Display | ST7789, shared SPI; configured for 320×240 landscape | Firmware configuration; panel behavior still needs a device check |
| Buttons | Eight base buttons wired; X/Y/L/R supported by firmware but not wired | Physical state recorded in Rule 01 |
| SD storage | MicroSD shares SCK/MOSI with the display | Firmware wiring map; electrical module details are unknown |
| Audio / battery monitor | Not fitted; both feature flags stay `0` | Hard stop; do not enable or access GPIO1 |
| FRAM | Not fitted; GPIO43/44 are only a proposed I2C assignment | Not wired or tested |

## Pins and bus rates

Pin assignments are listed only in `config.h` and Rule 01. The display default is `TFT_SPI_HZ` (80 MHz), its diagnostics comparison is `TFT_SPI_FALLBACK_HZ` (40 MHz), and the SD clock is `SD_SPI_HZ` (25 MHz). These are configured values, not measured board limits.

The firmware arbitrates the shared FSPI bus with `SpiArbiter`. Do not add another SPI user without updating the lock contract and checking every early return.

The ST7789VW v1.0 datasheet gives a 16 ns serial write cycle (62.5 MHz equivalent). The configured 80 MHz period is 12.5 ns. This is an open timing risk; no perfboard comparison has been recorded. The display diagnostics pattern can compare 80 MHz with 40 MHz: open Diagnostics from the console carousel with `START`, press `A` for the pattern, use `LEFT`/`RIGHT` to toggle the clock, press `A` to return to the dashboard, and `B` to leave. Source: [ST7789VW datasheet](https://dl.espressif.com/dl/schematics/ST7789VW_datasheet.pdf).

## SD module observations

The repository records a boot failure when an SD breakout was powered from 5 V. The MISO voltage and the cause have not been measured, so the earlier explanation that the module drives 5 V into GPIO15 is a hypothesis, not an established electrical fact. Do not add a resistor, clamp, or alternate wiring to the permanent board based on that hypothesis alone.

Before changing the SD power or signal wiring, record the exact module and its schematic, then measure GPIO15 relative to ground while the module is idle and during an SD read. Use an oscilloscope or a meter with a suitable input range; do not connect a 5 V signal to an ESP32-S3 input. Record the supply rail and reset log with the card removed and inserted. No measurement is recorded in this repository yet.

## Power plan

The selected portable-power direction is **three AA NiMH cells feeding a regulated 5 V boost converter**. The cell holder, converter model, current capability, switching, charging, USB power-path isolation, and physical battery monitor remain design work. This plan does not mean the battery is installed. Keep `FEATURE_BATTERY_MONITOR=0`; GPIO1 must remain unread until its divider is physically installed and checked. Do not enable audio.

## Board settings

Use the FQBN configured by the repository validator:

```text
esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi
```

The physical module has Quad-SPI flash and Octal-SPI PSRAM. Never select OPI flash mode. The custom partition file is [`partitions.csv`](../firmware/BmoGameboy/partitions.csv); its contents were not changed in this remediation.

## Enclosure and perfboard

The [parametric enclosure guide](enclosure-3d-printing.md) describes the WebGL model, seven STL parts, GLB assembly, clearances, and the caliper survey. The 82 × 124 × 26 mm shell, 2.1 mm walls, board envelopes, battery bay, and display profiles are design inputs only. Both the board survey and physical-fit state are `WAITING`; do not slice a final shell or change the permanent wiring from these dimensions. The current wiring remains the source of truth. A pin reassignment, FRAM installation, battery holder, new power rail, or connector change requires a documented wiring proposal and a physical review before code or pin configuration changes.

## Human measurement record

Use [`hardware_measurements.md`](hardware_measurements.md) for raw logs and measured values. Every field begins as `WAITING`; firmware telemetry and analytical estimates must not be presented as physical measurements.
