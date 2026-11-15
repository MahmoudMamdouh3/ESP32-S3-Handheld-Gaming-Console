# Owner Decisions Register (`docs/owner-decisions.md`)

This document collects every decision from the tech-lead audit (`techlead/audit-2`) that requires your approval as project owner. **None of these changes have been forced on your hardware configuration yet** — all Hardware Hard Stops in `config.h`, `partitions.csv`, and `sketch.yaml` remain untouched until you check a box below and tell the agent to apply it.

---

## Decision 1: Rechargeable AA Battery Architecture (`2×AA` vs `3×AA` NiMH) — [ADR 0006](decisions/0006-portable-power.md)

- **Why you need to decide:** Rechargeable NiMH AA cells (`1.2 V` nominal, `1.0 V` empty, `1.42 V` full) need a step-up ("boost") regulator to provide `5.0 V` to the `ESP32-S3-DevKitC-1` `5V`/`VIN` pin.
- **Options:**
  - [ ] **(Recommended) Option 1A — `2×AA NiMH` (`2.4 V` nominal) + Pololu `5 V` Step-Up (`U3V40F5` or `S7V7F5`) + `1.5 A` Polyfuse + Slide Switch:**
    - *Pros:* Lighter (`~52 g` vs `~78 g`), narrower (`5.80 × 3.20 × 1.65 cm`), fits easily in the Lower Y Zone (`y = 8.10..11.30 cm`) behind the buttons with `0.09 cm` Z-clearance inside the `3.20 cm` shell, ~2.8 hours runtime on 2000 mAh Eneloop cells.
    - *Cons:* Draws higher current from the batteries (`~0.88 A` when nearly empty), so the boost converter must be rated for `>= 1.5 A` switch current (do not use cheap `MT3608` boards, which need `>= 2.0 V` just to start).
  - [ ] **Option 1B — `3×AA NiMH` (`3.6 V` nominal) + Pololu `5 V` Step-Up (`U3V40F5`):**
    - *Pros:* ~4.4 hours runtime (`6.6 Wh`), lower battery current (`~0.59 A` max).
    - *Cons:* `50%` heavier and wider (`5.80 × 4.80 × 1.65 cm`), tighter fit around the D-pad/ABXY solder pins.

---

## Decision 2: Default Display SPI Speed (`80 MHz` vs `40 MHz`) — [ADR 0014](decisions/0014-display-spi-clock.md)

- **Why you need to decide:** Currently `config.h` defaults to `TFT_SPI_HZ = 80000000` (`80 MHz`). The Sitronix `ST7789VW` datasheet guarantees writes up to `62.5 MHz` (`16 ns` cycle), and hand-soldered perfboard wires (`> 5 cm`) can ring at `80 MHz`. However, ESP32-S3 only supports `80 MHz` or `40 MHz` (no `60 MHz` step), and at `40 MHz` a `240×216` Game Boy frame takes `20.74 ms` on the wire (`~48 FPS` ceiling).
- **Options:**
  - [ ] **(Recommended) Option 2A — Change default `TFT_SPI_HZ` to `40 MHz` for initial hardware bring-up, with `80 MHz` selectable in the on-device Diagnostics menu:**
    - *Pros:* Within the ST7789VW datasheet write specification (`<= 62.5 MHz`) and far more reliable on hand-soldered perfboard wires on first power-on.
    - *Cons:* Caps full-frame wire rate at `~48 FPS` until you toggle `80 MHz` in Diagnostics (`START` -> `A` -> `LEFT/RIGHT`).
  - [ ] **Option 2B — Keep `80 MHz` as default and use the `SELECT`/`B` boot Safe Mode button if the screen glitches:**
    - *Pros:* Full 60 FPS wire headroom (`10.37 ms/frame` for GB/GBC) if your short wires (`<= 6.0 cm`) + `33 Ω` series resistor handle `80 MHz`.
    - *Cons:* First boot could show a corrupted screen until you hold `SELECT` or `B` at power-on.

---

## Decision 3: Physical Enclosure & Perfboard Dimensions — [`hardware/layout.json`](../hardware/layout.json)

