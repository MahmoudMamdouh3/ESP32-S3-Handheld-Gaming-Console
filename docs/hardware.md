# Hardware Reference

- **Last Verified Commit:** `d8d4b5e` (`2026-11-15`)

This document separates verified source configuration (`TARGET_COMPILED` / `HOST_TESTED`) from physical assumptions and open electrical measurements (`NOT_TESTED`). **Physical `HARDWARE_TESTED` records: none recorded yet.**

See [`firmware/BmoGameboy/src/hal/config.h`](../firmware/BmoGameboy/src/hal/config.h) as the authoritative pin/flag definition, [`docs/hardware_measurements.md`](hardware_measurements.md) for the bench measurement log, and [`docs/decisions/README.md`](decisions/README.md) for ADRs.

## 1. Board Identity, Configuration & Safety Hard Stops

The soldered unit uses a **44-pin `ESP32-S3-DevKitC-1-N16R8` development carrier board** (`2.54 cm × 6.33 cm`, 0.254 cm header pitch, dual USB-C ports labeled `UART/COM` and `USB/OTG`, onboard 3.3 V LDO regulator, and `BOOT`/`RST` buttons) carrying the **`ESP32-S3-WROOM-1-N16R8` SMD module** (`1.80 cm × 2.55 cm`).

| Setting | Configured Value | Status / Constraint |
| :--- | :--- | :--- |
| Carrier Board | `ESP32-S3-DevKitC-1-N16R8` (44-pin) | `2.54 cm × 6.33 cm`, dual USB-C (`UART/COM` via USB-serial bridge on `GPIO 43/44`; `USB/OTG` on `GPIO 19/20`), soldered to `0.254 cm` pitch perfboard |
| SMD Radio/Memory Module | `ESP32-S3-WROOM-1-N16R8` | Dual-core Xtensa LX7 @ 240 MHz, 16 MB **Quad SPI** Flash, 8 MB **Octal SPI** PSRAM |
| Arduino Profile / FQBN | `FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi` | **Hard Stop:** Never use `FlashMode=opi` (the N16R8 flash is Quad SPI; `FlashMode=opi` causes a boot loop) |
| Partitions (`partitions.csv`) | `app0 = 8 MiB` (`0x800000`), `ffat = ~7.9 MiB` (`0x7E0000`), `64 KiB` unallocated tail at `0xFF0000` | **Hard Stop:** Leave unchanged until owner approves [ADR 0013](decisions/0013-partition-layout.md) |
| `FEATURE_SD_CARD` | `1` | Shared FSPI bus with ST7789 via `SpiArbiter::ScopedLock` |
| `FEATURE_BATTERY_MONITOR` | `0` (Hard Stop) | No voltage divider soldered on `BATTERY_ADC_PIN` (`GPIO 1`); do not read |
| `FEATURE_AUDIO` | `0` (Hard Stop) | No I2S DAC soldered (`GPIO 38/39/40`); do not enable |
| `TFT_SPI_HZ` / Fallback | `80 MHz` default / `40 MHz` Safe Mode & Diagnostics fallback | Sitronix ST7789VW datasheet specifies `62.5 MHz` max write cycle (`16 ns`); `80 MHz` is unmeasured on physical perfboard ([ADR 0014](decisions/0014-display-spi-clock.md)) |
| `SD_SPI_HZ` | `25 MHz` | Configured nominal SD mount rate |
| Power Input | USB-C (`5 V`) only on current board | Planned rechargeable AA NiMH (2×AA or 3×AA) + 5 V boost converter is not yet wired or measured ([ADR 0006](decisions/0006-portable-power.md)) |

## 2. Pin Assignments (`config.h`) & ESP32-S3 GPIO Budget

