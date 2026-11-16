# BMO Handheld — Plain-English Owner Guide

Welcome! This guide is written specifically for a first-time hardware builder with **zero programming or electronics background**. Every technical term is explained in plain English at the bottom of this page, and every physical measurement is in **centimetres (`cm`)** (except the `2.4"` screen diagonal).

---

## 1. Where Your Project Stands Today (Honest Status)

- **What is finished and verified on the computer (`HOST_TESTED` / `TARGET_COMPILED`):**
  - The handheld software ("firmware") compiles cleanly for your `ESP32-S3-DevKitC-1-N16R8` board.
  - A tiny, safe **First-Power-On Bring-Up Sketch** (`firmware/bringup/bringup.ino`) is ready to test your soldered screen, SD card, and 8 buttons one piece at a time.
  - A complete **31 × 46 hole (`7.87 × 11.68 cm`) perfboard placement map** ([`docs/wiring/perfboard-layout.md`](../wiring/perfboard-layout.md)) and **point-to-point soldering table** ([`docs/wiring/wire_list.csv`](../wiring/wire_list.csv)) have been generated for an **`8.80 × 12.40 × 3.20 cm`** pocketable shell.
- **What has NOT been tested yet (`0 HARDWARE_TESTED` records):**
  - The software has **never** run on your physical soldered board yet. Computer tests prove the code compiles and its math works, but only plugging in and testing your real board proves the wires, solder joints, and screen speed work physically.
- **Important Note About Your `hardware-notes.md` File:**
  - **Keep your `hardware-notes.md` note** — do **not** delete it. Your note recorded a real boot loop under `FlashMode=qio` while `opi` booted. That conflicts with Espressif's *ESP32-S3-WROOM-1 Datasheet* (`v1.4` Table 1, [`espressif.com`](https://www.espressif.com/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf), accessed `2026-11-15`), which specifies `16 MB` **Quad SPI** flash (`QIO`) + `8 MB` **Octal SPI** PSRAM (`OPI`). Let the read-only `esptool flash-id` / `chip-id` check in [`bringup.md` Step 3](bringup.md#step-3-read-only-esptool-v531-chip--flash-verification-before-flashing-anything) settle what is physically on your board before changing any build setting. (One rule is already certain: never connect a `TP4056` Li-ion charger to rechargeable AA NiMH cells.)

---

## 2. Your Next 10 Physical Steps (In Order)

Do not skip ahead to 3D printing the case or loading hundreds of games until Steps 1–7 pass on your desk!

1. **Read the 1-Page Safety Rules ([`docs/guides/safety.md`](safety.md)) & Install the Two `10 kΩ` SPI Pull-Up Resistors FIRST:**
   - Unplug USB-C and remove AA batteries before soldering or probing continuity.
   - Before starting Step 1, solder the two `10 kΩ` pull-up resistors (`GPIO 10` `TFT_CS` -> `3V3` and `GPIO 13` `SD_CS` -> `3V3`; [`docs/wiring/02-sd-card.md`](../wiring/02-sd-card.md)) so both SPI devices stay idle during reset.
2. **Do the Power-Off Continuity, Polarity & Pull-Up Checks ([`docs/guides/bringup.md` Step 1](bringup.md)):**
   - Put your multimeter in Continuity (beep) mode and make sure `3V3` and `GND` do **not** beep, `VCC`/`GND` on every module are **not** swapped, both `10 kΩ` pull-ups read `~10 kΩ`, and all 8 buttons beep only when held down.
   - **Disconnect** the SD module's `MISO` wire from ESP32 `GPIO 15` before turning power on for the first time.
