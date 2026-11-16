# Hardware Validation Kit (Merged Into [`docs/guides/bringup.md`](guides/bringup.md))

The standalone validation checklist and first-power-on bring-up steps have been **merged into one canonical sequence** in [**`docs/guides/bringup.md`**](guides/bringup.md) so electrical safety order cannot drift across two files.

Follow [**`docs/guides/bringup.md` (Steps 1–8)**](guides/bringup.md) in strict order and record every physical reading in [`docs/hardware_measurements.md`](hardware_measurements.md):

1. [**Step 1:**](guides/bringup.md#step-1-unpowered-continuity-pull-up--polarity-checks-usb--batteries-unplugged) Install `10 kΩ` CS pull-ups, disconnect SD `MISO`, check `VCC`/`GND` module polarity, rail shorts, and buttons.
2. [**Step 2:**](guides/bringup.md#step-2-power-on-with-sd-miso-disconnected--standalone-miso-test--335-v) Power on via USB with SD `MISO` disconnected, powering the SD module on its **permanent rail** (`3V3` for `3.3 V`-only, `5V` for `AMS1117` Type B), and verify standalone `MISO <= 3.35 V` both without and with a card before connecting `GPIO 15`.
3. [**Step 3 & Boot-Loop Fallback:**](guides/bringup.md#step-3-read-only-esptool-v531-chip--flash-verification-before-flashing-anything) Run read-only `esptool flash-id` / `chip-id` / `read-mac` (or `python -m esptool`) before any upload; use [`dio` / `opi` diagnostic commands](guides/bringup.md#boot-loop-fallback) only if needed.
4. [**Step 4:**](guides/bringup.md#step-4-flash-minimal-bring-up-sketch-first-firmwarebringupbringupino) Flash `firmware/bringup/bringup.ino` FIRST (`390,880 B` Flash, `24,600 B` RAM) and verify PSRAM, ST7789 bars, SD mount, and buttons.
5. [**Steps 5–7:**](guides/bringup.md#step-5-flash-full-bmogameboy-firmware--compare-40-mhz-vs-80-mhz-spi) Flash full `BmoGameboy`, compare `80 MHz` vs Safe Mode `40 MHz` SPI ([Sitronix ST7789 Datasheet](https://www.buydisplay.com/download/ic/ST7789.pdf), `62.5 MHz` max write, accessed `2026-11-16`), test SD catalog / `.sav` / `.s01`, and capture 600-frame telemetry via `tools/telemetry_plotter.py`.
6. [**Step 8:**](guides/bringup.md#step-8-before-adding-aa-batteries) Only add AA NiMH batteries after USB bring-up passes and [`docs/owner-decisions.md`](owner-decisions.md) Decision 1 + [`docs/decisions/0006-portable-power.md`](decisions/0006-portable-power.md) are finalized.