- **Why you need to decide:** Your reference layout uses a full **`31 × 46` hole perfboard (`7.87 × 11.68 cm`)** inside an **`8.80 × 12.40 × 3.20 cm`** shell (`0` Z-axis collisions when the `ESP32-S3-DevKitC-1` sits in the upper half `y = 1.25..7.58 cm` behind the screen and the `2×AA` holder sits in the lower half `y = 8.10..11.30 cm` behind the buttons). Meanwhile, `generate_bmo_enclosure.py` still has an older compact placeholder (`8.20 × 12.40 × 2.60 cm` with a `5.00 × 7.00 cm` board and a `5.40 cm` battery bay that is `0.40 cm` too short for a `5.80 cm` AA holder).
- **Options:**
  - [ ] **(Recommended) Option 3A — Update `generate_bmo_enclosure.py` and `simulator/bmo_enclosure_meshes.js` to match `hardware/layout.json` (`8.80 × 12.40 × 3.20 cm`, `31 × 46` perfboard, `0.60 × 0.60 cm` tactile switches):**
    - *Pros:* Fits a real `5.80 cm` AA battery holder, a full `31 × 46` hole perfboard (`7.87 × 11.68 cm`), and leaves `0.814 cm` between ABXY switches for easy hand soldering.
    - *Cons:* Shell is `0.60 cm` wider (`8.80 cm` vs `8.20 cm`) and `0.60 cm` thicker (`3.20 cm` vs `2.60 cm`).
  - [ ] **Option 3B — Wait until you measure your soldered board with calipers (`docs/hardware_measurements.md`) before regenerating the 3D printable STL files.**

---

## Decision 4: Emulator Core Licensing & Binary Distribution — [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) & [ADR 0008](decisions/0008-rom-policy-and-licensing.md)

- **Why you need to decide:** Three vendored emulator cores in `firmware/BmoGameboy/src/third_party/` contain explicit **Non-Commercial** clauses (`Snes9x`, MAME `z80.c` inside `SMS Plus`/`ColEm`, and `Musashi v3.32` inside `GPL-3.0` `Gwenesis`). Under GPL v2 Section 6 and GPL v3 Section 7, you cannot legally distribute a combined binary that links GPL code with non-commercial-restricted code. (We already fixed GitHub Actions so CI/Releases only upload the clean `mit` binary bundle.)
- **Options:**
  - [ ] **(Recommended) Option 4A — Keep all 15 platforms in source code for your personal local builds (`default` profile), and publish only the `mit` binary (`GB`, `GBC`, `NES`) in GitHub Releases:**
    - *Pros:* You keep every console for your own handheld, and public binary downloads stay 100% license-compliant (`MIT`).
    - *Cons:* Public release `.zip` files only include `GB`, `GBC`, and `NES` unless users compile locally.
  - [ ] **Option 4B — Make the GitHub repository Private (personal hardware build only).**
  - [ ] **Option 4C — Quarantine or remove the 3 non-commercial cores (`snes9x`, `smsplus`, `gwenesis`) so a `GPL-2.0` binary can be distributed publicly.**

---

## Decision 5: Flash Partition Table & Crash Coredump (`partitions.csv`) — [ADR 0013](decisions/0013-partition-layout.md)

- **Why you need to decide:** `firmware/BmoGameboy/partitions.csv` leaves `64 KiB` (`0xFF0000..0x1000000`) at the end of the 16 MB Flash chip unallocated and has no `coredump` partition to store crash backtraces in flash.
- **Options:**
  - [ ] **(Recommended) Option 5A — Add `coredump, data, coredump, 0xFF0000, 0x10000,` (`64 KiB`) to `partitions.csv`:**
    - *Pros:* Zero impact on `nvs`, `app0` (`8 MiB`), or `ffat` (`~7.9 MiB`); saves full crash backtraces to flash if a core panics on battery power.
    - *Cons:* Requires a one-time full USB-C flash when applied.
  - [ ] **Option 5B — Leave `partitions.csv` untouched.**

---

## Decision 6: FRAM Removal & Optional PWM Display Backlight Pin — [ADR 0007](decisions/0007-fram-wiring.md)

- **Why you need to decide:** `config.h` currently reserves `I2C_SDA = 43` and `I2C_SCL = 44` for an unfitted FRAM chip, which collides with the ESP32-S3's USB-serial `U0TXD`/`U0RXD` pins (`GPIO 43/44`). Meanwhile, display backlight `BLK` is wired straight to `3V3` (100% brightness).
- **Options:**
  - [ ] **(Recommended) Option 6A — Remove the unused `I2C_SDA`/`I2C_SCL` (`43/44`) defines and `fram_save.*`, and reserve free `GPIO 2` (or `GPIO 41`) for optional transistor-driven PWM backlight dimming in a future revision.**
  - [ ] **Option 6B — Leave `config.h` pin defines unchanged (`FEATURE_FRAM=0` already prevents `fram_save.cpp` from compiling).**

---

## Decision 7: Project / Sketch Naming & Trademark Hygiene (`L4`)

- **Why you need to decide:** The Arduino sketch is named `firmware/BmoGameboy/BmoGameboy.ino` ("BMO" is a Cartoon Network trademark; "Game Boy" is a Nintendo trademark).
- **Options:**
  - [ ] **(Recommended) Option 7A — Keep `firmware/BmoGameboy/BmoGameboy.ino` for now so no build scripts or paths break, and keep the clear non-commercial fan-project disclaimer in `README.md`.**
  - [ ] **Option 7B — Rename the sketch folder and `.ino` file to `firmware/BmoHandheld/BmoHandheld.ino` across the repository.**

---

