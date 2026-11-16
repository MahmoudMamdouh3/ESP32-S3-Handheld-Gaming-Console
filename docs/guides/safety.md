# Electrical & Battery Safety Guide (Beginner Reference)

- **Audience:** First-time hardware builder hand-soldering on a `0.254 cm` (`2.54 mm`) pitch perfboard.
- **Golden Rule:** Never connect AA batteries and a USB-C cable at the same time — always turn the battery switch **OFF** and remove at least **one AA cell** before plugging in USB-C (`ESP32-S3-DevKitC-1` power inputs are mutually exclusive).

---

## 1. Why Rechargeable AA NiMH Cells Are Chosen Over LiPo Packs

This handheld is designed to use **2 or 3 AA-size Nickel-Metal Hydride (`NiMH`) rechargeable cells** (such as Panasonic Eneloop `1.2 V`, `1,900 mAh` min; [BK-3MCCF Datasheet](https://eneloop101.com/download/datasheets/BK-3MCCF.pdf), accessed `2026-11-15`), **not** a Lithium-Polymer (`LiPo`) pouch cell:

- **External smart charging & rigid steel cans:** AA NiMH cells use sturdy metal cans charged outside the handheld in an external smart wall charger (e.g., Panasonic BQ-CC17 or BQ-CC55).
- **Mandatory Insulating Sheet (`0.09 cm` Z Clearance):** Always place a physical **insulating barrier sheet** (`~0.05 cm` / `0.5 mm` Kapton tape, fishpaper, or PET plastic sheet) between the back of the AA battery holder and the perfboard solder joints (`hardware/layout.json` leaves only `0.09 cm` clearance, so clipped solder pins can puncture thin insulation and short-circuit wires).
- **No low-battery auto-cutoff yet:** No hardware low-voltage cutoff is installed yet (`FEATURE_BATTERY_MONITOR=0`, and `Adafruit #2030` runs down to `~1.8 V`, below `1.0 V/cell`). Stop playing and recharge promptly if the screen dims or restarts.

---

## 2. Six Mandatory Battery Safety Rules

1. **NEVER connect a `TP4056` or Li-ion charger module to AA NiMH cells.**
   - A `TP4056` charges to `4.20 V` using constant-current/constant-voltage (`CC/CV`) for Lithium chemistry. Applying a `TP4056` to NiMH cells will overcharge, overheat, and vent the cells.
2. **NEVER charge batteries inside the handheld.**
   - Always remove the AA NiMH cells through the rear battery door and charge them in an external NiMH smart charger.
3. **NEVER mix battery types, brands, or charge levels.**
   - Do not mix NiMH (`1.2 V`) with non-rechargeable Alkaline (`1.5 V`) or Lithium (`1.5 V`) AA cells. Always insert a matched set of fully charged NiMH cells of the same model.
4. **ALWAYS wire a fuse and power switch on the positive (`+`) battery wire.**
   - Even AA NiMH cells can deliver `> 5 A` into a dead short, which will melt thin `28–30 AWG` wires and burn your fingers or melt the 3D-printed PLA/PETG shell.
   - Solder a properly sized [Bourns MF-R](https://www.bourns.com/docs/Product-Datasheets/mfr.pdf) resettable polyfuse (`MF-R300` `3.0 A` hold for `2×AA`, or `MF-R250` `2.5 A` hold for `3×AA`; datasheet accessed `2026-11-15`) inline with the positive (`+`) wire right at the battery holder terminal, before a high-current power switch (`>= 2.0 A DC @ >= 5 V` such as [C&K 1101M2S3CQE2](https://www.littelfuse.com/media/graphics/reshare/datasheets/ck-slides-1000.pdf), or `>= 3.0 A DC` for `2×AA`; see [ADR 0006](../decisions/0006-portable-power.md)).
5. **NEVER plug in USB-C while AA batteries are installed or the battery switch is ON (No `D2` USB Diode Possible).**
   - Per [Espressif's ESP32-S3-DevKitC-1 Guide](https://docs.espressif.com/projects/esp-idf/en/v5.1.6/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html) (accessed `2026-11-16`), its three power paths (`USB/OTG`, `UART/COM`, and `5V` header pin) are **mutually exclusive**, and an external `D2` diode cannot be installed inside the soldered USB-C jacks. Moreover, `Adafruit #2030` outputs **`5.2 V`** intentionally ([Adafruit #2030](https://www.adafruit.com/product/2030), accessed `2026-11-15`), so a boost-side diode/ideal diode could push current toward USB if both are active.
   - **Mandatory Interlock Rule:** **Always turn the battery power switch OFF AND remove at least one AA cell whenever USB-C is connected.**
6. **Orient `Q1` (`DMP2035U` / `AO3401A` P-MOSFET) With `Source` Toward the Load (`Boost IN+`).**
   - Installing Drain and Source backwards conducts straight through the MOSFET body diode and gives **zero** reverse-battery protection:
   ```text
     SOT-23 Top View (2 pins bottom, 1 pin top):
            [Pin 3: Drain (D)] ----> Battery (+) / F1 Polyfuse side
                +-------+
                |DMP2035|
                +-------+
     [Pin 1: Gate (G)]   [Pin 2: Source (S)]
             |                    |
             v                    v
            GND          Load / Boost IN (+)
   ```

---

## 3. What To Do If Anything Gets Hot, Smells, or Smokes

1. **Unplug the USB-C cable immediately** and/or **flip the battery slide switch OFF and pop out an AA cell**.
2. **Do not touch the hot component with bare fingers** — let the board cool for 2 minutes on a non-flammable surface.
3. Common causes of sudden heating on a perfboard:
   - **`VCC` and `GND` swapped** on the MicroSD module or ST7789 display header.
   - **A stray whisker of solder** bridging `5V` or `3V3` to `GND` on the bottom of the perfboard.
   - **`GPIO 15` (`SD_MISO`) driven to `5 V`** by a 5 V MicroSD breakout, heating the ESP32-S3 ESD protection diode.
   - **An AA cell inserted backwards** in the battery holder.
4. Return to **Step 1 (Unpowered Continuity Checks)** in [`docs/guides/bringup.md`](bringup.md) and find the short with your multimeter before applying power again.
