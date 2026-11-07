# SDD Section 02: Hardware State, Wiring, and Constraints

[Back to the SDD index](../software-design-document.md)

This section distinguishes configuration from physical evidence. The source of pin and feature-flag values is [`config.h`](../../firmware/BmoGameboy/src/core/config.h); the current soldered state is recorded in [Rule 01](../../.agents/rules/01_hardware.md). Firmware values do not prove that a peripheral is present or electrically safe.

## 2.1 Target module and build profile

The firmware targets an ESP32-S3-WROOM-1 N16R8 module with Quad-SPI flash and Octal-SPI PSRAM. The project build profile is:

```text
esp32:esp32:esp32s3:CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi,USBMode=default
```

`FlashMode=qio` and `PSRAM=opi` are separate settings. OPI flash mode is forbidden. CPU frequency is pinned to the recorded 240 MHz option and `USBMode=default` selects USB-OTG TinyUSB CDC with CDC on boot. The project partition table is [`partitions.csv`](../../firmware/BmoGameboy/partitions.csv). Current build-size measurements must come from a successful target compile; the retained ELF under an ignored build directory is not proof for the current source tree.

## 2.2 Recorded physical assembly

| Subsystem | Recorded physical state | Firmware state |
| :--- | :--- | :--- |
| Power | USB-C only; no battery or voltage divider | Battery monitor disabled |
| Display | ST7789 connected to shared SPI | 320×240 landscape output configured |
| Controls | Eight base buttons wired; X/Y/L/R are not soldered | Twelve active-low controls supported |
| SD card | MicroSD connected to shared SPI | `FEATURE_SD_CARD=1` |
| Audio | I2S DAC not fitted | `FEATURE_AUDIO=0` |
| FRAM | Not fitted | GPIO43/44 remain a proposed I2C mapping |

No physical hardware check was performed for this document update. The exact GPIO assignment table is in [Section 03](section-03.md).

The firmware sets the selected Arduino CDC implementation's TX timeout to zero after `Serial.begin()`. Log output is best-effort and may be truncated when the host is absent or cannot drain the USB buffer. The pinned core source and scope are documented in [Section 14.2](section-14.md#142-target-compilation).

## 2.3 Shared SPI and display

Display and SD share SCK GPIO12 and MOSI GPIO11. They use separate chip selects: display GPIO10 and SD GPIO13. SD return data is on GPIO15. `SpiArbiter` serializes access to this FSPI bus. The display default is `TFT_SPI_HZ=80 MHz`; the Diagnostics comparison is `TFT_SPI_FALLBACK_HZ=40 MHz`; SD is configured at `SD_SPI_HZ=25 MHz`.

These are software clock settings. See the single timing comparison and its unverified module assumption in [Section 07.1](section-07.md#71-panel-configuration-and-published-timing), and the [hardware measurement procedure](../hardware_measurements.md).

The repository records a boot failure with an SD breakout powered from 5 V. The MISO level and the failure cause have not been measured. The explanation that MISO back-powers the 3.3 V rail is unverified; do not treat it as established hardware behavior or change the permanent wiring based on that explanation.

## 2.4 Power and optional hardware

The selected portable-power direction is three AA NiMH cells with a regulated 5 V boost converter. The battery holder, converter model, current margin, switching, charging, and USB power-path design remain unspecified. This is a design decision, not installed hardware.

GPIO1 is reserved for a future battery divider. `FEATURE_BATTERY_MONITOR` must remain `0` and firmware must not read GPIO1 while the divider is absent. Audio remains disabled until an I2S DAC and its wiring are physically installed and reviewed. FRAM remains uninstalled; GPIO43/44 are a proposal and will not be reassigned without a reviewed wiring plan.

## 2.5 Enclosure references

The browser simulator contains a 3D enclosure model and the wiring pages contain proposed perfboard coordinates. Those coordinates describe design references only. They are not independently measured dimensions of the physical perfboard, panel, switch spacing, shell, or battery compartment.
