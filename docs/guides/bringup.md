# First-Time Hardware Bring-Up Guide (Step-by-Step)

- **Target Hardware:** `ESP32-S3-DevKitC-1-N16R8` (`2.54 cm × 6.33 cm` carrier board with `ESP32-S3-WROOM-1-N16R8` module) soldered on a `0.254 cm` (`2.54 mm`) pitch perfboard, driving a `2.4"` (`6.10 cm` diagonal) `240×320` ST7789 SPI display, MicroSD breakout, and 8 tactile pushbuttons.
- **Evidence Status:** Firmware and the bring-up sketch are `TARGET_COMPILED`. Physical board verification is `NOT_TESTED` until you complete these 5 steps in order and record your readings in [`docs/hardware_measurements.md`](../hardware_measurements.md).
- **Safety First:** Read [`docs/guides/safety.md`](safety.md) before applying power.

---

## Step 1: Unpowered Continuity Checks (No USB, No Batteries)

Disconnect all USB cables and remove any batteries. Set your digital multimeter to **Continuity (beep mode) / Resistance ($\Omega$)**:

1. **Check `3V3` to `GND`:** Place the black probe on any `GND` pin and the red probe on the `3V3` pin of the ESP32-S3 board.
   - **PASS:** Resistance is **above `100 Ω`** (no continuous beep).
   - **FAIL:** Continuous beep or `< 10 Ω` -> **STOP.** You have a solder bridge between `3.3 V` and `GND`. Inspect the underside of the perfboard under a bright light before plugging in USB.
2. **Check `5V` (`VBUS`) to `GND`:** Place the black probe on `GND` and the red probe on `5V` / `VBUS`.
   - **PASS:** Resistance is **above `100 Ω`** (no continuous beep).
   - **FAIL:** Continuous beep or `< 10 Ω` -> **STOP.** Fix the `5V`-to-`GND` short before applying power.
3. **Check `3V3` to `5V`:** Place one probe on `3V3` and the other on `5V`.
   - **PASS:** Resistance is **above `100 Ω`** (the `3.3 V` and `5 V` rails must never be shorted together).
   - **FAIL:** `< 10 Ω` -> **STOP.** Separate the `3.3 V` and `5 V` traces immediately; `5 V` on the `3.3 V` rail will destroy the ESP32-S3 and ST7789 display.
4. **Check `GPIO 15` (`SD_MISO`) to `GND` and `5V`:**
   - **PASS:** `> 100 Ω` to both `GND` and `5V`.
   - **FAIL:** `< 10 Ω` -> **STOP.** Clear the solder bridge on `GPIO 15`.
5. **Check Unpressed Buttons:** Touch `GND` with the black probe and touch each of the 8 button pins (`GPIO 4, 5, 6, 7, 16, 17, 18, 21`) with the red probe while **not** pressing any button, then while pressing that button.
   - **PASS:** Open circuit (`OL` / `> 10 kΩ`) when released; `< 5 Ω` (beeps) only while that specific button is held down.
   - **FAIL:** Beeps while released -> **STOP.** The tactile switch is rotated 90 degrees (its internal always-connected legs are bridging `GPIO` to `GND`) or has a solder bridge.

---

## Step 2: First Power-Up on Current-Limited USB (No SD Card Inserted Yet)

1. Remove the MicroSD card from its slot for the very first power-up.
2. Plug a USB-C data cable into a **powered USB hub** or inline **USB power meter** (if available), then into the **`UART` / `COM` USB-C port** on the ESP32-S3 board. Keep one hand on the USB plug for the first 10 seconds and lightly touch the ESP32-S3 metal shield and LDO regulator (`U2`).
   - **PASS:** Nothing gets hot (stays cool or barely warm to the touch) and USB current draw is between **`40 mA` and `180 mA`** at `5.0 V`.
   - **FAIL:** Regulator or chip gets hot within seconds, or USB hub trips over-current (`> 400 mA`) -> **UNPLUG IMMEDIATELY.** Re-check Step 1 polarity on the display `VCC`/`GND` and MicroSD `VCC`/`GND` pins.

---

## Step 3: Measure `3V3`, `5V`, and `GPIO 15` (`SD_MISO`) Idle Voltages

Set your multimeter to **DC Volts (`20 V` range)**. Keep the black probe on `GND`:

1. **Measure `5V` (`VBUS`) pin:**
   - **PASS:** Between **`4.75 V` and `5.25 V`**.
   - **FAIL:** `< 4.50 V` -> Bad USB cable, overloaded port, or partial short.
2. **Measure `3V3` pin:**
   - **PASS:** Between **`3.20 V` and `3.40 V`**.
   - **FAIL:** `< 3.00 V` or `> 3.50 V` -> **UNPLUG IMMEDIATELY.** Onboard 3.3 V regulator is overloaded or damaged.
