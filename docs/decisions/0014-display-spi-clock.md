# ADR 0014: Display SPI Clock (`80 MHz` vs `40 MHz`), Wire-Time Budget, and Signal Integrity

- Status: Proposed (`TFT_SPI_HZ = 80000000UL` preserved as default pending bench measurement; Safe Mode `40 MHz` active)
- Date: 2026-11-13 (Updated: 2026-11-15, verified at `d8d4b5e`)
- Decision owner: User (`docs/owner-decisions.md`)

## 1. Datasheet vs ESP32-S3 Clock Divider Facts

1. **Sitronix ST7789VW Datasheet Limit:** Section 7.4.1 (*Serial Interface Characteristics (4-line serial system)*) of the Sitronix ST7789VW v1.0 datasheet specifies a minimum write clock cycle time of **`tWC = 16 ns` (`62.5 MHz` maximum)** and a minimum read cycle time of `tRC = 150 ns` (`6.67 MHz`).
2. **ESP32-S3 GP-SPI Clock Generation:** Per the *ESP32-S3 Technical Reference Manual* v1.5 (Chapter 30, *GP-SPI*) and Arduino-ESP32 `3.3.11` (`esp32-hal-spi.c` `spiFrequencyToClockDiv()`), the FSPI master clock derives from the **`80 MHz` APB clock** using integer dividers (`80 MHz / N`):
   - `N = 1`: **`80.0 MHz`** (`12.5 ns` period — `28%` above the ST7789VW `62.5 MHz` datasheet rating, though commonly tolerated by hobbyist ST7789 panels on short traces).
   - `N = 2`: **`40.0 MHz`** (`25.0 ns` period — within the `62.5 MHz` datasheet limit).
   - No intermediate frequency (such as `60 MHz` or `62.5 MHz`) can be generated from the `80 MHz` APB clock without changing PLL/CPU clocking.

---

## 2. Analytical SPI Wire-Time Budget (`80 MHz` vs `40 MHz`)

Pure serial wire time (`T_wire = W × H × 16 bits / f_SPI`, excluding `CS`/`DC` window setup and FreeRTOS queue handoff; `HOST_TESTED` via `python -m tools.guardian bus-calc`):

| Viewport Mode | Resolution (`RGB565`) | Frame Bytes | Target Frame Period | Wire Time @ `80 MHz` (% of Period) | Wire Time @ `40 MHz` (% of Period) | Fits 60 FPS Full-Frame @ `40 MHz`? |
| :--- | :--- | ---: | ---: | ---: | ---: | :--- |
| **Full Screen (`BMO UI / Menu`)** | `320 × 240` | `153,600 B` | `16.67 ms` (`60 Hz`) | `15.36 ms` (`92.2%`) | **`30.72 ms` (`184.3%`)** | **NO** (Caps at `~32.5 FPS` full-frame) |
| **Sega Genesis (`Gwenesis`)** | `320 × 224` | `143,360 B` | `16.67 ms` (`60 Hz`) | `14.34 ms` (`86.0%`) | **`28.67 ms` (`172.0%`)** | **NO** (Requires frameskip=1 at `40 MHz`) |
| **DOOM (`doomgeneric`)** | `320 × 200` | `128,000 B` | `28.57 ms` (`35 Hz`) | `12.80 ms` (`44.8%`) | **`25.60 ms` (`89.6%`)** | **YES** (Fits `35 Hz` DOOM tic period!) |
| **NES / PC Engine** | `256 × 240` | `122,880 B` | `16.67 ms` (`60 Hz`) | `12.29 ms` (`73.7%`) | **`24.58 ms` (`147.5%`)** | **NO** (Requires frameskip or `240`-col crop) |
| **SNES (`Snes9x`)** | `256 × 224` | `114,688 B` | `16.67 ms` (`60 Hz`) | `11.47 ms` (`68.8%`) | **`22.94 ms` (`137.6%`)** | **NO** (Requires frameskip=1 at `40 MHz`) |
| **Game Boy / GBC (`3:2` scaled)** | `240 × 216` | `103,680 B` | `16.74 ms` (`59.73 Hz`) | `10.37 ms` (`61.9%`) | **`20.74 ms` (`123.9%`)** | **NO** (`20.74 ms > 16.74 ms` DMG period!) |
| **SMS / Coleco / SG-1000** | `256 × 192` | `98,304 B` | `16.67 ms` (`60 Hz`) | `9.83 ms` (`59.0%`) | **`19.66 ms` (`118.0%`)** | **NO** (`19.66 ms > 16.67 ms`) |
| **WonderSwan (`Oswan`)** | `224 × 144` | `64,512 B` | `16.67 ms` (`60 Hz`) | `6.45 ms` (`38.7%`) | **`12.90 ms` (`77.4%`)** | **YES** |
| **Atari 2600 (`Stella`)** | `160 × 192` | `61,440 B` | `16.67 ms` (`60 Hz`) | `6.14 ms` (`36.9%`) | **`12.29 ms` (`73.7%`)** | **YES** |
| **Neo Geo Pocket (`RACE`)** | `160 × 152` | `48,640 B` | `16.67 ms` (`60 Hz`) | `4.86 ms` (`29.2%`) | **`9.73 ms` (`58.4%`)** | **YES** |
| **Game Gear (`SMS Plus`)** | `160 × 144` | `46,080 B` | `16.67 ms` (`60 Hz`) | `4.61 ms` (`27.6%`) | **`9.22 ms` (`55.3%`)** | **YES** |
| **PICO-8 (`FAKE-08`)** | `128 × 128` | `32,768 B` | `33.33 ms` (`30 Hz`) | `3.28 ms` (`9.8%`) | **`6.55 ms` (`19.7%`)** | **YES** |
| **Atari Lynx (`Handy`)** | `160 × 102` | `32,640 B` | `16.67 ms` (`60 Hz`) | `3.26 ms` (`19.6%`) | **`6.53 ms` (`39.2%`)** | **YES** |

