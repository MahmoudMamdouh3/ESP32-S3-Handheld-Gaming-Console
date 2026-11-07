# Section 01 — 1. Physically Wired Hardware (Verified Ground Truth)

Source: [`01_hardware.md`](../../.agents/rules/01_hardware.md).

# 1. Physically Wired Hardware (Verified Ground Truth)
Only the following hardware is *currently, physically soldered and wired* to the ESP32-S3:
- **Power:** USB-C power only. (No battery, no TP4056 module, no voltage divider yet).
- **Display:** ST7789 display (2.4", 240x320) wired to the shared FSPI bus.
- **Input:** 8 standard Game Boy-layout tactile buttons wired with internal pull-ups (`INPUT_PULLUP`).
- **Input (pending wiring):** 4 extra buttons (X, Y, L, R) are supported in firmware and the simulator but are not soldered yet. Unwired pins read released because of `INPUT_PULLUP`.
- **Storage:** SD card module wired to the shared FSPI bus.
- **Audio:** None. (No MAX98357A DAC, no I2S wiring).

---

# 2. Pin Map
This is the single source of truth for all pin assignments, synchronized with `config.h`. No other file should hardcode a pin number.

| Pin | Function / Macro | Module / Owner | Constraints / Notes |
| :--- | :--- | :--- | :--- |
| **0, 3, 45, 46** | *Strapping Pins* | System | **DO NOT USE** for buttons. Affects boot mode. |
| **33-37** | *Internal PSRAM* | System | **DO NOT USE**. Reserved internally for octal PSRAM. |
| **4** | `BTN_UP` | `buttons.cpp` | `INPUT_PULLUP` (reads LOW when pressed). |
| **5** | `BTN_DOWN` | `buttons.cpp` | `INPUT_PULLUP` |
| **6** | `BTN_LEFT` | `buttons.cpp` | `INPUT_PULLUP` |
| **7** | `BTN_RIGHT` | `buttons.cpp` | `INPUT_PULLUP` |
| **16** | `BTN_A` | `buttons.cpp` | `INPUT_PULLUP` |
| **17** | `BTN_B` | `buttons.cpp` | `INPUT_PULLUP` |
| **18** | `BTN_START` | `buttons.cpp` | `INPUT_PULLUP` |
| **21** | `BTN_SELECT` | `buttons.cpp` | `INPUT_PULLUP` |
| **14** | `BTN_X` | `buttons.cpp` | `INPUT_PULLUP`. Extra face button (SNES X, Genesis C, DOOM next weapon). Not yet soldered. |
| **42** | `BTN_Y` | `buttons.cpp` | `INPUT_PULLUP`. Read from `GPIO_IN1_REG` (pin above 31). Not yet soldered. |
| **47** | `BTN_L` | `buttons.cpp` | `INPUT_PULLUP`. Left shoulder trigger. Read from `GPIO_IN1_REG`. Not yet soldered. |
| **48** | `BTN_R` | `buttons.cpp` | `INPUT_PULLUP`. Right shoulder trigger. Read from `GPIO_IN1_REG`. Not yet soldered. On many DevKitC-1 boards GPIO48 also drives the onboard RGB LED. |
| **8** | `TFT_DC` | `display_emu.cpp` | Data/Command pin for ST7789. |
| **9** | `TFT_RST` | `display_emu.cpp` | Reset pin for ST7789. |
| **10** | `TFT_CS` | `display_emu.cpp` | Chip Select for ST7789 (FSPI). |
| **11** | `TFT_MOSI` | `display_emu.cpp` / SD | Shared FSPI MOSI. |
| **12** | `TFT_SCK` | `display_emu.cpp` / SD | Shared FSPI SCK. |
| **13** | `SD_CS` | `sd_card.cpp` | Chip Select for SD Card. |
| **15** | `SD_MISO` | `sd_card.cpp` | Shared FSPI MISO. |
| **1** | `BATTERY_ADC_PIN` | *Dormant (battery.cpp)* | Reserved for future voltage divider. |
| **38** | `I2S_BCLK` | *Dormant (audio_i2s.cpp)* | Reserved for future I2S DAC. |
| **39** | `I2S_LRC` | *Dormant (audio_i2s.cpp)*| Reserved for future I2S DAC. |
| **40** | `I2S_DIN` | *Dormant (audio_i2s.cpp)*| Reserved for future I2S DAC. |
| **43** | `I2C_SDA` | *Planned (fram_save.cpp)*| Reserved for future I2C FRAM. |
| **44** | `I2C_SCL` | *Planned (fram_save.cpp)*| Reserved for future I2C FRAM. |

---

# 3. Software Feature Flags & Dormant Modules
All peripheral modules must be gated by `FEATURE_*` flags in `config.h`. A disabled feature must compile to a safe no-op.

- **`FEATURE_SD_CARD` (Current: `1`)**: Gates `sd_card.cpp`. The SD card is physically present and enabled.
- **`FEATURE_BATTERY_MONITOR` (Current: `0`)**: Gates `battery.cpp`. **Note:** A complete, realistic driver implementation exists in `battery.cpp`. It is dormant. It is safe to read/modify, but MUST NEVER be enabled until the physical voltage divider is confirmed soldered in Section 1.
- **`FEATURE_AUDIO` (Current: `0`)**: Gates `audio_i2s.cpp` and emulator APU callbacks. **Note:** A complete driver implementation exists and is dormant. Do NOT enable until physical I2S hardware is verified in Section 1.

---

# 4. Verified ESP32-S3 Flash & PSRAM Configuration
- **Module Hardware:** ESP32-S3-WROOM-1 (N16R8 variant with Quad SPI Flash and Octal SPI PSRAM).
- **Verified Working Arduino IDE Settings (Ground Truth):**
  - `Flash Mode:` **`"QIO 80MHz"`** (CRITICAL: Setting to OPI causes a black screen because the flash chip is Quad SPI).
  - `Flash Size:` **`"16MB (128Mb)"`**
  - `PSRAM:` **`"OPI PSRAM"`** (Octal SPI PSRAM in PSRAM mapped window).
  - `Partition Scheme:` **`"Custom"`** (`partitions.csv`).
  - `CPU Frequency:` **`"240MHz (WiFi)"`**