# Wiring — MicroSD Breakout, `GPIO 15` (`SD_MISO`) Safety, and Shared SPI Bus

- **Last Verified Commit:** `d8d4b5e` (`2026-11-15`)
- **Board Target:** `ESP32-S3-DevKitC-1-N16R8` (`ESP32-S3-WROOM-1-N16R8` module). Pin assignments in [`firmware/BmoGameboy/src/hal/config.h`](../../firmware/BmoGameboy/src/hal/config.h) are authoritative.

---

## 1. Shared FSPI Bus Signal Map

| Signal | ESP32-S3 GPIO | Macro in `config.h` | Recommended Passive Component | Notes |
| :--- | ---: | :--- | :--- | :--- |
| **SPI Clock (`SCK`)** | `GPIO 12` | `TFT_SCK` | `22–33 Ω` series resistor at ESP32 pin | Shared by ST7789 Display and MicroSD; keep wire `< 6.0 cm` |
| **SPI Master Out (`MOSI`)** | `GPIO 11` | `TFT_MOSI` | `22–33 Ω` series resistor at ESP32 pin | Shared by ST7789 (`SDA`/`MOSI`) and MicroSD (`DI`/`MOSI`) |
| **SPI Master In (`MISO`)** | `GPIO 15` | `SD_MISO` | None (verify `<= 3.3 V`!) | Connected **only** to MicroSD `DO`/`MISO` (ST7789 is write-only) |
| **Display Chip Select (`CS`)** | `GPIO 10` | `TFT_CS` | **`10 kΩ` pull-up to `3V3`** | Keeps ST7789 deselected (`HIGH`) during ESP32-S3 reset/boot |
| **Display Data/Command (`DC`)** | `GPIO 8` | `TFT_DC` | None | Selects command (`LOW`) vs pixel data (`HIGH`) on ST7789 |
| **Display Reset (`RST`)** | `GPIO 9` | `TFT_RST` | None | Hardware reset for ST7789 |
| **MicroSD Chip Select (`CS`)** | `GPIO 13` | `SD_CS` | **`10 kΩ` pull-up to `3V3`** | Keeps MicroSD deselected (`HIGH`) during ESP32-S3 reset/boot |

Both devices share the ESP32-S3 `FSPI` controller and **must** be serialized by `SpiArbiter::ScopedLock` (`src/hal/spi_arbiter.h`). Configured clocks are `80 MHz` (`40 MHz` in Safe Mode) for the ST7789 and `25 MHz` for the MicroSD card.

---

## 2. Identifying Your MicroSD Module & Solving the `5 V` `MISO` Risk

Hobbyist MicroSD breakouts come in two common styles. Inspect your module before powering it:

### Type A: `3.3 V`-Native MicroSD Socket (or the SD Slot on the Red `2.4"` ST7789 PCB) — **SAFEST / RECOMMENDED**
- **Appearance:** Has **no** black 3-pin voltage regulator (`AMS1117-3.3`) and **no** 14-pin level-shifter IC (`74LVC125A`); only a MicroSD metal socket, `0.1 µF` decoupling capacitor, and `10 kΩ` pull-up resistors. (Note: Many red `2.4"` ST7789 display PCBs already have a `3.3 V`-native MicroSD slot on the back of the display PCB with pins `SD_CS`, `SD_MOSI`, `SD_MISO`, `SD_SCK`!)
- **Power Supply:** Connect `VCC` strictly to **`3V3`** (`3.3 V` pin on ESP32-S3). Never connect a `3.3 V`-native socket to `5V`!
- **`GPIO 15` (`SD_MISO`) Safety:** Guaranteed `<= 3.3 V` because no `5 V` rail is connected to the card or pull-ups.

### Type B: `5 V` Arduino-Style 6-Pin MicroSD Catalex Breakout (`AMS1117-3.3` + `74LVC125A`)
- **Appearance:** Blue or black 6-pin board (`GND, VCC, MISO, MOSI, SCK, CS`) with a 3-pin `AMS1117-3.3` LDO regulator and a 14-pin `74LVC125A` quad buffer.
- **How it works (and two known hardware traps):**
  1. **Dropout Voltage Trap:** The onboard `AMS1117-3.3` LDO requires `>= 4.5 V` on `VCC` to regulate `3.3 V`. If you wire `VCC` of a Type B board to `3V3`, the SD card only receives `~2.2 V` and fails to mount or browns out during reads.
  2. **Shared-Bus / `5 V` Pull-Up Trap:** Well-built Type B clones power the `74LVC125A` buffer from the `3.3 V` LDO output and gate the `MISO` tri-state output (`OE#`) with `SD_CS`. However, cheap clones sometimes wire `10 kΩ` pull-up resistors on `MISO` to `VCC` (`5 V`) or hardwire `MISO` `OE#` to `GND` (so the buffer drives `GPIO 15` continuously even when `SD_CS` is `HIGH`).
- **Mandatory Multimeter Check Before Soldering Permanently:**
  1. Power the board over USB-C and measure `GPIO 15` (`SD_MISO`) to `GND` with a multimeter in DC Volts mode ([`docs/guides/bringup.md`](../guides/bringup.md) Step 3).
  2. **PASS:** `GPIO 15` reads **`<= 3.35 V`**.
  3. **FAIL (`> 3.60 V`):** Unplug immediately and switch to a `3.3 V`-native MicroSD breakout (or the SD slot on the back of the `2.4"` ST7789 display PCB).
