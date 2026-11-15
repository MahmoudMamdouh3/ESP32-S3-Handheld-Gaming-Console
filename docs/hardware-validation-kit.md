# Hardware Validation Kit (Owner Bench Checklist)

This checklist takes your newly soldered ESP32-S3 perfboard from `NOT_TESTED` / `TARGET_COMPILED` to verified **`HARDWARE_TESTED`** evidence recorded in [`docs/hardware_measurements.md`](hardware_measurements.md). Execute **Steps 1 through 7 strictly in order**. For plain-English detail on Steps 1–5, see [`docs/guides/bringup.md`](guides/bringup.md).

---

## Before You Begin — Tools Needed

- Hand-soldered ESP32-S3-DevKitC-1-N16R8 perfboard + 2.4" ST7789 display + MicroSD module (`FAT32` card `8–32 GB`)
- Digital multimeter (Continuity beep mode, Resistance `Ω`, DC Volts)
- Direct USB-C data cable to your PC (`COM3` or your board's serial port) + optional USB inline power meter
- Workstation environment bootstrapped via `python tasks.py setup --install-arduino-cli` ([`docs/build-and-ci.md`](build-and-ci.md))

---

## Step 1: Unpowered Continuity & Short-Circuit Checks (Disconnect SD `MISO` First)

**Keep USB-C and batteries unplugged.**

1. **Disconnect the MicroSD module's `MISO` wire from ESP32 `GPIO 15`** before applying power for the first time.
2. Probe **`3V3` to `GND`** in Continuity mode: **PASS** = no continuous beep (`> 100 Ω`); **FAIL** = continuous beep (`< 5 Ω`) -> fix solder bridge first.
3. Probe **`5V` (`VBUS`) to `GND`** in Continuity mode: **PASS** = no continuous beep.
4. Measure resistance from **`GPIO 10` (`TFT_CS`) -> `3V3`** and **`GPIO 13` (`SD_CS`) -> `3V3`**: **PASS** = `~10 kΩ` each ([`docs/wiring/02-sd-card.md`](wiring/02-sd-card.md)).
5. Probe all 8 wired buttons (`UP=4, DOWN=5, LEFT=6, RIGHT=7, A=16, B=17, START=18, SELECT=21`) to `GND`: **PASS** = open when unpressed, `~0 Ω` when held.

---

## Step 2: Power On With SD `MISO` Disconnected & Measure Standalone SD `MISO` (`<= 3.35 V`)

**Critical Order:** Do **not** reconnect the SD module `MISO` wire until this measurement passes.

1. Remove the MicroSD card, verify SD `MISO` is still **disconnected from ESP32 `GPIO 15`**, and plug USB-C into the `UART/COM` port.
2. Confirm no hot components and idle USB current `~40–180 mA @ 5.0 V` (unplug immediately if `> 400 mA` or hot).
3. With black probe on `GND`, record DC voltage in [`docs/hardware_measurements.md`](hardware_measurements.md):
   - **`3V3` rail:** **`3.20–3.40 V`**
   - **`5V` / `VBUS` rail:** **`4.75–5.25 V`**
   - **Standalone MicroSD module `MISO` output** (module powered from `3V3`, wire disconnected from `GPIO 15`; test both **without card** and **with card inserted**):
     - **PASS:** **`<= 3.35 V`** (`~3.30 V`) in both states.
     - **FAIL:** **`> 3.35 V`** (`~4.5–5.0 V`) -> **STOP.** Do NOT connect to `GPIO 15`. Replace with a `3.3 V`-native breakout ([Adafruit #4682](https://www.adafruit.com/product/4682), accessed `2026-11-15`).
4. **Only after standalone SD `MISO` passes `<= 3.35 V`:** Unplug USB, connect/solder SD `MISO` to `GPIO 15`, plug USB back in, and verify `GPIO 15`, `CS`, `SCK`, and `MOSI` all stay **`<= 3.35 V`**.

---

## Step 3: Read-Only `esptool v5.3.1` Chip & Flash Check (Before First Flash)

Before flashing any firmware, query the physical chip and SPI flash in read-only mode ([Espressif `esptool` Basic Commands](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/basic-commands.html#read-spi-flash-id-flash-id), accessed `2026-11-15`):

```powershell
$ESPTOOL = "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\5.3.1\esptool.exe"
& $ESPTOOL --chip esp32s3 --port COM3 flash-id
& $ESPTOOL --chip esp32s3 --port COM3 chip-id
& $ESPTOOL --chip esp32s3 --port COM3 read-mac
```

Record the manufacturer ID, flash size, and chip revision in [`docs/hardware_measurements.md`](hardware_measurements.md). *(See [Boot-Loop Fallback](guides/bringup.md#boot-loop-fallback) if your board encounters the `OWNER-OBSERVED` `FlashMode=qio` boot-loop conflict — never edit `sketch.yaml` without an approved ADR; test `FlashMode=dio` cleanly with `--fqbn`.)*

---

## Step 4: Flash Minimal `firmware/bringup/bringup.ino` FIRST & Verify Buttons

Flash the isolated diagnostic sketch **before** the full firmware (`390,880 B` Flash, `24,600 B` RAM, `73,472 B` IRAM):

```powershell
.\arduino-cli.exe compile --profile esp32s3 firmware/bringup
.\arduino-cli.exe upload --profile esp32s3 --port COM3 firmware/bringup
.\arduino-cli.exe monitor -p COM3 -c baudrate=115200
```

Record pass/fail for each bring-up check:
- `[PSRAM]`: `8 MB` Octal PSRAM detected + `1MiB_memtest=PASS`
- `[TFT]`: Crisp color bars on the `2.4"` ST7789 at `40 MHz` SPI
- `[SD]`: FAT32 MicroSD mounts at `25 MHz` (`[SD] PASS: Mounted at 25 MHz, cardSize=... MB`)
- `[BTN]`: All `8` wired buttons (`UP=4, DOWN=5, LEFT=6, RIGHT=7, A=16, B=17, START=18, SELECT=21`) register individually while unsoldered `X=14, Y=42, L=47, R=48` remain inactive (`(all released)` when nothing pressed)

---

## Step 5: Flash Full `BmoGameboy` Firmware & Compare `40 MHz` vs `80 MHz` SPI

Only after Steps 1–4 pass:

```powershell
python tasks.py build --profile default --warnings strict --fail-on-warnings
.\arduino-cli.exe upload --profile esp32s3 --port COM3 firmware/BmoGameboy
.\arduino-cli.exe monitor -p COM3 -c baudrate=115200
```

1. **Default `80 MHz` Boot:** Observe display stability, menu transitions, and SD loads at default `TFT_SPI_HZ = 80000000UL` ([ADR 0014](decisions/0014-display-spi-clock.md)). Note `ST7789VW` datasheet lists `62.5 MHz` write max, so physical signal integrity must be observed.
2. **Safe-Mode `40 MHz` Comparison:** Hold **`SELECT`** or **`B`** during boot until Serial Monitor prints `[SAFE_MODE] SELECT/B held at boot; forced display SPI clock to 40 MHz`. Compare frame pacing and display clarity (`40 MHz` vs `80 MHz`).

---

## Step 6: MicroSD Catalog, Core Launch, `.sav`, and `.s01` Save-State Check

1. Load test homebrew / open-source `.gb` and `.nes` ROMs onto the FAT32 MicroSD card (`<root>/gb/`, `<root>/nes/`).
2. Confirm BMO indexes the catalog (`SD_CS=13`) with zero `CRC` errors or failed mounts.
3. Launch a core, play briefly, trigger in-game SRAM save (`.sav`) and pause-menu save state (`.s01`), power-cycle, and confirm both reload identically without corruption.

---

## Step 7: 600-Frame Serial Telemetry Capture & Measurement Sign-Off

1. Capture serial output during gameplay (`FPS:` and `[PROFILE_CSV]` / `Frame window: emu=... n=600 ...` lines) to a local log file (e.g., `serial_capture.log` outside tracked git files).
2. Generate a real hardware report with [`tools/telemetry_plotter.py`](../tools/telemetry_plotter.py):
   ```powershell
   python tools/telemetry_plotter.py --input serial_capture.log --output bench_telemetry.txt
   ```
   *(Verify the output header reads `Data source: CAPTURED`, not `SYNTHETIC`.)*
3. Log all voltages, `esptool flash-id` output, `40 MHz` vs `80 MHz` results, and `p50`/`p95`/`p99` timings in [`docs/hardware_measurements.md`](hardware_measurements.md).
