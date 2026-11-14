# Hardware Validation Kit (Owner Bench Checklist)

> **Evidence boundary:** As of this release, all firmware builds are `TARGET_COMPILED` and core/storage logic is `HOST_TESTED`. Physical board behavior (`HARDWARE_TESTED`) has **not** yet been recorded on the soldered perfboard. Run these steps in order before declaring the handheld hardware-verified.

---

## Before You Begin — Tools Needed

- **Digital multimeter** (DC Volts mode + continuity/resistance mode)
- **USB-C data cable** plugged into a powered USB hub or current-limited bench supply (avoid plugging straight into a motherboard port on the very first boot)
- **Blank or FAT32-formatted MicroSD card** (with at least 1 test `.gb`, `.gbc`, or `.nes` ROM in `/gb/`, `/gbc/`, or `/nes/`)
- **Terminal on this PC** at the repository root (`e:\BMO Gameboy`)

---

## Step 1: Unpowered Continuity & Short-Circuit Check

Do **not** plug in USB power yet. Set your multimeter to **continuity / resistance ($\Omega$)**:

1. Touch **black probe to `GND`**, **red probe to `3V3`**:
   - **Expected result:** Resistance rises above `100 Ω` (no continuous beep / dead short).
   - **If it beeps continuously (`< 5 Ω`):** **STOP.** Inspect solder bridges under the ESP32-S3-DevKitC, ST7789 display header, and MicroSD breakout before applying power.
2. Touch **black probe to `GND`**, **red probe to `5V` / `VBUS`**:
   - **Expected result:** No dead short (`> 100 Ω`).
3. Touch **black probe to `GND`**, **red probe to `GPIO 15` (`SD_MISO`)**:
   - **Expected result:** No dead short to ground or to the `5V` rail.

---

## Step 2: Build & Flash Firmware + Open Serial Monitor (`115200` baud)

Plug the ESP32-S3 USB-C port into your computer:

1. Compile and flash the firmware:
   ```powershell
   python tasks.py build --profile default --warnings strict --fail-on-warnings
   python tasks.py flash
   ```
2. Open the serial monitor (`115200` baud):
   ```powershell
   python tasks.py monitor
   ```
3. Press the **RESET (`RST`)** button on the ESP32-S3 once.
   - **Expected serial output:** Look for the boot banner lines:
     ```text
     --- BOOTING ---
     BMO Handheld v0.9.0-dev (nogit)
     ```
   - **Expected display output:** TFT backlight turns on cleanly and shows the BMO boot animation, transitioning into the console carousel (`FAVORITES`, `GAME BOY`, `GAME BOY COLOR`, `NES`, ...).

---

## Step 3: Critical `GPIO 15` (`SD_MISO`) Voltage Safety Check

Verify that the MicroSD breakout does **not** drive `5 V` into ESP32-S3 `GPIO 15` (which is a `3.3 V`-only pin):

1. Leave the MicroSD card **inserted** and USB-C powered on.
2. Set the multimeter to **DC Volts (20 V range)**.
3. Touch **black probe to `GND`** and **red probe to the `GPIO 15` (`SD_MISO`)** pin on the ESP32-S3 header, both while idle on the console carousel and right after opening Diagnostics / triggering a catalog scan.
   - **PASS condition:** Reading stays at or below **`3.35 V`** at all times.
   - **STOP condition (`> 3.60 V`):** **Unplug USB immediately.** Your MicroSD breakout's level shifter or pull-up is back-feeding 5 V onto `GPIO 15`. Power the MicroSD module strictly from `3.3 V` (bypassing an onboard 5 V LDO if required) or insert a 3.3 V buffer before continuing.

---

## Step 4: Safe-Mode Boot & SPI Bus Clock Diagnostics (`40 MHz` vs `80 MHz`)

Perfboard jumper wires can distort `80 MHz` SPI signals (`TFT_SCK = GPIO 12`, `TFT_MOSI = GPIO 11`, `SD_MISO = GPIO 15`). Test both speeds:

1. **Test Boot Safe Mode (`40 MHz` forced):**
   - Hold **`SELECT` (`GPIO 21`)** or **`B` (`GPIO 17`)** while pressing **`RST`**.
   - **Expected serial line:** Contains `[SAFE_MODE]` forcing `40 MHz` SPI fallback.
   - **Expected display:** Clean crisp text and icons with zero pixel snow or garbled colors.