**Key Takeaway:** At `40 MHz`, full-frame `240×216` Game Boy (`20.74 ms`) cannot stream at `59.73 Hz` (`16.74 ms`) without dropping every 4th–5th frame or rendering native unscaled `160×144` (`46,080 B` = `9.22 ms` at `40 MHz`). Conversely, at `80 MHz`, every console viewport fits inside a single `16.67 ms` frame period, **provided the physical perfboard wiring maintains clean `80 MHz` edges**.

---

## 3. Perfboard Signal-Integrity Mitigations for `80 MHz` SPI

Point-to-point jumper wires on a `0.254 cm` perfboard have approximately `7–10 nH/cm` of series inductance, which causes ringing and overshoot on sub-2 ns ESP32-S3 GPIO edges at `80 MHz`. To maximize `80 MHz` reliability on the physical perfboard:

1. **Series Source-Termination Resistors (`22 Ω` to `33 Ω`):** Solder a `22 Ω` or `33 Ω` (`1/8 W` through-hole or `0603`) resistor in series with **`TFT_SCK` (`GPIO 12`)** and **`TFT_MOSI` (`GPIO 11`)** as close as possible (`< 0.8 cm`) to the ESP32-S3 pin headers. This matches the `~30–40 Ω` output impedance of the ESP32-S3 pad to the `~65–80 Ω` characteristic impedance of perfboard hookup wire and damps ringing at the ST7789 input.
2. **Short Wire Length (`< 6.0 cm`) + Ground Return Pair:** Route `TFT_SCK` (`GPIO 12`) and `TFT_MOSI` (`GPIO 11`) as the shortest wires on the board (`<= 6.0 cm`), twisted or bundled tightly alongside a dedicated **`GND` return wire** from ESP32-S3 `GND` to Display `GND` to minimize loop area.
3. **`10 kΩ` Pull-Up Resistors on `TFT_CS` (`GPIO 10`) and `SD_CS` (`GPIO 13`) to `3V3`:** Prevents the MicroSD card from driving `MISO` (`GPIO 15`) or corrupting the shared SPI bus before software initializes GPIO outputs.

---

## 4. Options for Owner Decision (`docs/owner-decisions.md`)

- **Option A (Keep `80 MHz` Default + `40 MHz` Safe Mode & NVS Toggle — Current):** Preserves `59.73 Hz` `240×216` scaled GB/GBC and `60 Hz` NES/Genesis/SNES wire budgets; user can hold `SELECT` at boot or toggle `40 MHz` in Diagnostics if their wiring shows glitches.
- **Option B (Switch Default to `40 MHz` + Add Automatic Frameskip / Native `160×144` GB Option):** Guarantees ST7789VW datasheet compliance (`40 MHz <= 62.5 MHz`) on any perfboard wiring, using `1:1` native `160×144` (`9.22 ms`) or automatic frameskip when `40 MHz` is active.
- **Menu Optimization Proposal:** Cap static launcher/menu redraws to `30 FPS` (`33.3 ms` period) and dirty-rectangle updates to cut SPI bus occupancy and save `~25–40 mA` of battery current in menus.
