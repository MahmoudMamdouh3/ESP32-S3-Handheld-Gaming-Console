# SDD Section 03: GPIO Assignment and Feature Flags

[Back to the SDD index](../software-design-document.md)

## 3.1 Source and evidence

[`config.h`](../../firmware/BmoGameboy/src/core/config.h) is the single source of configured GPIO values and hardware feature flags. The physical wiring status below is copied from [Rule 01](../../.agents/rules/01_hardware.md), which records the user's soldered build. A configured peripheral is not proof of electrical measurements or behavior.

## 3.2 Display and shared SD SPI

| Macro | GPIO | Configured owner / use | Wiring status |
| :--- | ---: | :--- | :--- |
| `TFT_SCK` | 12 | ST7789 SCK; shared with SD | Soldered |
| `TFT_MOSI` | 11 | ST7789 MOSI; shared with SD | Soldered |
| `TFT_RST` | 9 | ST7789 reset | Soldered |
| `TFT_DC` | 8 | ST7789 data/command | Soldered |
| `TFT_CS` | 10 | ST7789 chip select | Soldered |
| `SD_MISO` | 15 | SD card data to ESP32 | Soldered; voltage behavior unmeasured |
| `SD_CS` | 13 | SD chip select | Soldered |
| `TFT_WIDTH` / `TFT_HEIGHT` | 240 / 320 | Driver's configured dimensions before rotation | ST7789 module recorded as landscape in use |
| `TFT_SPI_HZ` | 80,000,000 | Default display clock | Configured; timing comparison and assumptions in [Section 07.1](section-07.md#71-panel-configuration-and-published-timing) |
| `TFT_SPI_FALLBACK_HZ` | 40,000,000 | Diagnostics comparison | Configured comparison only |
| `SD_SPI_HZ` | 25,000,000 | SD transactions | Configured; not an observed bus trace |

The devices share clock/data and must not transact concurrently. Every display/SD access must follow the `SpiArbiter` contract described in [ADR 0002](../decisions/0002-shared-spi-bus.md).

## 3.3 Buttons

All buttons are active-low inputs using `INPUT_PULLUP` and a connection to ground on press. Eight base controls are recorded as soldered; X/Y/L/R are supported by software but not soldered.

| Macro | GPIO | Mapping | Wiring status |
| :--- | ---: | :--- | :--- |
| `BTN_UP` | 4 | D-pad up | Soldered |
| `BTN_DOWN` | 5 | D-pad down | Soldered |
| `BTN_LEFT` | 6 | D-pad left | Soldered |
| `BTN_RIGHT` | 7 | D-pad right | Soldered |
| `BTN_A` | 16 | A | Soldered |
| `BTN_B` | 17 | B | Soldered |
| `BTN_START` | 18 | Start | Soldered |
| `BTN_SELECT` | 21 | Select | Soldered |
| `BTN_X` | 14 | X | Not soldered |
| `BTN_Y` | 42 | Y | Not soldered; GPIO bank 1 read |
| `BTN_L` | 47 | Left shoulder | Not soldered; GPIO bank 1 read |
| `BTN_R` | 48 | Right shoulder | Not soldered; GPIO bank 1 read; check board LED multiplexing before any wiring |

`Buttons::update()` uses direct GPIO register reads for responsive polling. Do not replace the hardware map with simulator controls or assume an unwired input is a working control.

## 3.4 Disabled or planned peripherals

| Macro(s) | GPIO | Feature | State and hard stop |
| :--- | ---: | :--- | :--- |
| `BATTERY_ADC_PIN` | 1 | Battery monitor | No divider; `FEATURE_BATTERY_MONITOR` must remain `0`; never sample GPIO1 |
| `I2S_BCLK`, `I2S_LRC`, `I2S_DIN` | 38, 39, 40 | I2S DAC | No DAC recorded; `FEATURE_AUDIO` remains `0` |
| `I2C_SDA`, `I2C_SCL` | 43, 44 | Optional FRAM | Proposed only; no device or wiring plan; leave pins unchanged |

`FEATURE_SD_CARD=1`; `FEATURE_AUDIO=0`; `FEATURE_BATTERY_MONITOR=0`. These exact settings are enforced by repository checks and are covered by [ADR 0001](../decisions/0001-hardware-configuration.md).

## 3.5 Reserved pins and build profile

- GPIO33–37 are reserved by the N16R8 module's octal PSRAM connection.
- GPIO0, GPIO3, GPIO45, and GPIO46 are avoided strapping pins.
- The board profile uses QIO flash and OPI PSRAM; `FlashMode=opi` is forbidden.
- See [hardware state](section-02.md), [`config.h`](../../firmware/BmoGameboy/src/core/config.h), and [`partitions.csv`](../../firmware/BmoGameboy/partitions.csv) before changing any physical setting.
