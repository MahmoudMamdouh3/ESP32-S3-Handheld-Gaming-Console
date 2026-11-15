# Electrical & Battery Safety Guide (Beginner Reference)

- **Audience:** First-time hardware builder hand-soldering on a `0.254 cm` (`2.54 mm`) pitch perfboard.
- **Golden Rule:** Never connect batteries and a USB cable at the same time until a dedicated power-isolation circuit (Schottky diodes or an ideal-diode controller) is installed and verified with a multimeter.

---

## 1. Why Rechargeable AA NiMH Cells Are Safer Than LiPo Packs

This handheld is designed to use **2 or 3 AA-size Nickel-Metal Hydride (`NiMH`) rechargeable cells** (such as Panasonic Eneloop `1.2 V`, `2,000 mAh`), **not** a Lithium-Polymer (`LiPo`) pouch cell:

- **No thermal runaway from minor mechanical flexing:** Unlike soft LiPo pouches inside a 3D-printed case, steel-can AA NiMH cells do not catch fire if slightly pinched against perfboard solder points.
- **Standard external charger:** You remove the AA cells to charge them in a standard smart wall charger (e.g., Panasonic BQ-CC17 or BQ-CC55).

---

## 2. Five Mandatory Battery Safety Rules

1. **NEVER connect a `TP4056` or Li-ion charger module to AA NiMH cells.**
   - A `TP4056` charges to `4.20 V` using constant-current/constant-voltage (`CC/CV`) for Lithium chemistry. Applying a `TP4056` to NiMH cells will overcharge, overheat, and vent the cells.
2. **NEVER charge batteries inside the handheld.**
   - Always remove the AA NiMH cells through the rear battery door and charge them in an external NiMH smart charger.
3. **NEVER mix battery types, brands, or charge levels.**
   - Do not mix NiMH (`1.2 V`) with non-rechargeable Alkaline (`1.5 V`) or Lithium (`1.5 V`) AA cells. Always insert a matched set of fully charged NiMH cells of the same model.
4. **ALWAYS wire a fuse and power switch on the positive (`+`) battery wire.**
   - Even AA NiMH cells can deliver `> 5 A` into a dead short, which will melt thin `28–30 AWG` wires and burn your fingers or melt the 3D-printed PLA/PETG shell.
   - Solder a **`1.5 A` resettable polyfuse (`PTC`)** inline with the positive (`+`) wire right at the battery holder terminal, before the power slide switch.
5. **NEVER connect the battery boost converter output (`5 V`) directly to USB `5 V` (`VBUS`) without diode isolation.**
   - If you plug in a USB-C cable to flash firmware while the battery switch is ON and there is no blocking diode, the USB port and the boost converter will fight each other (back-feeding `5 V` into the battery boost module or into your PC's USB port).
   - Until a Schottky diode (`1N5819`) or ideal-diode OR-ing circuit ([ADR 0006](../decisions/0006-portable-power.md)) is soldered and tested, **always turn the battery switch OFF and remove one AA cell before plugging in USB-C**.

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
