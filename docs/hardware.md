# Hardware Reference

This document separates verified source configuration from physical assumptions and open electrical measurements. The physical board is a permanent perfboard build (`ESP32-S3-WROOM-1-N16R8`). **Evidence level: `TARGET_COMPILED` / `HOST_TESTED`; physical `HARDWARE_TESTED`: none recorded yet.**

See [`firmware/BmoGameboy/src/hal/config.h`](../firmware/BmoGameboy/src/hal/config.h) as the authoritative code definition, [`docs/hardware_measurements.md`](hardware_measurements.md) for the bench test log procedure, and [`docs/decisions/README.md`](decisions/README.md) for ADRs.

## Board Configuration & Safety Hard Stops

| Setting | Configured Value | Status / Constraint |
| :--- | :--- | :--- |
| MCU Module | ESP32-S3-WROOM-1 (`N16R8`) | Dual-core Xtensa LX7 @ 240 MHz, 16 MB Quad SPI Flash, 8 MB Octal SPI PSRAM |
| Arduino Profile / FQBN | `FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi` | **Hard Stop:** Never use `FlashMode=opi` (Quad Flash module) |
| Partitions (`partitions.csv`) | `app0 = 8 MiB` (`0x800000`), `ffat = ~7.9 MiB` (`0x7E0000`) | Leave unchanged (`firmware/BmoGameboy/partitions.csv`) |
| `FEATURE_SD_CARD` | `1` | Shared FSPI bus with ST7789 via `SpiArbiter` |
| `FEATURE_BATTERY_MONITOR` | `0` (Hard Stop) | No divider soldered on `BATTERY_ADC_PIN` (`GPIO 1`); do not read |
| `FEATURE_AUDIO` | `0` (Hard Stop) | No I2S DAC soldered (`GPIO 38/39/40`); do not enable |
| `TFT_SPI_HZ` / Fallback | `80 MHz` default / `40 MHz` Diagnostics comparison | ST7789VW datasheet specifies 62.5 MHz max write cycle (`16 ns`); 80 MHz is unmeasured on physical board |
| `SD_SPI_HZ` | `25 MHz` | Configured nominal mount rate |
| Power Input | USB-C only (current board) | Planned `3xAA NiMH` + 5 V boost converter (`54 x 47 x 16 mm` envelope) not installed or electrically validated (ADR 0006) |

## Pin Assignments (`config.h`)

| Peripheral | Macro | GPIO | Physical Wiring State & Notes |
| :--- | :--- | ---: | :--- |
| Display SCK | `TFT_SCK` | `12` | Wired; shared FSPI clock (`SpiArbiter`) |
| Display MOSI | `TFT_MOSI` | `11` | Wired; shared FSPI MOSI (`SpiArbiter`) |
| Display CS | `TFT_CS` | `10` | Wired |
| Display DC | `TFT_DC` | `8` | Wired |
| Display Reset | `TFT_RST` | `9` | Wired |
| MicroSD CS | `SD_CS` | `13` | Wired; shared FSPI (`SpiArbiter`) |
| MicroSD MISO | `SD_MISO` | `15` | Wired; return line for SD |
| D-Pad | `BTN_UP` / `DOWN` / `LEFT` / `RIGHT` | `4 / 5 / 6 / 7` | Wired active-low (`INPUT_PULLUP`, `GPIO_IN_REG`) |
| Face / System | `BTN_A` / `B` / `START` / `SELECT` | `16 / 17 / 18 / 21` | Wired active-low (`INPUT_PULLUP`, `GPIO_IN_REG`) |
| Extended | `BTN_X` / `Y` / `L` / `R` | `14 / 42 / 47 / 48` | Firmware-supported (`42/47/48` via `GPIO_IN1_REG`), **not soldered on current board** |
| Proposed I2S | `I2S_BCLK` / `LRC` / `DIN` | `38 / 39 / 40` | Not fitted (`FEATURE_AUDIO = 0`) |
| Proposed Battery ADC | `BATTERY_ADC_PIN` | `1` | Not fitted (`FEATURE_BATTERY_MONITOR = 0`) |
| Proposed I2C FRAM | `I2C_SDA` / `SCL` | `43 / 44` | Not fitted (ADR 0007) |
| Reserved | Strapping (`0, 3, 45, 46`), OPI PSRAM (`33–37`) | — | Never assign |

## Known Hardware Risks & Open Bench Checks

1. **ST7789 80 MHz vs 40 MHz Clock:** Toggle `40 MHz` vs `80 MHz` in Diagnostics (`START` -> `A` -> `LEFT`/`RIGHT`) on the real perfboard and record display stability in [`docs/hardware_measurements.md`](hardware_measurements.md) (ADR 0014).
2. **MicroSD 5 V Boot Hypothesis:** A previous boot failure when powering the SD breakout from 5 V is recorded, but `GPIO 15` voltage and the breakout level-shifter schematic have not been measured. Do not modify permanent wiring until `GPIO 15` is measured idle and active under current-limited power.
3. **Physical Wiring & Enclosure Fit:** Details for [Display and Buttons](wiring/01-display-and-buttons.md), [MicroSD and Shared SPI](wiring/02-sd-card.md), and the [3D Printable Enclosure](enclosure-3d-printing.md) (`physical_fit_status = WAITING`) await caliper and bench verification.