| Peripheral | Macro | GPIO | Physical Wiring State & Special Function Notes |
| :--- | :--- | ---: | :--- |
| Display SCK | `TFT_SCK` | `12` | Wired (`FSPICLK`); shared FSPI clock (`SpiArbiter`) |
| Display MOSI | `TFT_MOSI` | `11` | Wired (`FSPID`); shared FSPI MOSI (`SpiArbiter`) |
| Display CS | `TFT_CS` | `10` | Wired (`FSPICS0`); add 10 kΩ pull-up to `3V3` recommended so panel stays deselected during reset |
| Display DC | `TFT_DC` | `8` | Wired (`SUBSPICS1` / general GPIO / `ADC1_CH7`) |
| Display Reset | `TFT_RST` | `9` | Wired (`FSPIHD` / general GPIO / `ADC1_CH8`) |
| MicroSD CS | `SD_CS` | `13` | Wired (`FSPIQ`); shared FSPI (`SpiArbiter`); add 10 kΩ pull-up to `3V3` recommended |
| MicroSD MISO | `SD_MISO` | `15` | Wired (`XTAL_32K_P` / `ADC2_CH4`); return line for SD (`<= 3.3 V` only!) |
| D-Pad | `BTN_UP` / `DOWN` / `LEFT` / `RIGHT` | `4 / 5 / 6 / 7` | Wired active-low (`INPUT_PULLUP`, `GPIO_IN_REG`, `ADC1_CH3..CH6`) |
| Face / System | `BTN_A` / `B` / `START` / `SELECT` | `16 / 17 / 18 / 21` | Wired active-low (`INPUT_PULLUP`, `GPIO_IN_REG`) |
| Extended Face | `BTN_X` / `BTN_Y` | `14 / 42` | Configured (`42` via `GPIO_IN1_REG`; `GPIO 42` is also JTAG `MTMS`), **not soldered on current board** |
| Shoulder | `BTN_L` / `BTN_R` | `47 / 48` | Configured (`47/48` via `GPIO_IN1_REG`; note `GPIO 48` drives the onboard WS2812 RGB LED on DevKitC-1 v1.0), **not soldered on current board** |
| Proposed I2S | `I2S_BCLK` / `LRC` / `DIN` | `38 / 39 / 40` | Not fitted (`FEATURE_AUDIO = 0`; note `GPIO 38` drives the onboard RGB LED on DevKitC-1 v1.1, and `39/40` are JTAG `MTCK`/`MTDO`) |
| Proposed Battery ADC | `BATTERY_ADC_PIN` | `1` | Not fitted (`FEATURE_BATTERY_MONITOR = 0`, `ADC1_CH0`) |
| Legacy Proposed FRAM | `I2C_SDA` / `SCL` | `43 / 44` | **Collision Warning:** `GPIO 43` (`U0TXD`) and `GPIO 44` (`U0RXD`) are hardwired to the DevKitC-1 USB-UART bridge! Do **not** wire I2C FRAM here ([ADR 0007](decisions/0007-fram-wiring.md)) |
| Free / Available GPIOs | — | `2, 41` (`19, 20` if native USB-OTG unused) | Candidate pins for future transistor-driven PWM display backlight (`BLK`) dimming |
| Reserved (Never Assign) | Strapping (`0, 3, 45, 46`), OPI PSRAM (`33–37`), SPI Flash (`26–32`) | — | Enforced by compile-time `static_assert` in `src/hal/pins_check.h` and `validate_repo.py` |

## 3. Wokwi Simulation Diagram (`hardware/diagram.json`)

[`hardware/diagram.json`](../hardware/diagram.json) is a simulation-only schematic for Wokwi (`ADR 0010`):
- It uses `wokwi-ili9341` as a stand-in part because Wokwi lacks a native `240×320` ST7789 component; physical hardware uses a `2.4"` (`6.10 cm` diagonal) ST7789 SPI display.
- It wires all 12 configured buttons (`UP/DOWN/LEFT/RIGHT/A/B/START/SELECT` plus expansion `X/Y/L/R`), whereas only the first 8 buttons are soldered on the current physical perfboard.

## 4. Known Hardware Risks & Open Bench Checks

1. **ST7789 `80 MHz` vs `40 MHz` Clock ([ADR 0014](decisions/0014-display-spi-clock.md)):** Toggle `40 MHz` vs `80 MHz` in Diagnostics (`START` -> `A` -> `LEFT`/`RIGHT`) on the real perfboard and record display stability in [`docs/hardware_measurements.md`](hardware_measurements.md).
2. **MicroSD `5 V` Level-Shifter / `GPIO 15` Check:** Measure `GPIO 15` (`SD_MISO`) idle and active voltage under current-limited power before modifying permanent SD power wiring.
3. **Physical Caliper Survey:** All module, switch, and battery bay dimensions in [`docs/enclosure-3d-printing.md`](enclosure-3d-printing.md) (`physical_fit_status = WAITING`) await physical caliper verification in centimetres/millimetres.
