# Wiring — Display and Buttons

Board target: ESP32-S3-N16R8. Pin assignments below reproduce `config.h`; they are not a measurement of signal levels or a complete as-built module schematic.

## Display signals

| TFT signal | GPIO / source | Evidence |
| :--- | :--- | :--- |
| SCK | GPIO12 / `TFT_SCK` | Firmware assignment; shared with SD |
| MOSI | GPIO11 / `TFT_MOSI` | Firmware assignment; shared with SD |
| CS | GPIO10 / `TFT_CS` | Firmware assignment |
| DC | GPIO8 / `TFT_DC` | Firmware assignment |
| Reset | GPIO9 / `TFT_RST` | Firmware assignment |
| VCC, GND, backlight | Not fully recorded | Identify the exact TFT module before changing power wiring |

The firmware configures an 80 MHz display clock and Diagnostics offers 40 MHz for comparison. The ST7789VW datasheet timing lists a 16 ns serial write cycle (62.5 MHz equivalent), so the configured 80 MHz exceeds that published value. No physical display test has been recorded. See [hardware notes](../hardware-notes.md) and [measurement steps](../hardware_measurements.md).

## Button assignments

Eight buttons are recorded as physically wired. X, Y, L, and R are supported in firmware but remain unsoldered. Each configured button uses `INPUT_PULLUP` and is expected to connect its GPIO to ground when pressed; verify switch wiring against the actual board before modification.

| Button | Macro | GPIO | Register read |
| :--- | :--- | ---: | :--- |
| Up | `BTN_UP` | 4 | `GPIO_IN_REG` |
| Down | `BTN_DOWN` | 5 | `GPIO_IN_REG` |
| Left | `BTN_LEFT` | 6 | `GPIO_IN_REG` |
| Right | `BTN_RIGHT` | 7 | `GPIO_IN_REG` |
| A | `BTN_A` | 16 | `GPIO_IN_REG` |
| B | `BTN_B` | 17 | `GPIO_IN_REG` |
| Start | `BTN_START` | 18 | `GPIO_IN_REG` |
| Select | `BTN_SELECT` | 21 | `GPIO_IN_REG` |
| X | `BTN_X` | 14 | `GPIO_IN_REG` |
| Y | `BTN_Y` | 42 | `GPIO_IN1_REG` |
| L | `BTN_L` | 47 | `GPIO_IN1_REG` |
| R | `BTN_R` | 48 | `GPIO_IN1_REG` |

`Buttons::update()` samples the two GPIO register banks sequentially and forms the button state. The two reads are not one cross-bank atomic sample. Pins 33–37 belong to OPI PSRAM, and GPIO 0, 3, 45, and 46 are reserved strapping pins; do not repurpose them.

## Optional and proposed signals

- GPIO1 is reserved for a future battery divider; `FEATURE_BATTERY_MONITOR` must stay `0` until that circuit is installed and verified. Firmware must not read GPIO1 while it is absent.
- GPIO38–40 are proposed I2S signals; no DAC is installed and `FEATURE_AUDIO` must stay `0`.
- GPIO43/44 remain a proposed I2C mapping. No FRAM device or module was selected or wired. The user has not chosen the module or assessed USB-UART interaction.

## Physical layout reference

Coordinates, perfboard pitch, shell dimensions, switch sizes, display opening, and tactile-switch examples previously shown here were design assumptions, not an as-built survey. No drilling or soldering dimensions are provided by this page. Measure the physical board before making a layout change.

## Shared SD bus

The SD card uses SCK GPIO12, MOSI GPIO11, MISO GPIO15, and CS GPIO13. The exact breakout's supply rail and level-shifting circuit are not recorded. See [SD wiring](02-sd-card.md); do not infer its voltage behavior from the firmware configuration.
