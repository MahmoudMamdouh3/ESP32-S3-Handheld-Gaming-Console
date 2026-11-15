# Third-Party Notices, Provenance, and License Compatibility Matrix

- **Last Verified Commit:** `d8d4b5e` (`2026-11-15`)

> **Disclaimer:** This document is an engineering inventory of upstream license texts in the repository, not legal advice. BMO Handheld is an unofficial, non-commercial DIY fan hardware and software project and is unaffiliated with Cartoon Network / Warner Bros. Discovery, Nintendo, Sega, Atari, NEC, Coleco, SNK, Bandai, or id Software. No commercial game ROMs, BIOS files, or copyrighted artwork packs are shipped in this repository.

---

## 1. Build Profile License Summary & Redistribution Rules

First-party code written originally for BMO Handheld (`firmware/BmoGameboy/src/{app,hal,storage,ui,cores,tests}/`, `simulator/`, `hardware/`, `tools/`, `scripts/`, `tests/`) is dual-licensed under **`MIT OR GPL-2.0-or-later`**.

However, the 13 vendored third-party emulator engines under `firmware/BmoGameboy/src/third_party/` carry differing licenses — including **GNU GPL v2**, **GNU GPL v3**, and **three non-commercial-only clauses** (`snes9x`, `smsplus/z80.c`, and Musashi `m68k.h` inside `gwenesis`). Under GNU GPL v2 Section 6 and GNU GPL v3 Section 7, non-commercial restrictions are "further restrictions" that cannot be combined with GPL code in a redistributed binary.

| Build Profile (`python tasks.py build --profile <name>`) | Active Emulator Cores | Binary Redistribution Status | Reason |
| :--- | :--- | :--- | :--- |
| **`mit`** (`-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0`) | `Peanut-GB` (GB), `Walnut-CGB` (GBC), `agnes` (NES) | **REDISTRIBUTABLE (`MIT` / `Apache-2.0` / `BSD`)** | Links only permissive MIT emulator cores + Espressif/Adafruit libraries. Published by CI / GitHub Releases. |
| **`default`** (`BMO_EXPERIMENTAL_CORES=1, BMO_ENABLE_DOOM=1`) | All 15 platforms (13 upstream engines) | **PERSONAL-USE LOCAL BUILD ONLY (Do Not Redistribute Binary)** | Links `GPL-2.0` (`oswan`, `race`, `doom`, `stella2014`, `smsplus`, `huexpress`) + `GPL-3.0` (`gwenesis`) + Non-Commercial (`snes9x`, MAME `z80.c`, Musashi v3.32 `m68k.h`). |
| **`no-doom`** (`BMO_EXPERIMENTAL_CORES=1, BMO_ENABLE_DOOM=0`) | 14 platforms (excludes DOOM) | **PERSONAL-USE LOCAL BUILD ONLY (Do Not Redistribute Binary)** | Still links `GPL-2.0` + `GPL-3.0` + Non-Commercial cores. |

---

## 2. Complete Component License & Compatibility Matrix

