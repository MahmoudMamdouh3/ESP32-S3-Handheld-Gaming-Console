# ADR 0006: Rechargeable AA NiMH Power Architecture & Electrical Budget

- Status: Proposed (Hard Stop — `FEATURE_BATTERY_MONITOR=0` in `config.h` until hardware is built and measured)
- Date: 2026-11-07 (Updated: 2026-11-15, verified at `d8d4b5e`)
- Decision owner: User (`docs/owner-decisions.md`)

## 1. Context & Assumptions

The handheld requires a safe, pocketable battery power system using standard **rechargeable AA Nickel-Metal Hydride (`NiMH`) cells** (`1.2 V` nominal, `2,000 mAh` low-self-discharge such as Panasonic Eneloop `BK-3MCCE`), charged **externally only** (never use a `TP4056` Li-ion charger on NiMH).

> **Evidence Note (`NOT_TESTED`):** Until physical current is measured in [`docs/hardware_measurements.md`](../hardware_measurements.md), system load at `5.0 V` (`VBUS`) is modeled as an **unverified design assumption of `250–350 mA` (`1.25–1.75 W`)** during active emulation (ESP32-S3 dual-core @ `240 MHz` + Octal PSRAM `~110–150 mA`, `2.4"` ST7789 backlight `~60–90 mA`, MicroSD bursts `~40–80 mA`), with peak transient spikes up to `450 mA` (`2.25 W`).

---

## 2. Numerical Comparison: `2×AA NiMH` vs `3×AA NiMH`

NiMH cell voltage ranges from **`1.40 V` (fresh off charger)** -> **`1.20 V` (nominal plateau)** -> **`1.00 V` (end-of-discharge cutoff)**. Assuming a synchronous `5.0 V` boost converter with `η = 85%` efficiency (`P_in = P_out / 0.85 = 1.47–2.06 W` average, `2.65 W` peak):

| Parameter | Option A: `2×AA NiMH` (Series, Recommended for `3.2 cm` Shell) | Option B: `3×AA NiMH` (Series, Longer Runtime) |
| :--- | :--- | :--- |
| **Stack Voltage Range** | **`2.00 V` (empty) to `2.80 V` (full)** (`2.40 V` nominal) | **`3.00 V` (empty) to `4.20 V` (full)** (`3.60 V` nominal) |
| **Input Current @ Nominal Voltage** | `0.61–0.86 A` @ `2.40 V` | `0.41–0.57 A` @ `3.60 V` |
| **Input Current @ `1.0 V/cell` Cutoff** | **`0.74–1.03 A` continuous** (`1.33 A` peak @ `2.00 V`) | **`0.49–0.69 A` continuous** (`0.88 A` peak @ `3.00 V`) |
| **Required Boost Switch Rating** | **`>= 2.0 A` input switch current**, startup `<= 1.8 V` (e.g., Pololu `S7V7F5` / TI `TPS61230A`) | **`>= 1.5 A` input switch current**, startup `<= 2.5 V` |
| **Stored Energy (`2,000 mAh` cells)** | `2.40 V × 2.0 Ah = 4.80 Wh` (`4.08 Wh` delivered @ `85%`) | `3.60 V × 2.0 Ah = 7.20 Wh` (`6.12 Wh` delivered @ `85%`) |
| **Estimated Runtime (`1.25–1.75 W` load)** | **`2.3 to 3.3 hours`** (`t = 4.08 Wh / P_load`) | **`3.5 to 4.9 hours`** (`t = 6.12 Wh / P_load`) |
| **Holder Footprint (with springs/walls)** | **`~3.2 cm × 5.8 cm × 1.6 cm`** (fits easily inside `8.8 cm` width) | **`~4.8 cm × 5.8 cm × 1.6 cm`** (requires `1.6 cm` more vertical height) |
| **Weight (Cells + Holder)** | `~62 g` (`2 × 27 g` cells + `8 g` holder) | `~92 g` (`3 × 27 g` cells + `11 g` holder) |