3. **Check Your Standalone MicroSD Module `MISO` Voltage BEFORE Connecting `GPIO 15` ([`docs/guides/bringup.md` Step 2](bringup.md)):**
   - Power the SD module **on the exact rail it will use permanently** (`3V3` if it is a `3.3 V`-only board without an `AMS1117` chip; **`5V`** if it is the blue 6-pin module with a black 3-pin `AMS1117` regulator). Measure `MISO` -> `GND` while still disconnected from `GPIO 15` **both with no card and with a FAT32 card inserted**. Connect `MISO` to `GPIO 15` **only** if both read **`<= 3.35 V`**. (If yours is a blue 5 V module, consider ordering a `3.3 V`-only [Adafruit #4682](https://www.adafruit.com/product/4682), accessed `2026-11-15`.)
4. **Run the Read-Only Chip Check, Then Flash `bringup.ino` First ([`firmware/bringup/bringup.ino`](../../firmware/bringup/bringup.ino)):**
   - Run the read-only `esptool flash-id` / `chip-id` check first (`docs/guides/bringup.md` Step 3) so you know the exact flash and PSRAM wiring on your physical board before uploading any code.
   - Start with `bringup.ino` to verify buttons, SD, and LCD, and follow [`docs/decisions/0006-portable-power.md`](../decisions/0006-portable-power.md) and [`docs/bom.md`](../bom.md) (datasheets accessed `2026-11-15`) before adding a `2×AA` (`Adafruit #2030`/`TPS61022` + `MF-R300` + `DMP2035U`) or `3×AA` (`Pololu U3V70F5`/`U3V40F5` + `MF-R250`, taller shell) NiMH boost pack; old `S7V7F5` advice is withdrawn.
   - Before flashing the huge 3.3 MB emulator firmware, flash the small bring-up test sketch (instructions in Section 4 below). It checks your 8 MB memory (`PSRAM`), draws color bars on the `2.4"` screen at a safe `40 MHz` speed, checks the SD card, and lights up each of your 8 buttons on screen as you press them.
5. **Prepare Your MicroSD Card (Section 3 below):**
   - Format a MicroSD card (`<= 32 GB`) as **FAT32** and create the ROM folders.
6. **Flash the Full BMO Firmware (Section 4 below):**
   - Once the bring-up sketch shows color bars, `SD Card: OK`, and all 8 buttons working, flash `firmware/BmoGameboy`.
7. **Test `40 MHz` vs `80 MHz` Screen Speed on Your Board:**
   - If the screen looks glitchy or blank on first boot, hold **`SELECT` or `B`** while plugging in USB-C to force **Safe Mode (`40 MHz` screen speed)**. In the main menu, press **`START`** to open **Diagnostics** and test toggling between `40 MHz` and `80 MHz`.
8. **Measure Your Soldered Parts with Calipers (`cm`):**
   - Measure your soldered board, display module, switches, and AA battery holder in centimetres and record them in [`docs/hardware_measurements.md`](../hardware_measurements.md).
9. **Review the Decisions Waiting for Your Sign-Off ([`docs/owner-decisions.md`](../owner-decisions.md)):**
   - Choose your preferred AA battery setup (`2×AA` vs `3×AA`), enclosure size (`8.80 × 12.40 × 3.20 cm` vs compact), and public/private repo preferences.
10. **Print a 3D Fit Test Coupon Before Printing the Full Shell ([`docs/enclosure-3d-printing.md`](../enclosure-3d-printing.md)):**
    - Print a small test piece first to confirm the screen window and button caps fit smoothly before a 10-hour shell print.

---

## 3. How to Prepare Your MicroSD Card

1. Use a reputable **16 GB or 32 GB MicroSDHC card** formatted as **FAT32** (on Windows, right-click the drive -> *Format...* -> File system: `FAT32`). Cards `64 GB` and larger default to `exFAT`, which the ESP32-S3 will not read unless reformatted to `FAT32`.
2. Create these folders at the root of the MicroSD card (or run `python scripts/deploy_sd.py --destination E:\ --apply` — omit `--apply` for a dry-run preview — replacing `E:\` with your `FAT32` drive letter; see also the [Troubleshooting Guide](troubleshooting.md)):
   - `/gb` — Game Boy (`.gb`)
   - `/gbc` — Game Boy Color (`.gbc`)
   - `/nes` — Nintendo Entertainment System (`.nes`)
   - `/snes` — Super Nintendo (`.sfc`, `.smc`)
   - `/genesis` — Sega Genesis / Mega Drive (`.gen`, `.md`, `.smd`)
   - `/sms` — Sega Master System (`.sms`)
   - `/gg` — Sega Game Gear (`.gg`)
   - `/pce` — PC Engine / TurboGrafx-16 (`.pce`)
   - `/atari` — Atari 2600 (`.a26`)
   - `/coleco` — ColecoVision (`.col`, `.sg`)
   - `/wswan` — WonderSwan (`.ws`, `.wsc`)
   - `/ngp` — Neo Geo Pocket (`.ngp`, `.ngc`)
   - `/lynx` — Atari Lynx (`.lnx`)
   - `/pico8` — PICO-8 (`.p8`)
   - `/doom` — DOOM WAD files (`doom1.wad` or `freedoom1.wad`)
   - `/saves` — Created automatically by the handheld for battery saves (`.sav`) and save states (`.s01`–`.s05`).

---

## 4. How to Flash Your Board (Step-by-Step)

1. Plug your USB-C data cable into the port labeled **`UART` / `COM`** on your `ESP32-S3-DevKitC-1-N16R8` board.
2. Open PowerShell in the repository root (`BMO Gameboy`).
3. **First Run Read-Only `esptool v5.3.1` Check Before Flashing ([`docs/guides/bringup.md` Step 3](bringup.md)):**
   ```powershell
   $ESPTOOL = "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\5.3.1\esptool.exe"
   if (-not (Test-Path $ESPTOOL)) {
       python -m esptool --chip esp32s3 --port COM3 flash-id
       python -m esptool --chip esp32s3 --port COM3 chip-id
   } else {
       & $ESPTOOL --chip esp32s3 --port COM3 flash-id
       & $ESPTOOL --chip esp32s3 --port COM3 chip-id
   }
   ```
4. **Step A — Flash the First-Power-On Bring-Up Sketch first:**
   ```powershell
   .\arduino-cli.exe compile --profile esp32s3 firmware/bringup
   .\arduino-cli.exe upload --profile esp32s3 --port COM3 firmware/bringup
   python tasks.py monitor --port COM3
   ```
   *(Replace `COM3` with the COM port shown by `.\arduino-cli.exe board list`.)*
5. **Step B — After Bring-Up Passes, Flash the Full BMO Handheld Firmware:**
   ```powershell
   python tasks.py build
   python tasks.py flash --port COM3
   python tasks.py monitor --port COM3
   ```

---

## 5. Plain-English Troubleshooting Table

| Symptom | Most Likely Cause | How to Fix It |
| :--- | :--- | :--- |
| **No `COM` port appears on PC when plugged in** | Charge-only USB-C cable, or plugged into `USB/OTG` port instead of `UART/COM` | Use a data-capable USB-C cable in the `UART/COM` port; if using `USB/OTG`, hold `BOOT`, tap `RST`, release `BOOT` |
| **Screen backlight glows white, but no picture or garbled snow** | `80 MHz` screen clock is too fast for your perfboard wires, or `DC`/`CS`/`RST` wires are swapped | Hold **`SELECT` or `B`** while plugging in USB-C to force **40 MHz Safe Mode**; verify `TFT_CS=10`, `TFT_DC=8`, `TFT_RST=9` against [`docs/wiring/wire_list.csv`](../wiring/wire_list.csv) |
| **Screen is completely dark (no backlight)** | `BLK` or `VCC` pin on display is not receiving `3.3 V` | Check with multimeter that display `VCC` and `BLK` pins measure `3.30 V` relative to `GND` |
| **"SD READ FAILED" or `0` games shown** | SD card formatted as `exFAT`, `SD_CS` (`GPIO 13`) / `SD_MISO` (`GPIO 15`) loose, or missing `10 kΩ` pull-ups | Format SD card as `FAT32`; solder `10 kΩ` pull-up resistors from `GPIO 10` and `GPIO 13` to `3V3` ([`docs/wiring/02-sd-card.md`](../wiring/02-sd-card.md)) |
| **A button acts like it is held down continuously** | Button wired to `3V3` instead of `GND`, or wrong pair of legs on a 4-leg switch | Wire one leg of the switch to its `GPIO` pin and a diagonal leg to `GND` |
| **Board reboots endlessly (`rst:0x...` loop in serial monitor)** | Flash-mode mismatch (`qio` `OWNER-OBSERVED` conflict or accidental `opi` flash setting) or weak USB port (`ESP_RST_BROWNOUT`) | Run read-only `esptool flash-id` / `chip-id`, use a direct motherboard USB port, and follow the [`docs/guides/bringup.md`](bringup.md#boot-loop-fallback) fallback (diagnostic `FlashMode=dio` test without editing `sketch.yaml` permanently) |

---

## 6. Beginner Glossary (Plain-English Definitions)

- **ADC (Analog-to-Digital Converter):** A pin on the chip (`GPIO 1`) that measures battery voltage like a tiny built-in voltmeter.
- **Baud Rate (`115200`):** The speed (in bits per second) at which text messages travel over the USB cable to your computer's Serial Monitor.
- **Boost Converter:** A small circuit board that steps up `2.0–2.9 V` (`2×AA`) or `3.0–4.35 V` (`3×AA`) from rechargeable AA NiMH batteries to a steady `5.0 V` for the ESP32-S3 board ([ADR 0006](../decisions/0006-portable-power.md)).
- **Brownout (`ESP_RST_BROWNOUT`):** When the battery or USB voltage dips too low under heavy load, causing the chip to safely restart itself.
- **CPU Core 0 / CPU Core 1:** Your ESP32-S3 chip has two processor brains ("cores"). **CPU Core 1** runs the game emulator and reads buttons; **CPU Core 0** pushes pixels to the screen in the background.
- **DRAM (Internal Dynamic RAM, `320 KiB`):** The fast, small memory built directly inside the ESP32-S3 processor chip.
- **Emulator Core:** A software program (like `Peanut-GB` for Game Boy or `agnes` for NES) that imitates a classic video game console.
- **FAT32:** The standard file system format required on your MicroSD card so the ESP32-S3 can read files.
- **Flash (`16 MB`):** Permanent storage chip on your board. The `ESP32-S3-WROOM-1-N16R8` datasheet lists **Quad SPI (`QIO`)** flash, whereas your earlier `hardware-notes.md` recorded a **`QIO` boot loop (`OPI` worked)** — the read-only `esptool flash-id` check determines which flash mode your physical chip uses before changing build settings.
- **GPIO (General-Purpose Input/Output):** The numbered metal pins (`GPIO 4`, `GPIO 12`, etc.) on the ESP32-S3 board that connect to buttons, the screen, and the SD card.
- **I2C / I2S:** Standard 2-wire (`I2C`) and 3-wire (`I2S`, for audio) communication connections between chips.
- **LDO (Low-Dropout Regulator):** The tiny voltage regulator on the ESP32-S3 board that turns `5.0 V` USB/boost power into `3.3 V` for the chip.
- **NiMH (Nickel-Metal Hydride):** Safe, non-flammable rechargeable AA batteries (`1.2 V` nominal per cell, such as Panasonic Eneloop).
- **NVS (Non-Volatile Storage):** A small `20 KiB` section of Flash memory where the handheld remembers settings (like whether you chose `40 MHz` or `80 MHz` screen speed).
- **Perfboard (`0.254 cm` / `2.54 mm` pitch):** A fiberglass board (`7.87 × 11.68 cm`, `31 × 46` holes) with copper rings where you hand-solder components and wires.
- **Polyfuse (Resettable Fuse):** A safety part that automatically cuts battery power if a short circuit happens, then resets itself once cooled.
- **PSRAM (Pseudo-Static RAM, `8 MB` Octal SPI / `OPI`):** The large `8 MB` working memory chip on your N16R8 module where loaded game ROMs and screen canvases live while playing.
- **Pull-Up Resistor (`10 kΩ` or `INPUT_PULLUP`):** A resistor connected to `3.3 V` that keeps a signal wire cleanly `HIGH` (`1`) when a button isn't pressed or when the chip is restarting.
- **SPI (Serial Peripheral Interface):** The high-speed 4-wire data bus (`SCK` clock, `MOSI` data out, `MISO` data in, `CS` chip select) shared by your screen and MicroSD card.