3. **Measure `GPIO 15` (`SD_MISO`) with the MicroSD card inserted:**
   - **PASS:** Between **`0.00 V` and `3.35 V`** at all times.
   - **FAIL:** **Above `3.60 V`** -> **UNPLUG IMMEDIATELY.** Your MicroSD breakout is pulling `MISO` up to `5 V`, which can damage the `3.3 V`-only ESP32-S3 pin. See [`docs/wiring/02-sd-card.md`](../wiring/02-sd-card.md) to power the breakout safely from `3.3 V` or use a 3.3 V-native socket.
4. Log all three readings in [`docs/hardware_measurements.md`](../hardware_measurements.md).

---

## Step 4: Flash the Minimal Bring-Up Sketch (`firmware/bringup/bringup.ino`)

Before flashing the full 3.29 MB multi-console firmware, flash the small, self-contained bring-up test sketch (`firmware/bringup/bringup.ino`). It runs the display at a conservative `40 MHz` SPI clock and tests one subsystem at a time:

1. Compile the bring-up sketch from the repository root:
   ```powershell
   .\arduino-cli.exe compile --profile esp32s3 --build-path build/bringup firmware/bringup
   ```
2. Flash it to your connected ESP32-S3 (`replace COM3 with your port from python tasks.py doctor`):
   ```powershell
   .\arduino-cli.exe upload --profile esp32s3 --port COM3 --input-dir build/bringup firmware/bringup
   ```
3. Open the serial monitor at `115200` baud:
   ```powershell
   python tasks.py monitor --port COM3
   ```
4. Check the 4 bring-up indicators on the screen and Serial monitor:
   - **4a. Octal PSRAM (`8 MiB`):**
     - **PASS:** Serial prints `[PSRAM] total=8388608 B ... 1MiB_memtest=PASS` and screen shows `PSRAM 8MB : PASS`.
     - **FAIL:** `total=0 B` or `FAIL` -> Verify `PSRAM=opi` and `FlashMode=qio` in `sketch.yaml` and ensure nothing is soldered to `GPIO 33–37`.
   - **4b. Display Color Bars (`ST7789` at `40 MHz`):**
     - **PASS:** Top of screen shows 8 crisp vertical bars (Red, Green, Blue, Yellow, Cyan, Magenta, White, Teal) and legible white text on a black background.
     - **FAIL:** White screen only -> Check `TFT_CS` (`GPIO 10`), `TFT_DC` (`GPIO 8`), `TFT_RST` (`GPIO 9`), `TFT_SCK` (`GPIO 12`), and `TFT_MOSI` (`GPIO 11`). Add a `10 kΩ` pull-up resistor from `TFT_CS` (`GPIO 10`) to `3V3`.
   - **4c. MicroSD Mount (`GPIO 13` CS, `25 MHz`):**
     - **PASS:** With a FAT32 MicroSD card inserted, screen shows `MicroSD : PASS (<size> MB)`.
     - **FAIL:** `NOT MOUNTED` -> Format the card as FAT32 (not exFAT), check `SD_CS` (`GPIO 13`) and `SD_MISO` (`GPIO 15`), and add a `10 kΩ` pull-up resistor from `SD_CS` (`GPIO 13`) to `3V3`.
   - **4d. 8 Wired Buttons (`UP=4, DOWN=5, LEFT=6, RIGHT=7, A=16, B=17, START=18, SELECT=21`):**
     - **PASS:** All 8 boxes on screen are dark when untouched, and each box turns bright green (and prints `[BTN]` on Serial) when pressed.
     - **FAIL:** A box stays green without being pressed -> Check that pin for a short to `GND`.

---

## Step 5: Flash Full Firmware & Compare `40 MHz` vs `80 MHz` SPI

Once Steps 1–4 pass and are recorded in [`docs/hardware_measurements.md`](../hardware_measurements.md):

1. Build and flash the full firmware:
   ```powershell
   python tasks.py build --profile default --warnings strict --fail-on-warnings
   python tasks.py flash --port COM3
   ```
2. Hold **`SELECT` (`GPIO 21`)** while pressing **`RST`** on the first boot so the handheld starts in **`40 MHz` Safe Mode** (`[SAFE_MODE]`).
3. Open **Diagnostics** on-screen and compare **`40 MHz`** vs **`80 MHz`** SPI clock stability (`ADR 0014`).
   - **PASS:** Both `40 MHz` and `80 MHz` render cleanly, or `40 MHz` is saved in NVS if `80 MHz` shows pixel noise on your perfboard wires.
   - **FAIL:** Reboot loop or garbled screen at `80 MHz` -> Hold `SELECT` at boot to lock `40 MHz`, and add `22–33 Ω` series resistors on `GPIO 11` (`MOSI`) and `GPIO 12` (`SCK`) at the ESP32-S3 pins.
