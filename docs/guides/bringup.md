# First-Time Hardware Bring-Up & Bench Validation Guide (Unified Step-by-Step)

This guide covers every bench check for the hand-soldered ESP32-S3 perfboard, from unpowered continuity probes through full-firmware telemetry capture ([`docs/hardware_measurements.md`](../hardware_measurements.md)). Do **not** skip ahead, do **not** solder AA batteries yet, and execute **Steps 1 through 7 strictly in order**.

Read [`docs/guides/safety.md`](safety.md) first if you have any doubt about electrical safety.

---

## Step 1: Unpowered Continuity, Pull-Up & Polarity Checks (USB & Batteries Unplugged)

Make sure USB-C and all batteries are completely **unplugged**, and confirm both `10 kΩ` SPI chip-select pull-up resistors (`GPIO 10` -> `3V3` and `GPIO 13` -> `3V3`) are soldered before starting:

1. Set your multimeter to **Continuity** mode (the speaker/beep symbol) and touch both probes together to verify it beeps cleanly.
2. **Disconnect the MicroSD module's `MISO` wire from ESP32 `GPIO 15`** before applying power for the first time. Leaving `MISO` disconnected prevents an unsafe SD module from driving `> 3.35 V` onto `GPIO 15`.
3. **Module power-pin polarity continuity check (`VCC` vs `GND`):**
   - Put the black probe on ESP32 **`GND`** and probe **`GND`** on both the `2.4"` ST7789 display module and the MicroSD module -> **PASS:** `~0 Ω` + beep; **FAIL:** open circuit.
   - Now move the red probe to ESP32 **`GND`** and probe **`VCC`** on both the display module and the MicroSD module -> **PASS:** **NO beep** (`> 100 Ω` / open); **FAIL:** beep (`0 Ω`) means `VCC` and `GND` are swapped or shorted — fix before ever applying power!
   - Confirm ST7789 **`VCC`** connects only to **`3V3`**, and MicroSD **`VCC`** connects only to its intended rail (`3V3` for a `3.3 V`-only Type A board, or `5V`/`VBUS` for a `5 V` Type B `AMS1117` board — **zero** short between `3V3` and `5V`).
4. **Rail-to-ground short checks (`GND` black probe):**
   - Probe **`3V3`** (red): **PASS** = resistance **`> 100 Ω`** and **no continuous beep** (**FAIL** = continuous beep or `< 5 Ω`).
   - Probe **`5V` / `VBUS`** (red): **PASS** = **no continuous beep**.
5. **SPI CS pull-up resistance (`Ω` mode):**
   - Measure **`GPIO 10` (`TFT_CS`) -> `3V3`** and **`GPIO 13` (`SD_CS`) -> `3V3`**: **PASS** = approximately **`10 kΩ`** on both ([`docs/wiring/02-sd-card.md`](../wiring/02-sd-card.md)).
6. **Button continuity to `GND` (`UP=4`, `DOWN=5`, `LEFT=6`, `RIGHT=7`, `A=16`, `B=17`, `START=18`, `SELECT=21`):**
   - **PASS:** High resistance (`> 1 kΩ` / open) while unpressed; near **`0 Ω` + beep** while held down.

---

## Step 2: Power-On With SD `MISO` Disconnected & Standalone `MISO` Test (`<= 3.35 V`)

