# Bill of Materials (BOM) & Component Envelope Specification

- **Last Verified Commit:** `d8d4b5e` (`2026-11-15`)
- **Dimension Convention:** All physical dimensions in documentation use **centimetres (`cm`)** (except the nominal `2.4"` display diagonal). Convert `1 cm = 10 mm` at the CAD boundary (`hardware/layout.json`).
- **Verification Status:** Electrical ratings are sourced from manufacturer datasheets; physical dimensions of the owner's specific modules are marked `WAITING` until verified with calipers in [`docs/hardware_measurements.md`](hardware_measurements.md).

---

## 1. Core Electronic Modules & Passive Components

| Ref | Component | Spec / Part Number | Nominal Size (`W × H × D cm`) | Voltage / Current Rating | Status / Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **U1** | MCU Development Board | `ESP32-S3-DevKitC-1-N16R8` (44-pin carrier with `ESP32-S3-WROOM-1-N16R8` module, 16 MB Quad Flash, 8 MB Octal PSRAM) | `2.54 × 6.33 × 1.35 cm` (including pin headers; `WAITING` caliper check) | `5.0 V` USB/VBUS in, `3.3 V` GPIO (`~110–180 mA` active @ `240 MHz`) | Soldered on back of perfboard (`UART` USB-C accessible for flashing) |
| **DISP1** | `2.4"` SPI IPS/TFT Display | `240 × 320` Sitronix `ST7789` (`ST7789VW`) 4-wire SPI module (active area `4.88 × 3.66 cm`) | `~6.00 × 4.30 × 0.45 cm` PCB (`WAITING` caliper check of active window offset) | `3.3 V` logic & VCC (`~60–90 mA` backlight) | Shared FSPI (`SCK=12`, `MOSI=11`, `CS=10`, `DC=8`, `RST=9`) |
| **SD1** | MicroSD Card Interface | `3.3 V`-native MicroSD breakout (or integrated SD slot on `DISP1` PCB) + `8–32 GB` FAT32 MicroSDHC card | `~1.80 × 1.80 × 0.30 cm` (`WAITING` caliper check) | `3.3 V` logic (`<= 3.35 V` on `GPIO 15` `SD_MISO`; `~40–80 mA` read/write burst) | Shared FSPI (`SCK=12`, `MOSI=11`, `MISO=15`, `CS=13`) |
| **SW_DPAD** | D-Pad Tactile Switches (`×4`) | `6×6 mm` (`0.60 × 0.60 cm`) or `12×12 mm` (`1.20 × 1.20 cm` OMRON `B3F-4055`) through-hole tactile switches | `0.60 × 0.60 × 0.50 cm` (or `1.20 × 1.20 × 0.73 cm`; `WAITING` owner selection) | `3.3 V`, `< 1 mA` (`INPUT_PULLUP` to `GND`) | `UP=4`, `DOWN=5`, `LEFT=6`, `RIGHT=7` (`±1.016 cm` = 4 perfboard holes from D-pad center) |
| **SW_AB** | Action Tactile Switches (`×2` wired, `+2` optional `X/Y`) | Through-hole tactile switches (`0.60 × 0.60 cm` or `1.20 × 1.20 cm` OMRON `B3F`) | Same as `SW_DPAD` (`WAITING` caliper check) | `3.3 V`, `< 1 mA` (`INPUT_PULLUP` to `GND`) | Wired: `A=16`, `B=17`. Optional unsoldered: `X=14`, `Y=42` |
| **SW_SYS** | `SELECT` / `START` Switches (`×2`) | `0.60 × 0.60 cm` (`6×6 mm`) through-hole tactile switches | `0.60 × 0.60 × 0.50 cm` | `3.3 V`, `< 1 mA` (`INPUT_PULLUP` to `GND`) | Wired: `START=18`, `SELECT=21` |
| **SW_LR** | Shoulder Switches (`×2` optional) | Right-angle (side-actuated) through-hole tactile switches (`6×6 mm` right-angle, e.g., `EVQ-P1D05M`) | `0.62 × 0.74 × 0.43 cm` | `3.3 V`, `< 1 mA` (`INPUT_PULLUP` to `GND`) | Optional unsoldered: `L=47`, `R=48` on top edge of perfboard |
| **R_CS1, R_CS2** | SPI Chip-Select Pull-Up Resistors (`×2`) | `10 kΩ`, `1/4 W` or `1/8 W` `1%` metal-film through-hole resistors | `0.254 cm` (`1` hole) or `0.508 cm` (`2` holes) lead spacing | `3.3 V` (`0.33 mA` when low) | `TFT_CS` (`GPIO 10`) -> `3V3` and `SD_CS` (`GPIO 13`) -> `3V3` |
| **R_SCK, R_MOSI** | SPI Series Termination Resistors (`×2`) | `22 Ω` to `33 Ω`, `1/8 W` metal-film resistors | Placed at `GPIO 12` (`SCK`) and `GPIO 11` (`MOSI`) ESP32-S3 pins | `3.3 V` high-speed SPI damping | Damps `80 MHz` ringing on perfboard jumper wires ([ADR 0014](decisions/0014-display-spi-clock.md)) |
| **C_DEC1, C_DEC2** | Rail Decoupling Capacitors (`×2`) | `10 µF` (`6.3 V+` X5R/X7R) + `0.1 µF` (`100 nF`) ceramic radial capacitors | `0.254 cm` (`2.54 mm`) lead pitch | Across `3V3` and `GND` near `SD1` and `DISP1` | Suppresses `3.3 V` rail droop during MicroSD card insertion/read bursts |
| **PCB1** | Prototyping Perfboard | Double-sided FR-4 plated-through-hole (`PTH`) perfboard, `0.254 cm` (`2.54 mm`) grid | Cut from `9.0 × 15.0 cm` stock to **`7.87 × 11.68 cm` (`31 × 46` holes, `0.16 cm` thick)** | — | Fits inside `8.8 × 12.4 cm` shell with `0.21 cm` walls |

