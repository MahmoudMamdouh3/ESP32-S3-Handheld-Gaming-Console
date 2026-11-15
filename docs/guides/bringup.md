# First-Time Hardware Bring-Up Guide (Step-by-Step)

This guide is written for your very first power-on of the hand-soldered ESP32-S3 perfboard. Do **not** skip ahead, do **not** solder AA batteries yet, and follow Steps 1 through 6 **strictly in order** so a wiring mistake or a misbehaving MicroSD module cannot damage your ESP32-S3.

Read [`docs/guides/safety.md`](safety.md) first if you have any doubt about electrical safety.

---

## Step 1: Unpowered Continuity Checks (Disconnect SD `MISO` First)

Keep USB-C and all batteries completely **unplugged**:

1. Set your multimeter to **Continuity** mode (the speaker/beep symbol) and touch both probes together to verify the beep.
2. **Disconnect the MicroSD module's `MISO` wire from ESP32 `GPIO 15`** before applying power for the first time. Leaving `MISO` disconnected guarantees an unknown SD breakout cannot push `5.0 V` onto `GPIO 15`.
3. Touch black probe to ESP32 `GND` and red probe to `3V3`:
   - **PASS:** Meter shows resistance (`> 100 Ω`) and **does NOT beep continuously** (a brief sub-second chirp as capacitors charge is normal).
   - **FAIL:** Continuous beep or `< 5 Ω`. Do **NOT** plug in USB — inspect the underside of the perfboard for solder bridges between `3V3` and `GND`.
4. Touch black probe to `GND` and red probe to `5V` (`VBUS`):
   - **PASS:** No continuous beep.
   - **FAIL:** Continuous beep -> fix solder short before powering on.
5. Switch multimeter to **Resistance (`Ω`)** mode and measure from `GPIO 10` (`TFT_CS`) to `3V3` and from `GPIO 13` (`SD_CS`) to `3V3`:
   - **PASS:** Both read approximately **`10 kΩ`** (confirming both SPI chip-select pull-up resistors in [`docs/wiring/02-sd-card.md`](../wiring/02-sd-card.md) are installed).
6. Check button continuity (`UP=4`, `DOWN=5`, `LEFT=6`, `RIGHT=7`, `A=16`, `B=17`, `START=18`, `SELECT=21`):
   - **PASS:** High resistance (`> 1 kΩ` / open) to `GND` while unpressed; near `0 Ω` + beep while held down.

---

## Step 2: Power-On With SD `MISO` Disconnected & Standalone SD `MISO` Voltage Check (`<= 3.35 V`)

**Critical Rule:** The MicroSD `MISO` wire must STILL be **disconnected from ESP32 `GPIO 15`** when you plug USB-C in for the first time.

1. Make sure **no MicroSD card** is inserted and plug a USB-C data cable into the ESP32-S3 **`UART/COM`** port (preferably through a USB current meter or hub).
2. **5-Second Heat & Current Check:**
   - **PASS:** Neither the ESP32-S3 module nor its voltage regulator gets hot (cool or barely warm), and USB idle draw is around **`40–180 mA @ 5.0 V`**.
   - **FAIL:** Chip/regulator heats rapidly or USB draw spikes `> 400 mA` -> **UNPLUG IMMEDIATELY** and re-check Step 1.