| Component | Platform(s) | Path in Repo | Primary Upstream & Adaptation Source | SPDX / License Read from Files | Non-Commercial Clause? | Compatible with Combined GPL-2.0 Binary? | Evidence File |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **BMO First-Party Firmware & Tools** | Launcher, HAL, UI, Storage, Sim, CAD | `firmware/BmoGameboy/src/{app,hal,storage,ui,cores}/`, `tools/`, `scripts/` | Original to repository | `MIT OR GPL-2.0-or-later` | No | Yes | `LICENSE`, file headers |
| **Peanut-GB** | Game Boy (`ROM_GB`) | `src/third_party/peanut_gb/` | Mahyar Koshkouei (`deltabeard/Peanut-GB`, portions from SameBoy / Lior Halphon) | `MIT` | No | Yes (and `mit` profile) | `peanut_gb/peanut_gb.h:1-27`, `PROVENANCE.md` |
| **Walnut-CGB** | Game Boy Color (`ROM_GBC`) | `src/third_party/walnut_cgb/` | Mr. Paul (`Mr-PauI/Walnut-CGB`, derived from Peanut-GB / SameBoy) | `MIT` | No | Yes (and `mit` profile) | `walnut_cgb/walnut_cgb.h:1-30`, `PROVENANCE.md` |
| **agnes** | NES (`ROM_NES`) | `src/third_party/agnes/` | Krzysztof Gabis (`kgabis/agnes`) | `MIT` | No | Yes (and `mit` profile) | `agnes/agnes.h:1-24`, `PROVENANCE.md` |
| **FAKE-08 + z8lua** | PICO-8 (`ROM_PICO8`) | `src/third_party/fake08/` | Jonathaninger (`jtothebell/fake-08` @ `814991a2`); `z8lua` (`e6928578`); LodePNG; Zepto-8 | `MIT` AND `Zlib` AND `WTFPL` | No | Yes | `fake08/LICENSE.MD`, `PROVENANCE.md` |
| **Handy** | Atari Lynx (`ROM_LYNX`) | `src/third_party/handy/` | Keith Wilkins (`Handy`); adapted via `ducalex/retro-go` (`4ced1206`) | `Zlib` | No | Yes | `handy/license.txt`, `PROVENANCE.md` |
| **doomgeneric / Chocolate DOOM** | DOOM (`ROM_WAD`) | `src/third_party/doom/` | Id Software (1993–1996); Simon Howard (`chocolate-doom`); `ozkl/doomgeneric` | `GPL-2.0-or-later` | No | Yes | `doom/src/doomdef.h:1-21`, `PROVENANCE.md` |
| **Stella 2014** | Atari 2600 (`ROM_ATARI`) | `src/third_party/stella2014/` | Bradford W. Mott & Stephen Anthony (`libretro/stella2014-libretro` @ `7d1361e4`) | `GPL-2.0-or-later` | No | Yes | `stella2014/license.txt`, `PROVENANCE.md` |
| **HuExpress (PCE-Go)** | PC Engine (`ROM_PCE`) | `src/third_party/huexpress/` | Alexander Kalman / Zeograd (`HuExpress`); adapted via `ducalex/retro-go` (`4ced1206`) | `GPL-2.0-or-later` | No | Yes | `huexpress/COPYING`, `PROVENANCE.md` |
| **RACE** | Neo Geo Pocket (`ROM_NGP`) | `src/third_party/race/` | Judge / Flavor (`libretro/RACE` @ `c7810dd7`) | `GPL-2.0-or-later` | No | Yes | `race/LICENSE`, `race/license.txt`, `race/main.h:1-19` |
| **Oswan** | WonderSwan (`ROM_WSWAN`) | `src/third_party/oswan/` | Oswan authors (`3e4d7bb8`) via ESP32 adaptation (`74b47763`) | `GPL-2.0-only` (v2 text without "or later" header in C files) | No | Yes with `GPL-2.0`, **No** with `GPL-3.0` | `oswan/LICENSE`, `PROVENANCE.md` |
| **SMS Plus** | SMS, GG, SG-1000, ColecoVision | `src/third_party/smsplus/` | Charles MacDonald (`SMS Plus`) via `ducalex/retro-go` (`4ced1206`); embeds Juergen Buchmueller MAME Z80 v2.7 | `GPL-2.0-or-later` (SMS Plus) + **Custom Non-Commercial** (`z80.c`) | **YES** (`smsplus/z80.c:11-21`) | **NO** (MAME Z80 v2.7 non-commercial restriction conflicts with GPL redistribution) | `smsplus/COPYING`, `smsplus/z80.c:11-21` |
| **Gwenesis** | Sega Genesis (`ROM_GENESIS`) | `src/third_party/gwenesis/` | bzhxx (`bzhxx/gwenesis` @ `168e4666`); embeds Karl Stenerud Musashi v3.32 (`cpus/M68K/m68k.h`) | `GPL-3.0-or-later` (Gwenesis) + **Musashi v3.32 Non-Commercial** (`m68k.h`) | **YES** (`gwenesis/cpus/M68K/m68k.h:14-19`) | **NO** (`GPL-3.0` is incompatible with `GPL-2.0-only`, and Musashi v3.32 header is non-commercial) | `gwenesis/LICENSE`, `gwenesis/cpus/M68K/m68k.h:8-20` |
| **Snes9x (2005/2010)** | Super Nintendo (`ROM_SNES`) | `src/third_party/snes9x/` | Gary Henderson, Jerremy Koot, Snes9x Team (`libretro/snes9x2005` / `ndssfc`, adapted via `74b47763`) | **LicenseRef-Snes9x-NonCommercial** | **YES** (`snes9x/LICENSE:142-160`: "freeware for PERSONAL USE only") | **NO** (Explicitly forbids commercial use; incompatible with GPL binary combination) | `snes9x/LICENSE:142-160`, `PROVENANCE.md` |
| **Three.js (`r128`)** | Browser 3D Simulator | `simulator/three.min.js`, `OrbitControls.js` | Three.js Authors (2010–2021) | `MIT` | No | N/A (not linked into firmware) | SHA-256 verified in `THIRD_PARTY_NOTICES.md` |
| **Arduino-ESP32 (`3.3.11`)** | ESP32-S3 HAL & RTOS | External (`sketch.yaml`) | Espressif Systems | `Apache-2.0` AND `LGPL-2.1-or-later` AND `MIT` | No | Yes | `firmware/BmoGameboy/sketch.yaml` |
| **Adafruit Libraries** | BusIO (`1.17.4`), GFX (`1.12.6`), ST7789 (`1.11.0`) | External (`sketch.yaml`) | Adafruit Industries | `MIT` AND `BSD-2-Clause` | No | Yes | `firmware/BmoGameboy/sketch.yaml` |
