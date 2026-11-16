# ADR 0006: Rechargeable AA NiMH Power Architecture & Electrical Budget

- **Status:** Proposed (Hard Stop — `FEATURE_BATTERY_MONITOR=0` in `config.h` until hardware is built and measured)
- **Date:** 2026-11-07 (Revised: 2026-11-15)
- **Decision Owner:** Project Owner ([`docs/owner-decisions.md`](../owner-decisions.md) Decision 1)

> [!CAUTION]
> **Withdrawal & Correction Notice (2026-11-15):** Earlier revisions of this ADR, `docs/bom.md`, and `docs/owner-decisions.md` contained **false electrical claims** that have been withdrawn:
> 1. **`Pololu S7V7F5` (`#2119`):** Rated for **`2.7–11.8 V`** input and only **`~500 mA` max in step-up (boost) mode** (`~1 A` only in step-down mode; [Pololu #2119](https://www.pololu.com/product/2119), accessed `2026-11-15`), **not** `2.0–5.5 V` and `>= 2 A`. It fails both the `2×AA` voltage window and the `>= 1.0 A @ 5.0 V` requirement.
> 2. **`Pololu U3V40F5` (`#4012`):** Operates down to `1.3 V` only *after* starting, but requires **`2.7 V` minimum start-up** ([Pololu #4012](https://www.pololu.com/product/4012), accessed `2026-11-15`). It **cannot start** from a nominal `2×AA NiMH` pack (`~2.4 V`) and is valid **only** for `3×AA NiMH`.
> 3. **`OS102011MA1QN1` Slide Switch & `IRF9540N`/`FQP27P06` MOSFETs:** `OS102011MA1QN1` is rated for only `0.1 A (100 mA) @ 12 VDC`, and `IRF9540N`/`FQP27P06` require `-10 V` gate drive. None of those parts may be placed in the main AA battery power path.

---

## 1. Requirements Derived from the NiMH Discharge Curve

This handheld uses **rechargeable AA Nickel-Metal Hydride (`NiMH`) cells** (e.g., Panasonic Eneloop `BK-3MCCF` / `BK-3MCCE`, `1.20 V` nominal, `1,900 mAh` minimum / `2,000 mAh` typical; [Eneloop BK-3MCCF Datasheet](https://eneloop101.com/download/datasheets/BK-3MCCF.pdf), accessed `2026-11-15`), **charged externally only** in a dedicated smart NiMH charger (never connect a `TP4056` Li-ion charger).

> **Evidence Note (`NOT_TESTED`):** Physical handheld current draw has **not** yet been measured on the soldered board ([`docs/hardware_measurements.md`](../hardware_measurements.md)). Modeled typical draw at `5.0 V` (`VBUS`) is `~200–350 mA` (`1.0–1.75 W`: ESP32-S3-WROOM-1 active `~110–180 mA`, `2.4"` ST7789 display + backlight `~30–60 mA`, MicroSD bursts `~40–80 mA`). Until real bench current is measured, the power path is sized for **`I_out >= 1.00 A @ 5.0 V` (`P_out >= 5.00 W`) continuous**.

### 1.1 Discharge-Curve & Worst-Case Sizing Equations

A series AA NiMH pack spans:
- **Fresh off charger (`~1.45 V/cell`):** `2.90 V` (`2×AA`) or `4.35 V` (`3×AA`)
- **Nominal flat plateau (`~1.20–1.25 V/cell`):** `2.40–2.50 V` (`2×AA`) or `3.60–3.75 V` (`3×AA`)
- **End-of-discharge cutoff (`1.00 V/cell` under load):** **`2.00 V` (`2×AA`)** or **`3.00 V` (`3×AA`)** (stopping at `1.00 V/cell` prevents deep-discharge cell reversal)

Accounting for `~0.15–0.20 V` drop across springs, polyfuse, switch, and reverse-protection MOSFET under `1.00 A` output load, plus conservative boost efficiency $\eta = 85\%$ (`0.85`):

$$I_{\text{in,max}} \ge \frac{P_{\text{out}}}{\eta \times V_{\text{in,min}}} = \frac{5.00\,\text{W}}{0.85 \times V_{\text{in,min}}}$$

| Design Requirement | Option 1A: `2×AA NiMH` (Series) | Option 1B: `3×AA NiMH` (Series) |
| :--- | :--- | :--- |
| **Pack Voltage Range (`1.00–1.45 V/cell`)** | **`2.00 V` (cutoff) to `2.90 V` (full)** (`2.40 V` nom) | **`3.00 V` (cutoff) to `4.35 V` (full)** (`3.60 V` nom) |
| **Max Converter Start-Up Voltage** (`< V_pack_load`) | **`<= 1.80 V`** (must start reliably under load sag) | **`< 3.00 V`** (`<= 2.70–2.80 V`) |
| **Minimum Continuous `5.0 V` Output Current** | **`>= 1.00 A` (`5.00 W`)** down to `2.00 V` input | **`>= 1.00 A` (`5.00 W`)** down to `3.00 V` input |
| **Worst-Case Input Current ($I_{\text{in,max}}$ @ `85%`)** | **`2.94 A`** (`5.00 W / 0.85 / 2.00 V`) | **`1.96 A`** (`5.00 W / 0.85 / 3.00 V`) |
| **Min Downstream Switch & FET Continuous Rating** | **`>= 3.00 A DC`** (FET $R_{\text{DS(on)}}$ spec'd @ $V_{\text{GS}} \le -2.0\,\text{V}$) | **`>= 2.00 A DC`** (FET $R_{\text{DS(on)}}$ spec'd @ $V_{\text{GS}} \le -2.5\,\text{V}$) |
| **Polyfuse Hold Current ($I_{\text{hold}}$)** | **`3.00 A`** (`Bourns MF-R300`, `6.0 A` trip) | **`2.50 A`** (`Bourns MF-R250`, `5.0 A` trip) |
| **Stored Energy (`1,900 mAh` min Eneloop)** | `2.40 V × 1.9 Ah = 4.56 Wh` (`3.88 Wh` @ `85%`) | `3.60 V × 1.9 Ah = 6.84 Wh` (`5.81 Wh` @ `85%`) |
| **Estimated Runtime (`1.25–1.75 W` modeled load)** | **`~2.2 to 3.1 hours`** (`NOT_TESTED`) | **`~3.3 to 4.6 hours`** (`NOT_TESTED`) |
| **Standard Holder Envelope (`W × H × D`)** | **`~5.80 × 3.20 × 1.65 cm`** (fits existing bay) | **`~5.80 × 4.80 × 1.65 cm`** (collides in `12.40 cm` shell) |

$$\text{Runtime (hours)} = \frac{N_{\text{cells}} \times 1.20\,\text{V} \times C_{\text{Ah,min}} \times \eta_{\text{boost}}}{P_{\text{load (W)}}}$$

---

## 2. Vetted Step-Up Regulators (and Disqualified Parts)

### 2.1 Option 1A — `2×AA NiMH` Vetted Candidates (`V_start <= 1.80 V`, `I_out >= 1.00 A @ 2.0 V`)

1. **`Adafruit PowerBoost 1000 Basic` (`#2030`, `TI TPS61030`)** — [`adafruit.com/product/2030`](https://www.adafruit.com/product/2030) & [`TPS61030 Datasheet`](https://www.ti.com/lit/ds/symlink/tps61030.pdf) (accessed `2026-11-15`).
   - **Specs:** `1.8–5.5 V` input & start-up, `4.0 A` internal switch, `5.2 V` fixed output (`>= 1,000 mA` from `2.0 V`, `1,500 mA+` from `2×NiMH`), true shutdown via `EN`, `2.90 × 2.30 × 0.20 cm` (`0.75 cm` with JST).
   - **Notes:** Requires external reverse-polarity MOSFET (`DMP2035U`) and `MF-R300` polyfuse; verify continuous thermal rise at `2.0 V` in hardware bench testing.
2. **`TI TPS61022` Evaluation Module (`TPS61022EVM-034`) or Pre-Assembled `TPS61022` Carrier** — [`TPS61022 Datasheet`](https://www.ti.com/lit/ds/symlink/tps61022.pdf) & [`TPS61022EVM-034`](https://www.ti.com/tool/TPS61022EVM-034) (accessed `2026-11-15`).
   - **Specs:** **`1.8 V` minimum start-up** (operates down to **`0.5 V`** once running), **`6.5 A` min / `8.0 A` typ valley switch current limit**, `5.0 V` output (`>= 2.0–3.0 A` from `2.0 V` input), true load disconnect on `EN`.
   - **Notes:** Bare `TPS61022` is a `2×2 mm` VQFN package (not hand-solderable); use a pre-assembled carrier/EVM verified with calipers, plus external `DMP2035U` reverse protection and `MF-R300` polyfuse.

### 2.2 Option 1B — `3×AA NiMH` Vetted Candidates (`V_start < 3.00 V`, `I_out >= 1.00 A @ 3.0 V`)

1. **`Pololu U3V40F5` (`#4012`)** — [`pololu.com/product/4012`](https://www.pololu.com/product/4012) (accessed `2026-11-15`).
   - **Specs:** **`2.7 V` minimum start-up** (`< 3.00 V` cutoff), operates `1.3–5.0 V` once started, `4.0 A` continuous input switch limit (`~1.8–2.2 A` continuous @ `5.0 V` from `3.0 V` input), true shutdown (`EN`), `2.03 × 1.52 × 0.50 cm`.
   - **Notes:** **No built-in reverse-voltage protection** — requires external `Q1` (`DMP2035U` or `AO3401A`) + `MF-R250` polyfuse.
2. **`Pololu U3V70F5` (`#2891`)** (or `Pololu U3V50F5` `#2565`) — [`pololu.com/product/2891`](https://www.pololu.com/product/2891) & [`pololu.com/product/2565`](https://www.pololu.com/product/2565) (accessed `2026-11-15`).
   - **Specs:** `U3V70F5` operates `2.9–5.0 V` (`2.8 V` rising / `2.4 V` falling UVLO), `10 A` peak / `>= 6 A` input capability (`>= 3.0 A @ 5.0 V` from `3.0 V`), true shutdown (`EN`), and **built-in reverse-voltage protection** (`U3V50F5` adds `5 A` switch with built-in reverse protection from `2.9 V` min).
   - **Notes:** Built-in reverse-voltage protection eliminates external SOT-23 `Q1` soldering; `2.8 V` rising UVLO cleanly halts restart near `1.0 V/cell` cutoff.

### 2.3 Disqualified Parts (Do Not Purchase for Main Boost Stage)

| Part | Why Disqualified (Verified `2026-11-15`) | Source |
| :--- | :--- | :--- |
| `Pololu S7V7F5` (`#2119`) | `2.7–11.8 V` input; only `~500 mA` max in boost mode (`Vin < 5 V`). Fails `2×AA` voltage & `< 1.0 A` current. | [`pololu.com/product/2119`](https://www.pololu.com/product/2119) |
| `Adafruit MiniBoost TPS61023` (`#4654`) | Only `~300 mA @ 2.0 V` and `~500 mA @ 2.5 V` output (`NOT >= 1.0 A`). | [`adafruit.com/product/4654`](https://www.adafruit.com/product/4654) |
| `Adafruit PowerBoost 500` (`TPS61090`) | `2.0 A` switch gives `< 0.7 A @ 5.0 V` from `2.0 V` (`2.0 A × 2.0 V × 0.85 / 5 V = 0.68 A`). | [`adafruit.com/product/1903`](https://www.adafruit.com/product/1903) |
| `Pololu U1V11F5` / `TI TPS61230A` | `U1V11F5` is `1.4 A` max input; `TPS61230A` has `2.50 V` min `V_in` (fails `2.0 V` `2×AA` cutoff). | [`ti.com/lit/ds/symlink/tps61230a.pdf`](https://www.ti.com/lit/ds/symlink/tps61230a.pdf) |

---

## 3. Protection, Switch & Power-Path Isolation Circuit

```text
[AA NiMH Stack (+)] ---> [F1: MF-R300 (2xAA) / MF-R250 (3xAA)] ---> [SW1: >= 3A DC Slide/Toggle] ---> [Q1: DMP2035U Reverse Prot*] ---> [Boost 5.0V Out] ---> [D1: LM66100 / 1N5822] ---> [ESP32-S3 5V / VBUS]
                                                                                                                                                                                                    ^
[USB-C 5V (UART Port)] -------------------------------------------------------------------------------------------------------------------------------------------> [D2: LM66100 / 1N5822] ---------+
(* Omit external Q1 if using Pololu U3V70F5 / U3V50F5, which already includes built-in reverse-voltage protection.)
```

1. **Over-Current Fuse (`F1`):** [`Bourns MF-R`](https://www.bourns.com/docs/Product-Datasheets/mfr.pdf) (accessed `2026-11-15`) radial PPTC soldered directly at the battery holder `+` lug — **`MF-R300`** (`3.00 A` hold / `6.00 A` trip) for `2×AA` (`2.94 A` max), or **`MF-R250`** (`2.50 A` hold / `5.00 A` trip) for `3×AA` (`1.96 A` max). *(The previously cited `MF-R150` does not exist in the `MF-R` datasheet, and a `1.5 A` fuse trips below `1 A @ 5 V` from `2×AA`.)*
2. **Power Switch (`SW1`) — Two Safe Architectures:**
   - **Architecture A (Full Zero-Leakage Pack Disconnect):** Use a slide or toggle switch rated **`>= 2.0 A DC`** (minimum **`3.0 A DC` for `2×AA`**) @ `>= 5 V`: **`C&K 1101M2S3CQE2`** (`6 A @ 28 VDC`, [datasheet](https://www.littelfuse.com/media/graphics/reshare/datasheets/ck-slides-1000.pdf), accessed `2026-11-15`), **`C&K L202011MS02Q`** (`4 A @ 28 VDC`, [datasheet](https://www.littelfuse.com/media/graphics/reshare/datasheets/ck-slides-l.pdf), accessed `2026-11-15`), or **`NKK M2012SS1W01`** (`4 A @ 30 VDC`, [datasheet](https://www.mouser.com/pdfdocs/NKK_M_Series_Toggles_General_Specs.pdf), accessed `2026-11-15`).
   - **Architecture B (`EN`-Pin Shutdown Control):** Wire a miniature signal switch (carrying `< 1 mA`) to pull the converter `EN` pin to `GND` (OFF) or leave pulled up (ON). Note that `EN` shutdown draws `~20 µA` standby current from the cells if left inserted for months, whereas Architecture A cuts current to `0 µA`.
3. **Reverse-Polarity Protection (`Q1`):** Only logic-level P-MOSFETs with $R_{\text{DS(on)}}$ guaranteed at $V_{\text{GS}} \le -2.5\,\text{V}$:
   - **`Diodes Inc. DMP2035U`** ([datasheet](https://www.diodes.com/assets/Datasheets/DMP2035U.pdf), accessed `2026-11-15`): $-20\,\text{V}$, $-3.6\,\text{A}$, **$R_{\text{DS(on)}} \le 45\,\text{m}\Omega$ @ $-2.5\,\text{V}$**, **$\le 62\,\text{m}\Omega$ @ $-1.8\,\text{V}$** — works cleanly for **both `2×AA` (`2.0 V`) and `3×AA` (`3.0 V`)**.
   - `3×AA`-only alternatives: **`AOSMD AO3401A`** ($-30\,\text{V}$, $-4.0\,\text{A}$, $R_{\text{DS(on)}} \le 85\,\text{m}\Omega$ @ $-2.5\,\text{V}$; [AO3401A](https://www.aosmd.com/products/mosfets/standard/ao3401a), accessed `2026-11-15`) or **`onsemi FDN340P`** ($-20\,\text{V}$, $-2.0\,\text{A}$, $R_{\text{DS(on)}} \le 110\,\text{m}\Omega$ @ $-2.5\,\text{V}$; [FDN340P](https://www.mouser.com/datasheet/2/308/FDN340P-1808605.pdf), accessed `2026-11-15`) — neither is guaranteed down to `2.0 V` cutoff, so do not use on `2×AA`.
4. **USB-C / Battery Power-Path OR-ing (`D1`/`D2`):** Pair **`TI LM66100`** (`1.5 A`, `79 mΩ`, [datasheet](https://www.ti.com/lit/ds/symlink/lm66100.pdf), accessed `2026-11-15`) or **`1N5822`** Schottky diodes (`3.0 A`, `40 V`, [datasheet](https://www.st.com/resource/en/datasheet/1n582x.pdf), accessed `2026-11-15`; `1N5819` is only `1.0 A` max with zero margin). Even with OR-ing installed, switch battery OFF before USB flashing until bench-verified.

---

## 4. Enclosure Collision Check (`2×AA` vs `3×AA` Variant)

Running the layout/collision validator (`scripts/generate_hardware_layout_artifacts.py`) on `hardware/layout.json` (`2×AA`) vs `hardware/layout_3xaa_variant.json` (`3×AA` holder `5.80 × 4.80 × 1.65 cm` inside the unmodified `8.80 × 12.40 × 3.20 cm` shell) confirms:
- **`2×AA` (`hardware/layout.json` default):** `5.80 × 3.20 × 1.65 cm` in `y = 8.10..11.30 cm` clears `ESP32-S3-DevKitC-1` (`y_max = 7.577 cm`) by `0.523 cm`, inner bottom wall (`12.19 cm`) by `0.89 cm`, and has `0.09 cm` Z clearance (`0` collisions).
- **`3×AA` (`hardware/layout_3xaa_variant.json`):** Inside `8.80 × 12.40 × 3.20 cm`, total Y span between `DevKitC-1` bottom (`7.577 cm`) and inner bottom wall (`12.19 cm`) is only **`4.613 cm` (`-0.187 cm` shorter than a `4.80 cm` `3×AA` holder)** — centering at `y = 7.30..12.10 cm` causes a **`0.277 cm` XY overlap with `DevKitC-1` and `1.26 cm` Z collision (`4.46 cm` vs `3.20 cm` shell)**, while shifting below `DevKitC-1` breaches the bottom wall. Choosing `3×AA` requires growing the shell height to `~13.0–13.4 cm` (`+0.6–1.0 cm`) and expanding the rear battery door.

---

## 5. Battery Monitor ADC Design (Keep `FEATURE_BATTERY_MONITOR=0` Until Built)

- Tap pack `+` after `F1`/`SW1`/`Q1` and ahead of boost input -> **`100 kΩ` / `100 kΩ` (`1%`) divider** + **`0.1 µF` capacitor** to `GPIO 1` (`ADC1_CH0`): `1.00–1.45 V` (`2×AA`) or `1.50–2.18 V` (`3×AA`), safely inside ESP32-S3 ADC limits. Display coarse 3-bar status only (`FULL` / `MID` / `LOW`) after hardware calibration. Keep `FEATURE_BATTERY_MONITOR=0` until wired and measured.