**Runtime Formula:**
$$\text{Runtime (hours)} = \frac{N_{\text{cells}} \times 1.20\,\text{V} \times C_{\text{Ah}} \times \eta_{\text{boost}}}{5.0\,\text{V} \times I_{\text{load (A)}}}$$

---

## 3. Mandatory Protection & Power-Path Isolation Circuit

```text
[AA NiMH Stack (+)] ---> [F1: 1.5A Polyfuse] ---> [SW1: Slide Switch] ---> [Q1: P-FET Reverse Prot] ---> [Boost 5.0V Out] ---> [D1: Ideal Diode / 1N5819] ---> [ESP32-S3 5V / VBUS]
                                                                                                                                                                      ^
[USB-C 5V (UART Port)] -------------------------------------------------------------------------------------------------------> [D2: Ideal Diode / 1N5819] -----------+
```

1. **Over-Current Fuse (`F1`):** `1.5 A` hold / `3.0 A` trip resettable polymeric PTC fuse (`Bourns MF-R150` or equivalent) soldered directly at the battery holder `+` lug.
2. **Physical Power Switch (`SW1`):** High-current slide switch (`>= 1.5 A` DC contact rating, e.g., `OS102011MA1QN1`) placed on the battery input side so quiescent current is `0 µA` when switched OFF.
3. **Reverse-Polarity Protection (`Q1`):** Low-$R_{\text{DS(on)}}$ logic-level P-channel MOSFET (`AO3401A`, $V_{\text{GS(th)}} \approx -0.9\,\text{V}$, $R_{\text{DS(on)}} < 60\,\text{m}\Omega$ at $-2.5\,\text{V}$) ahead of the boost converter so backwards AA cells cannot damage the boost IC.
4. **USB-C / Battery Power-Path OR-ing (`D1`/`D2`):** Pair of ideal-diode controllers (`TI LM66100` dual ideal diode) or low-drop `1N5819` / `SS24` Schottky barrier diodes prevent the `5.0 V` boost converter and USB-C `5 V` from back-feeding each other when flashing over USB.
5. **Under-Voltage Lockout (UVLO):** Adjust the boost converter `EN` resistor divider so the converter shuts off cleanly at `1.95 V` (`2×AA`) or `2.90 V` (`3×AA`), preventing deep cell reversal and ESP32-S3 flash brownout resets.

---

## 4. Battery Monitor ADC Design (Keep `FEATURE_BATTERY_MONITOR=0` Until Built)

- Tap the battery stack **after `SW1` and before the boost converter** (`V_BAT = 2.0–2.8 V` for `2×AA`, or `3.0–4.2 V` for `3×AA`).
- Use a **`100 kΩ` / `100 kΩ` (`1%`) voltage divider** with a **`0.1 µF` ceramic capacitor** to `GND` routed to **`GPIO 1` (`ADC1_CH0`)**:
  - For `2×AA` (`2.0–2.8 V`): `V_ADC = 1.00–1.40 V` (well within ESP32-S3 `ADC_ATTEN_DB_12` linear range).
  - For `3×AA` (`3.0–4.2 V`): `V_ADC = 1.50–2.10 V` (safe `< 3.3 V` on `GPIO 1`).
- Because the NiMH discharge curve is flat (`1.20–1.25 V/cell` from `80%` down to `20%` state-of-charge), the UI must display **coarse 3-bar status only** (`FULL` / `MID` / `LOW`), not a synthetic `0–100%` integer.

## 5. Recommendation

Select **Option A (`2×AA NiMH` + `>= 2 A` Pololu `S7V7F5` 5 V boost converter)** if preserving the compact `8.8 × 12.4 × 3.2 cm` enclosure is the top priority (`~2.3–3.3 hours` runtime), or **Option B (`3×AA NiMH`)** if `~3.5–4.9 hours` runtime is preferred. Keep `FEATURE_BATTERY_MONITOR=0` until the divider is physically wired and verified with a multimeter.
