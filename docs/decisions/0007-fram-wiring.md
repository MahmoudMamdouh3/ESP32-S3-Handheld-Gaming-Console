# ADR 0007: UART0 Collision on Proposed FRAM Pins (`GPIO 43/44`) and PWM Backlight Pin

- Status: Proposed (FRAM disabled via `FEATURE_FRAM=0`; `config.h` pin values preserved as hard stop)
- Date: 2026-11-07 (Updated: 2026-11-15, verified at `d8d4b5e`)
- Decision owner: User (`docs/owner-decisions.md`)

## 1. Context & Pin Collision Finding

1. `firmware/BmoGameboy/src/hal/config.h` defines `I2C_SDA = 43` and `I2C_SCL = 44` under a "Planned/Optional FRAM" comment, and `firmware/BmoGameboy/src/storage/fram_save.cpp` contains an uncalled `FramSave` scaffold.
2. On the **ESP32-S3** (`ESP32-S3 Datasheet` v1.9, Table 2-3) and the **`ESP32-S3-DevKitC-1-N16R8`** carrier board, **`GPIO 43` is `U0TXD` and `GPIO 44` is `U0RXD`**, which are hardwired to the onboard USB-to-UART bridge (`CP2102N` / `CH343P`) and emit ROM bootloader logs on every reset (`Serial.begin(115200)`).
3. Calling `Wire.begin(43, 44, 400000)` reconfigures the GPIO matrix for `GPIO 43/44` as open-drain I2C lines, breaking `UART0` serial logging and risking bus contention with the onboard USB-UART bridge TX line.
4. No physical FRAM module exists in the hardware build, and MicroSD `BMOIDX03` / `BSS2` / `.sav` persistence already handles all save data (`ADR 0004`, `ADR 0012`).

## 2. Display Backlight (`BLK`) Power Saving Opportunity

Currently, the `2.4"` ST7789 display backlight (`BLK` / `LED`) is tied continuously to `3V3`, drawing `~60–90 mA` at `100%` brightness with no ability to dim in menus or turn off during idle mascot sleep.

Inspecting all 45 physical GPIOs on the `ESP32-S3-WROOM-1-N16R8` module confirms that **`GPIO 2`** and **`GPIO 41`** are completely free, non-strapping, non-PSRAM GPIOs exposed on the DevKitC-1 headers. Driving an N-channel logic-level MOSFET (`AO3400A` or `2N7000` with a `100 Ω` gate resistor and `100 kΩ` gate-to-GND pull-down) from **`GPIO 2`** via ESP32-S3 `LEDC` PWM (`20 kHz`) would allow software brightness control (`10–100%`) and idle backlight sleep (`saving ~50–80 mA`).

## 3. Decision

1. **Do NOT wire I2C FRAM to `GPIO 43/44`.** Gate `fram_save.cpp` behind `#if defined(FEATURE_FRAM) && FEATURE_FRAM` (default `0`) so `Wire.h` and dead I2C routines are not compiled into the firmware image.
2. Recommend retiring the FRAM proposal entirely (`owner-decisions.md`), or moving any future I2C expansion to `GPIO 2` / `GPIO 41`.
3. Recommend reserving **`GPIO 2`** for optional PWM backlight dimming (`TFT_BL`) in a future hardware revision once initial bench bring-up is complete.
