# Third-Party Notices and Licensing

First-party source code in this repository is licensed under the [MIT License](LICENSE). No commercial game ROMs or copyrighted artwork packs are shipped in this repository.

BMO Handheld is an unofficial DIY fan hardware and software project and is unaffiliated with Cartoon Network, Nintendo, Sega, Sony, Atari, NEC, Coleco, or Bandai.

## Optional GPL-2.0-or-later Build Flag (`BMO_ENABLE_DOOM`)

The optional DOOM engine under `firmware/BmoGameboy/src/third_party/doom/` is derived from id Software / Chocolate Doom (`doomgeneric`) and is licensed under the **GNU General Public License v2.0 or later (GPL-2.0-or-later)**.

- **Default public build (`BMO_ENABLE_DOOM=0`):** The DOOM wrapper (`src/cores/emu_doom.cpp`), frame streamer (`DisplayEmu::streamDoomFrame`), and DOOM engine calls are disabled at compile time so unreferenced DOOM object code is excluded from the linked firmware binary. Default firmware binaries built with `--profile esp32s3` contain MIT-licensed code plus standard Espressif/Arduino/Adafruit SDK dependencies.
- **DOOM-enabled build (`-DBMO_ENABLE_DOOM=1`):** Compiling and linking with `BMO_ENABLE_DOOM=1` incorporates GPL-2.0-or-later code, which makes the resulting firmware binary as a whole licensed under **GPL-2.0-or-later**. Any redistribution of such a binary must comply with the GPLv2/GPLv3 terms (including corresponding source availability).

## Bundled Third-Party Components

| Component | Location in Repo | Upstream / Copyright | License | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Peanut-GB** | `firmware/BmoGameboy/src/third_party/peanut_gb/peanut_gb.h` | Mahyar Koshkouei (portions from SameBoy, Lior Halphon) | MIT | Original Game Boy emulator header |
| **Walnut-CGB** | `firmware/BmoGameboy/src/third_party/walnut_cgb/walnut_cgb.h` | Mr. Paul (forked from Peanut-GB / SameBoy) | MIT | Game Boy / Game Boy Color emulator header |
| **agnes** | `firmware/BmoGameboy/src/third_party/agnes/` | Krzysztof Gabis (`https://github.com/kgabis/agnes`) | MIT | NES emulator (`agnes.h` / `agnes.c`) |
| **doomgeneric / Chocolate DOOM** | `firmware/BmoGameboy/src/third_party/doom/src/` | Id Software, Inc.; Simon Howard; doomgeneric contributors | GPL-2.0-or-later | Disabled unless `-DBMO_ENABLE_DOOM=1` is passed |
| **Three.js (`three.min.js`)** | `simulator/three.min.js` | Three.js Authors (Revision `128`, 2010–2021) | MIT | SHA-256: `9274BBCEC8D96168626C732B5D31C775AA8CFB7EAA0599BEC0C175908A2C1CE2` |
| **Three.js (`OrbitControls.js`)** | `simulator/OrbitControls.js` | Three.js Authors (Revision `128`) | MIT | SHA-256: `02BB4ADE710F3E607329E37A21F098BC3AC70EB6E33DAF8A65E79F4DB785E7B2` |

## External Build Dependencies (Installed via Arduino CLI)

Configured in `firmware/BmoGameboy/sketch.yaml`:

- **Espressif `esp32:esp32` core (`3.3.11`)** — Apache-2.0 / LGPL-2.1 / MIT (ESP-IDF & Arduino-ESP32 components)
- **Adafruit BusIO (`1.17.4`)** — MIT
- **Adafruit GFX Library (`1.12.6`)** — BSD-2-Clause / BSD-3-Clause
- **Adafruit ST7735 and ST7789 Library (`1.11.0`)** — MIT