2. **Test Runtime `80 MHz` vs `40 MHz` Toggle:**
   - Open **Diagnostics** on-screen (or navigate using `D-Pad` + `A`).
   - Toggle the SPI clock setting between **`40 MHz`** and **`80 MHz`** and trigger a MicroSD scan.
   - **Expected result:** Notice whether `80 MHz` introduces display glitches or SD read retries; if so, keep `40 MHz` saved in NVS preferences (`bmo_disp`).

---

## Step 5: Button Matrix Verification (`8` Wired vs `4` Unsoldered)

In the on-screen **Diagnostics** view (or serial log), verify all button states:

1. With **no buttons pressed**:
   - **Expected result:** All 8 wired inputs (`UP=4`, `DOWN=5`, `LEFT=6`, `RIGHT=7`, `A=16`, `B=17`, `START=18`, `SELECT=21`) show **`0` / released**, and all 4 unsoldered optional pins (`X=14`, `Y=42`, `L=47`, `R=48`) show **`0` / inactive** with zero ghost presses.
2. Press and hold each of the **8 wired buttons** one by one:
   - `UP` (`GPIO 4`), `DOWN` (`GPIO 5`), `LEFT` (`GPIO 6`), `RIGHT` (`GPIO 7`), `A` (`GPIO 16`), `B` (`GPIO 17`), `START` (`GPIO 18`), `SELECT` (`GPIO 21`).
   - **Expected result:** Each button transitions cleanly from `0 -> 1` while held and returns to `0` immediately on release.

---

## Step 6: MicroSD Catalog, Gameplay, `.sav`, and `.s01` Save-State Check

1. Prepare a FAT32 MicroSD card using:
   ```powershell
   python scripts/deploy_sd_complete.py --sd-root <DRIVE_LETTER>:\
   ```
2. Insert the MicroSD card and reboot (or rescan in Diagnostics).
   - **Expected serial/UI result:** Catalog builds/loads `/.bmo_index` (`BMOIDX03`) without SD lock contention errors (`SpiArbiter`).
3. Launch one Game Boy (`.gb`), Game Boy Color (`.gbc`), or NES (`.nes`) homebrew/test ROM:
   - Press **`START` + `SELECT`** to open the Pause menu (remember `X`, `Y`, `L`, `R` are unsoldered on the perfboard).
   - Save a state (**Slot 1** -> `.s01` file on SD), make a few moves in the game, then load **Slot 1** back.
   - **Expected result:** Game resumes cleanly at the saved state without crashing (`CRC32` verified) and battery SRAM `.sav` persists across a full power cycle.

---

## Step 7: 600-Frame Serial Telemetry Capture

1. Capture at least **15 seconds** (600+ frames) of serial output while running a game at steady gameplay into a text file, for example `bench_gb_log.txt` (do **not** commit personal logs containing ROM titles if undesired).
2. Analyze the captured log using:
   ```powershell
   python tools/telemetry_plotter.py --input bench_gb_log.txt --output bench_report.txt --json-out bench_report.json
   ```
   - **Expected serial lines present in log:** `[INFO] FPS: ... | Emu: ... target=...us | Frame: avg=...us min=...us max=...us` and `Frame window: ... p50=...us p95=...us p99=...us`.
   - Confirm `Data source: CAPTURED` (not `SYNTHETIC`) is printed.

---

## What to Report Back

Record the following details (or paste serial output + multimeter readings) to promote checks from `TARGET_COMPILED` to `HARDWARE_TESTED` in `docs/hardware_measurements.md` and `docs/performance.md`:

1. `3V3` / `5V` unpowered resistance readings (`> 100 Ω`?).
2. Exact serial boot banner line (`BMO Handheld v0.9.0-dev ...`).
3. Measured `GPIO 15` (`SD_MISO`) DC voltage with MicroSD inserted (`<= 3.35 V`?).
4. Highest glitch-free SPI speed (`40 MHz` only vs `80 MHz` stable).
5. Button test pass/fail for all 8 wired buttons (`UP/DOWN/LEFT/RIGHT/A/B/START/SELECT`).
6. Copy of `bench_report.txt` and `bench_report.json` generated by `tools/telemetry_plotter.py`.