---

## 2. Portable AA NiMH Power Components (Proposed — [ADR 0006](decisions/0006-portable-power.md))

*Do not wire until USB bench bring-up ([`docs/guides/bringup.md`](guides/bringup.md)) is complete and the owner selects `2×AA` vs `3×AA` in `docs/owner-decisions.md`.*

| Ref | Component | Recommended Part | Nominal Size (`W × H × D cm`) | Rating | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **BAT1** | Rechargeable AA NiMH Cells (`×2` or `×3`) | Panasonic Eneloop `BK-3MCCE` (`1.2 V`, `2,000 mAh` min / `2,100 mAh` typ) | `1.45 cm Ø × 5.05 cm` per cell | `1.0–1.4 V` per cell | Charge externally only in a smart NiMH charger |
| **BH1** | AA Battery Holder | `2×AA` holder (`Keystone 2462` or lidded box) or `3×AA` holder | `2×AA`: `~3.20 × 5.80 × 1.65 cm`; `3×AA`: `~4.80 × 5.80 × 1.65 cm` (`WAITING`) | Series contacts (`2.4 V` or `3.6 V`) | Oriented horizontally across the lower back shell |
| **F1** | Resettable Polyfuse | Bourns `MF-R150` (or `MF-R110`) radial PTC | `0.85 × 1.40 × 0.30 cm` | `1.5 A` hold / `3.0 A` trip | Soldered inline at battery holder `+` terminal |
| **SW_PWR** | Power Slide Switch | SPDT / SPST slide switch (`OS102011MA1QN1` or `SS-12D00G4`) | `0.86 × 0.43 × 0.90 cm` | `>= 1.5 A @ 12 V DC` | Mounted on right edge (`y ≈ 3.0 cm`) |
| **Q1** | Reverse-Polarity MOSFET | `AO3401A` (`SOT-23` on `0.254 cm` adapter) or `IRF9540N` / `FQP27P06` | `0.80 × 1.00 × 0.45 cm` | $V_{\text{DS}} = -30\,\text{V}$, $I_D \ge 3\,\text{A}$, $R_{\text{DS(on)}} < 60\,\text{m}\Omega$ | Protects boost converter if AA cells are inserted backwards |
| **U2** | `5.0 V` Synchronous Boost Converter | Pololu `S7V7F5` (`#2119`) or TI `TPS61230A` module | `1.15 × 1.65 × 0.35 cm` | `2.0–5.5 V` in -> `5.0 V` out, `>= 2.0 A` switch current | Steps `2.0–4.2 V` NiMH stack up to regulated `5.0 V` |
| **D1, D2** | USB-C / Battery Power Isolation | TI `LM66100` ideal diode (`×2`) or `1N5819` / `SS24` Schottky diodes (`×2`) | `0.50 × 0.90 cm` | `>= 1.0 A`, `V_F <= 0.35 V` | Prevents USB-C `5 V` and Boost `5 V` from back-feeding each other |
| **R_BAT1, R_BAT2, C_BAT** | Battery ADC Divider (Dormant until enabled) | `100 kΩ` (`1%`, `×2`) + `0.1 µF` ceramic cap | `0.254 cm` pitch | Connected to `GPIO 1` (`ADC1_CH0`) | Keep `FEATURE_BATTERY_MONITOR=0` until soldered and measured |