**Critical Rule (Prevent False-Pass):** You **must** power the MicroSD module **from the exact supply rail it will use permanently**, while its `MISO` wire remains **disconnected from ESP32 `GPIO 15`**:
- **Type A (`3.3 V`-only module, no `AMS1117` chip, e.g., [Adafruit #4682](https://www.adafruit.com/product/4682), accessed `2026-11-15`):** Wire module `VCC` to **`3V3`**. *(Never connect `5 V`.)*
- **Type B (blue 6-pin module with black 3-pin `AMS1117-3.3` regulator + `74LVC125A`):** Wire module `VCC` to **`5V` (`VBUS`)**. *(Never test a Type B module on `3V3` — its LDO drops output to `~2.2 V` and gives a dangerous false-pass that can rise toward `5 V` once wired permanently to `5V`! If you own a blue Type B module, consider ordering a `3.3 V`-only `Adafruit #4682` now.)*

1. Make sure **no MicroSD card** is inserted and SD `MISO` is **disconnected from `GPIO 15`**, then plug USB-C into the ESP32-S3 **`UART/COM`** port (preferably through a USB current meter).
2. **5-Second Heat & Current Check:**
   - **PASS:** Neither the ESP32-S3 module nor its LDO regulator gets hot (cool or barely warm), and idle current is around **`40–180 mA @ 5.0 V`**.
   - **FAIL:** Chip or regulator heats rapidly, or current exceeds **`> 400 mA`** -> **UNPLUG IMMEDIATELY** and return to Step 1.
3. Set multimeter to **DC Volts (`20 V` range)** with black probe on `GND` and verify:
   - ESP32 **`3V3` pin:** **`3.20 V` to `3.40 V`**
   - ESP32 **`5V` / `VBUS` pin:** **`4.75 V` to `5.25 V`**
   - **Standalone MicroSD module `MISO` output** (module powered on its permanent rail `3V3` or `5V`, wire **still disconnected from ESP32 `GPIO 15`**):
     - Measure **with no card inserted**, then **with a FAT32 card inserted**:
     - **PASS:** **`<= 3.35 V`** (`3.30 V` nominal) in **both** states.
     - **FAIL (`> 3.35 V`):** **STOP AND UNPLUG IMMEDIATELY.** Do not connect to `GPIO 15` — replace the breakout with a `3.3 V`-native board ([Adafruit #4682](https://www.adafruit.com/product/4682), accessed `2026-11-15`) or the display PCB's `3.3 V` SD slot ([`docs/wiring/02-sd-card.md`](../wiring/02-sd-card.md)).
4. **Only after standalone SD `MISO <= 3.35 V` passes in both states:** Unplug USB-C, solder/connect SD `MISO` to `GPIO 15`, plug USB-C back in, and verify `GPIO 15`, `TFT_CS` (`10`), `SD_CS` (`13`), `SCK` (`12`), and `MOSI` (`11`) all measure **`<= 3.35 V`** relative to `GND`. Record every reading in [`docs/hardware_measurements.md`](../hardware_measurements.md).

---

## Step 3: Read-Only `esptool v5.3.1` Chip & Flash Verification (Before Flashing Anything)

Before compiling or uploading any sketch, query your physical chip in read-only mode so you know what flash/PSRAM silicon is actually on your board ([Espressif `esptool` Basic Commands](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/basic-commands.html#read-spi-flash-id-flash-id), accessed `2026-11-15`). Use hyphenated `v5.3.1` subcommands (`flash-id`, `chip-id`, `read-mac`):

```powershell
$ESPTOOL = "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\5.3.1\esptool.exe"
if (-not (Test-Path $ESPTOOL)) {
    python -m esptool --chip esp32s3 --port COM3 flash-id
    python -m esptool --chip esp32s3 --port COM3 chip-id
    python -m esptool --chip esp32s3 --port COM3 read-mac
} else {
    & $ESPTOOL --chip esp32s3 --port COM3 flash-id
    & $ESPTOOL --chip esp32s3 --port COM3 chip-id
    & $ESPTOOL --chip esp32s3 --port COM3 read-mac
}
```

*(If `esptool` cannot connect, hold `BOOT`, tap `RST`, release `RST`, then release `BOOT` and re-run; replace `COM3` with your active port.)* Save the full output in [`docs/hardware_measurements.md`](../hardware_measurements.md).

---

## Boot-Loop Fallback

> [!IMPORTANT]
> **`OWNER-OBSERVED` `FlashMode=qio` Conflict:** Espressif's *ESP32-S3-WROOM-1 Datasheet* (`v1.4` Table 1, [`espressif.com`](https://www.espressif.com/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf), accessed `2026-11-15`) specifies `16 MB` **Quad SPI** flash (`FlashMode=qio`) + `8 MB` **Octal SPI** PSRAM (`PSRAM=opi`). **However**, the owner's own notes recorded this physical board **boot-looping under `qio` while `opi` worked**. Both datasheet and owner observation conflict until Step 3 `esptool flash-id` / `chip-id` confirms the real flash chip.
> Do **not** edit [`firmware/BmoGameboy/sketch.yaml`](../../firmware/BmoGameboy/sketch.yaml) (`qio/opi` stays a locked Hard Stop unless you approve an ADR).

If flashing hits continuous `rst:0x...` reboots in Serial Monitor (`115200` baud):

1. Check USB power first: plug straight into a PC motherboard USB port (weak hubs cause `ESP_RST_BROWNOUT`) and verify Step 3 `flash-id`.
2. Run a **temporary non-persistent diagnostic compile** of `bringup.ino` using `--fqbn` (`.\arduino-cli.exe board details -b esp32:esp32:esp32s3` lists `qio`, `qio120`, `dio`, `opi`) without changing `sketch.yaml`:

```powershell
# Option A — Test Dual I/O flash (dio):
.\arduino-cli.exe compile --fqbn esp32:esp32:esp32s3:FlashMode=dio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi firmware/bringup
.\arduino-cli.exe upload --fqbn esp32:esp32:esp32s3:FlashMode=dio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi --port COM3 firmware/bringup

# Option B — Test Octal SPI flash (opi, since owner notes reported opi previously booted):
.\arduino-cli.exe compile --fqbn esp32:esp32:esp32s3:FlashMode=opi,FlashSize=16M,PartitionScheme=custom,PSRAM=opi firmware/bringup  # diagnostic only; never edit sketch.yaml without ADR
.\arduino-cli.exe upload --fqbn esp32:esp32:esp32s3:FlashMode=opi,FlashSize=16M,PartitionScheme=custom,PSRAM=opi --port COM3 firmware/bringup  # diagnostic only; never edit sketch.yaml without ADR
```

3. Record whichever mode boots cleanly alongside Step 3 `flash-id` output, open an ADR in [`docs/decisions/`](../decisions/README.md), and approve the hard-stop change before editing `sketch.yaml`.

---

## Step 4: Flash Minimal Bring-Up Sketch FIRST (`firmware/bringup/bringup.ino`)

Never flash the full emulator firmware onto untested hardware. Flash [`firmware/bringup/bringup.ino`](../../firmware/bringup/bringup.ino) (`TARGET_COMPILED`: `390,880 B` Flash, `24,600 B` DRAM, `73,472 B` IRAM) first:

```powershell
.\arduino-cli.exe compile --profile esp32s3 firmware/bringup
.\arduino-cli.exe upload --profile esp32s3 --port COM3 firmware/bringup
.\arduino-cli.exe monitor -p COM3 -c baudrate=115200
```

Verify all 4 checks on the screen and Serial Monitor:
- **`[MEM]` / `[PSRAM]`:** `8 MB` Octal PSRAM detected and `1MiB_memtest=PASS`
- **`[TFT]`:** Crisp Red / Green / Blue / White / Black test bars (`320×240` at `40 MHz` SPI) with no snow or washed-out lines
- **`[SD]`:** FAT32 MicroSD mounts at `25 MHz` (`[SD] PASS: Mounted at 25 MHz, cardSize=... MB`)
- **`[BTN]`:** Every wired button (`UP=4`, `DOWN=5`, `LEFT=6`, `RIGHT=7`, `A=16`, `B=17`, `START=18`, `SELECT=21`) prints when pressed and shows `(all released)` otherwise; unsoldered `X=14`, `Y=42`, `L=47`, `R=48` stay unpressed

---

## Step 5: Flash Full `BmoGameboy` Firmware & Compare `40 MHz` vs `80 MHz` SPI

Only after Steps 1–4 all pass:

```powershell
python tasks.py build --profile default --warnings strict --fail-on-warnings
.\arduino-cli.exe upload --profile esp32s3 --port COM3 firmware/BmoGameboy
.\arduino-cli.exe monitor -p COM3 -c baudrate=115200
```

1. **Default `80 MHz` Boot:** Observe BMO's boot screen, menu transitions, and SD loads at default `TFT_SPI_HZ = 80000000UL` ([ADR 0014](../decisions/0014-display-spi-clock.md)). Because the [Sitronix ST7789 Datasheet](https://www.buydisplay.com/download/ic/ST7789.pdf) (accessed `2026-11-16`) specifies `62.5 MHz` (`16 ns`) max serial write clock, watch carefully for pixel shimmer or corrupted frames.
2. **Safe Mode `40 MHz` Comparison:** Hold **`SELECT`** or **`B`** during boot until Serial Monitor prints `[SAFE_MODE] SELECT/B held at boot; forced display SPI clock to 40 MHz`. Compare frame pacing and display clarity (`40 MHz` vs `80 MHz`) and record in [`docs/hardware_measurements.md`](../hardware_measurements.md).

---

## Step 6: MicroSD Catalog, Gameplay, `.sav`, and `.s01` Save-State Check

1. Load test homebrew / open-source `.gb` and `.nes` ROMs onto the FAT32 MicroSD card (`<root>/gb/`, `<root>/nes/`).
2. Confirm BMO indexes the catalog (`SD_CS=13`) with zero `CRC` errors or failed mounts.
3. Launch a game, trigger an in-game SRAM save (`.sav`) and a pause-menu save state (`.s01`), power-cycle, and confirm both reload identically without corruption.

---

## Step 7: 600-Frame Serial Telemetry Capture & Bench Sign-Off

1. Capture serial output during gameplay (`FPS:` and `[PROFILE_CSV]` / `Frame window: emu=... n=600 ...` lines) to a local log file (outside tracked git files, e.g., `serial_capture.log`).
2. Render the real hardware telemetry report with [`tools/telemetry_plotter.py`](../../tools/telemetry_plotter.py):
   ```powershell
   python tools/telemetry_plotter.py --input serial_capture.log --output bench_telemetry.txt
   ```
   *(Verify the output header reads `Data source: CAPTURED`, never `SYNTHETIC`.)*
3. Log all voltages, `esptool flash-id` output, `40 MHz` vs `80 MHz` results, and `p50`/`p95`/`p99` timings in [`docs/hardware_measurements.md`](../hardware_measurements.md).

---

## Step 8: Before Adding AA Batteries

Do **not** solder AA cells until USB bring-up (Steps 1–7) passes AND you select `2×AA` (`Adafruit #2030` or `TI TPS61022` + `MF-R300` + `DMP2035U`) or `3×AA` (`Pololu U3V70F5` or `Pololu U3V40F5` + `MF-R250`, requiring a taller shell) in [`docs/owner-decisions.md`](../owner-decisions.md) Decision 1 and [`docs/decisions/0006-portable-power.md`](../decisions/0006-portable-power.md). Note that **no low-battery hardware cutoff exists yet** and `FEATURE_BATTERY_MONITOR=0`, and you must always keep the battery switch **OFF** and remove one AA cell whenever USB-C is connected.