## Decision 8: Sidecar Tools, Weak Tests & Feature-Freeze Priorities ([AI & Tooling Review](audits/ai-and-tooling-review.md))

- **Why you need to decide:** This repository holds several large optional tools built before the first physical board powered on (a browser simulator, 3D teardown viewer, custom static analyzer `tools/guardian/`, `63` Python string-matching/toy core checks, on-device `unit_tests.cpp`, and the Console Museum info screen). Keeping things lean reduces future confusion and AI maintenance cost.
- **Options:**
  - [ ] **(Recommended) Option 8A — Freeze the browser simulator / 3D teardown as read-only preview tools right now, prioritize physical bring-up, and after your board boots cleanly, remove `unit_tests.cpp` + the `7` low-signal source-regex Python test files while keeping real C++/WASI tests (`CTest` + `test_native_cores_execution.py`).**
  - [ ] **Option 8B — Remove optional sidecar tooling (`simulator/bmo_internals_3d.js`, `unit_tests.cpp`, and the `7` low-signal Python test files) immediately.**
  - [ ] **Option 8C — Keep everything as-is.**

---

## Decision 9: First-Party Code License & SPDX Header Cleanup ([Third-Party Notices](../THIRD_PARTY_NOTICES.md))

- **Why you need to decide:** An earlier automated commit (`addcc07`) added `SPDX-License-Identifier: MIT OR GPL-2.0-or-later` to `124` repository files (`118` survive + `7` added later = `125` tracked first-party code files; baseline `131` repo grep hits also included `2` upstream `agnes` MIT files, `1` vendored `three.min.js`, and `3` doc/ignore lines) without your sign-off. Meanwhile, root [`LICENSE`](../LICENSE) is **GNU GPL v2 (`GPL-2.0`)**, and **`19` of those first-party files** wrap or couple directly to GPL-2.0-only (`oswan`), GPL-2.0-or-later (`doom`, `stella2014`, `huexpress`, `race`, `smsplus`), GPL-3.0-or-later (`gwenesis`), or Non-Commercial (`snes9x`, MAME `z80.c`, Musashi `m68k.h`) upstream code — so claiming `MIT OR ...` on those `19` coupled files is legally inconsistent and must not stay:
  - **Non-Commercial / Mixed (`6` files):** `emu_snes.{cpp,h}` (`LicenseRef-Snes9x-NonCommercial`), `emu_sms.{cpp,h}` & `emu_colem.{cpp,h}` (`GPL-2.0-or-later` + MAME Z80 Non-Commercial), plus `emu_genesis.{cpp,h}` (`GPL-3.0-or-later` + Musashi v3.32 Non-Commercial — also incompatible with `GPL-2.0-only`).
  - **`GPL-2.0-only` (`2` files):** `emu_wswan.{cpp,h}` (mirrors `oswan` `WS_*` layout & callbacks).
  - **`GPL-2.0-or-later` (`11` files):** `emu_atari.{cpp,h}`, `emu_doom.{cpp,h}`, `emu_ngp.{cpp,h}` (`RACE` globals/`0x8100` offsets), `emu_pce.{cpp,h}`, and `ui/display_worker.cpp` (declares `doomgeneric`'s `extern "C" struct color colors[256]`).
  - *(Of the remaining `106` first-party files: `emu_lynx.{cpp,h}` wrap permissive `Zlib` `Handy`; `emu_pico.{cpp,h}` wrap `MIT AND Zlib AND WTFPL` `fake-08`; `emu_{nes,peanut,walnut}.{cpp,h}` wrap `MIT` engines; `core_registry.h` conditionally includes core wrappers under `#if BMO_EXPERIMENTAL_CORES`; and `95` files are pure original BMO firmware/CAD/scripts/tests.)*
- **Options:**
  - [ ] **(Recommended) Option 9A — Accept the prepared cleanup on unmerged branch `license/spdx-review`: dual-license purely original first-party files (`MIT OR GPL-2.0-or-later` so your own UI/HAL/storage/tools stay permissive and shareable) while stamping the `19` core-coupled files (plus `Zlib` on `emu_lynx.{cpp,h}`) with their exact upstream-matching licenses, and update root `LICENSE` notices accordingly.**
  - [ ] **Option 9B — Keep everything strictly under `GPL-2.0` (`LICENSE`) and merge only the revert of `addcc07` from `license/spdx-review` (zero `MIT` tags until you later decide to share pure modules separately).**
  - [ ] **Option 9C — Wait to publish/share binaries (`mit` profile builds cleanly as MIT if `core_registry.h` / wrappers keep clean boundaries) and choose `9A` vs `9B` after physical bring-up.**
- **Staged Work (NOT Merged into `techlead/audit-2`):** Branch `license/spdx-review` contains both (1) a clean revert of `addcc07` and (2) the corrected SPDX split separating purely original first-party files from the `19` upstream-coupled files, waiting for your approval to merge.