3. Set your multimeter to **DC Volts (`20 V` range)** with black probe on `GND` and verify rails:
   - ESP32 **`3V3` pin**: **`3.20 V` to `3.40 V`**
   - ESP32 **`5V` / `VBUS` pin**: **`4.75 V` to `5.25 V`**
   - **MicroSD module standalone `MISO` output** (wire still disconnected from ESP32 `GPIO 15`, module powered from `3V3` — measure both with **no card** and with a **FAT32 card inserted**):
     - **PASS:** **`<= 3.35 V`** (`3.30 V` nominal) at idle and while card is inserted.
     - **FAIL:** **Above `3.35 V`** (especially `~4.5–5.0 V`) -> **STOP AND UNPLUG.** That breakout is pulling `MISO` toward `5 V` or uses an incompatible level shifter/regulator that can destroy ESP32 `GPIO 15`. Replace it with a true `3.3 V`-native breakout ([Adafruit #4682](https://www.adafruit.com/product/4682), datasheet accessed `2026-11-15`) or verify [`docs/wiring/02-sd-card.md`](../wiring/02-sd-card.md).
4. **Only after standalone SD `MISO` passes `<= 3.35 V`:** Unplug USB-C, solder/connect SD `MISO` to `GPIO 15`, plug USB-C back in, and verify `GPIO 15` still measures `<= 3.35 V` with black probe on `GND` (`CS`/`SCK`/`MOSI` also `<= 3.35 V`). Record the readings in [`docs/hardware_measurements.md`](../hardware_measurements.md).

---

## Step 3: Read-Only `esptool v5.3.1` Chip & Flash Verification (Before Flashing Anything)

Before compiling or flashing any sketch, interrogate your physical chip in read-only mode so you know the real flash manufacturer/size and chip configuration ([Espressif `esptool` Basic Commands](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/basic-commands.html#read-spi-flash-id-flash-id), accessed `2026-11-15`). Note that `esptool v5.3.1` uses **hyphenated** subcommands (`flash-id`, `chip-id`, `read-mac`):

```powershell
$ESPTOOL = "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\5.3.1\esptool.exe"
& $ESPTOOL --chip esp32s3 --port COM3 flash-id
& $ESPTOOL --chip esp32s3 --port COM3 chip-id
& $ESPTOOL --chip esp32s3 --port COM3 read-mac
```

*(If `esptool` reports unable to connect, hold `BOOT`, tap `RST`, release `RST`, then release `BOOT` and re-run.)* Save the output in [`docs/hardware_measurements.md`](../hardware_measurements.md).

---

## Boot-Loop Fallback

> [!IMPORTANT]
> **`OWNER-OBSERVED` `FlashMode=qio` Conflict:** According to Espressif's *ESP32-S3-WROOM-1 Datasheet* (`v1.4` Table 1, [`espressif.com`](https://www.espressif.com/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf), accessed `2026-11-15`), `ESP32-S3-WROOM-1-N16R8` integrates `16 MB` **Quad SPI** flash (`FlashMode=qio`) and `8 MB` **Octal SPI** PSRAM (`PSRAM=opi`). **However**, the owner observed this specific physical board reboot-looping under `FlashMode=qio` in an earlier session.
> Do **not** blindly modify [`firmware/BmoGameboy/sketch.yaml`](../../firmware/BmoGameboy/sketch.yaml) (`qio/opi` is a locked Hard Stop pending an ADR).

If flashing causes continuous `rst:0x...` reboots in Serial Monitor (`115200` baud):

1. Check power first: plug straight into a direct PC motherboard USB port (weak hubs trigger `ESP_RST_BROWNOUT`) and verify Step 3 `flash-id` output.
2. Run a **temporary non-persistent diagnostic compile** of `bringup.ino` in Dual I/O flash mode (`FlashMode=dio` — one of the four modes supported by `.\arduino-cli.exe board details -b esp32:esp32:esp32s3`: `qio`, `qio120`, `dio`, `opi`):

```powershell
.\arduino-cli.exe compile --fqbn esp32:esp32:esp32s3:FlashMode=dio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi firmware/bringup
.\arduino-cli.exe upload --fqbn esp32:esp32:esp32s3:FlashMode=dio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi --port COM3 firmware/bringup
```

3. If `dio` boots cleanly while `qio` loops, capture both the Step 3 `flash-id` output and the serial boot log, open an ADR in [`docs/decisions/`](../decisions/README.md), and approve the hard-stop change before touching `sketch.yaml`.

---

## Step 4: Flash Minimal Bring-Up Sketch FIRST (`firmware/bringup/bringup.ino`)

Never jump straight to the full emulator firmware on untested hardware. Flash [`firmware/bringup/bringup.ino`](../../firmware/bringup/bringup.ino) (`TARGET_COMPILED`: `390,880 B` Flash, `24,600 B` DRAM, `73,472 B` IRAM) first:

```powershell
.\arduino-cli.exe compile --profile esp32s3 firmware/bringup
.\arduino-cli.exe upload --profile esp32s3 --port COM3 firmware/bringup
.\arduino-cli.exe monitor -p COM3 -c baudrate=115200
```

Verify all 4 checks on the screen and Serial Monitor:
- `[MEM]` / `[PSRAM]`: `8 MB` Octal PSRAM detected and `1MiB_memtest=PASS`
- `[TFT]`: Crisp Red / Green / Blue / White / Black test bars (`320×240` at `40 MHz` SPI) with no snow or washed-out lines
- `[SD]`: FAT32 MicroSD mounts cleanly at `25 MHz` (`cardSize=... MB`)
- `[BTN]`: Every wired button (`UP=4`, `DOWN=5`, `LEFT=6`, `RIGHT=7`, `A=16`, `B=17`, `START=18`, `SELECT=21`) prints when pressed and shows `(all released)` otherwise (no stuck pins)

---

## Step 5: Flash Full `BmoGameboy` Firmware & Compare `40 MHz` vs `80 MHz` SPI

Only after Steps 1–4 all pass:

1. Compile and flash the full handheld firmware (`default` profile for personal use, or `mit` for strictly permissive cores):
   ```powershell
   python tasks.py build --profile default --warnings strict --fail-on-warnings
   .\arduino-cli.exe upload --profile esp32s3 --port COM3 firmware/BmoGameboy
   .\arduino-cli.exe monitor -p COM3 -c baudrate=115200
   ```
2. Observe BMO's boot screen at default **`80 MHz`** display SPI ([ADR 0014](../decisions/0014-display-spi-clock.md)).
3. Hold **`SELECT`** or **`B`** during boot to trigger Safe Mode **`40 MHz`** (`[SAFE_MODE] SELECT/B held at boot; forced display SPI clock to 40 MHz`) and compare visual crispness, snow/glitches, and SD card reliability. Record `40 MHz` vs `80 MHz` findings in [`docs/hardware_measurements.md`](../hardware_measurements.md).

---

## Step 6: Before Adding AA Batteries

Do **not** solder AA cells until USB bring-up passes AND you select `2×AA` (`Adafruit #2030` or `TI TPS61022` + `MF-R300`) or `3×AA` (`Pololu U3V70F5` or `Pololu U3V40F5` + `MF-R250`, requiring a taller shell) in [`docs/owner-decisions.md`](../owner-decisions.md) Decision 1 and [`docs/decisions/0006-portable-power.md`](../decisions/0006-portable-power.md).
