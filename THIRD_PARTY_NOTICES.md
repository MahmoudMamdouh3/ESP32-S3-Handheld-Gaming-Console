# Third-Party Notices and Licensing

BMO Handheld distributes its repository and default full-emulator firmware under the [GNU General Public License v2.0](LICENSE) (`GPL-2.0-or-later` compatibility for combined distribution, with specific upstream terms noted below for `gwenesis` GPLv3 and non-commercial clauses in `snes9x` / MAME `z80.c`). First-party standalone tooling and core HAL/UI modules written original to BMO Handheld remain permissively reusable under MIT/GPLv2-compatible terms when stripped of third-party copyleft cores (via `-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0`).

No commercial game ROMs or copyrighted artwork packs are shipped in this repository. BMO Handheld is an unofficial DIY fan hardware and software project and is unaffiliated with Cartoon Network, Nintendo, Sega, Atari, NEC, Coleco, SNK, Bandai, or id Software.

## Default Firmware Licensing (`BMO_EXPERIMENTAL_CORES=1`, `BMO_ENABLE_DOOM=1`)

- **Default firmware build (`config.h`):** Enables all 15 console platforms and DOOM (`BMO_EXPERIMENTAL_CORES=1`, `BMO_ENABLE_DOOM=1`) using genuine upstream open-source emulators under `firmware/BmoGameboy/src/third_party/`. Distributing compiled default binaries subjects the combined work to the copyleft and non-commercial conditions of the bundled upstream engines below (specifically `GPL-2.0`, `GPL-3.0`, and Snes9x / MAME Z80 non-commercial restrictions).
- **Minimal MIT core subset (`-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0`):** Excludes all GPL/non-commercial experimental cores and DOOM at compile time, leaving only MIT-licensed cores (`Peanut-GB`, `Walnut-CGB`, `agnes`) plus standard Espressif/Arduino/Adafruit SDK dependencies.

## Bundled Third-Party Firmware Emulator Cores (`firmware/BmoGameboy/src/third_party/`)

| Component | Platform(s) | Location in Repo | Upstream Origin & Commit | License |
| :--- | :--- | :--- | :--- | :--- |
| **Peanut-GB** | Game Boy (`ROM_GB`) | `firmware/BmoGameboy/src/third_party/peanut_gb/` | Mahyar Koshkouei (portions from SameBoy / Lior Halphon) | MIT |
| **Walnut-CGB** | Game Boy / GBC (`ROM_GBC`) | `firmware/BmoGameboy/src/third_party/walnut_cgb/` | Mr. Paul (`https://github.com/Mr-PauI`, forked from Peanut-GB / SameBoy) | MIT |
| **agnes** | NES (`ROM_NES`) | `firmware/BmoGameboy/src/third_party/agnes/` | Krzysztof Gabis (`https://github.com/kgabis/agnes`) | MIT |
| **doomgeneric / Chocolate DOOM** | DOOM (`ROM_WAD`) | `firmware/BmoGameboy/src/third_party/doom/` | Id Software, Inc. (1993–1996); Simon Howard (2005–2014); doomgeneric contributors | GPL-2.0-or-later |
| **SMS Plus** | Master System, Game Gear, SG-1000, ColecoVision | `firmware/BmoGameboy/src/third_party/smsplus/` | `https://github.com/geo-tp/Cardputer-Game-Station-Emulators.git` (`74b47763c3f2ccbaed6505b83ef0ef37c2deac82`) & `https://github.com/ducalex/retro-go.git` (`4ced120669750ca7228fd0414211430c1d923166`) | GPL-2.0-or-later (`COPYING`; `z80.c` includes MAME Z80 v2.7 Juergen Buchmueller non-commercial header notice) |
| **HuExpress (PCE-Go)** | PC Engine / TurboGrafx-16 (`ROM_PCE`) | `firmware/BmoGameboy/src/third_party/huexpress/` | `https://github.com/geo-tp/Cardputer-Game-Station-Emulators.git` (`74b47763c3f2ccbaed6505b83ef0ef37c2deac82`) & `https://github.com/ducalex/retro-go.git` (`4ced120669750ca7228fd0414211430c1d923166`) | GPL-2.0-or-later (`COPYING`) |
| **Handy** | Atari Lynx (`ROM_LYNX`) | `firmware/BmoGameboy/src/third_party/handy/` | `https://github.com/geo-tp/Cardputer-Game-Station-Emulators.git` (`74b47763c3f2ccbaed6505b83ef0ef37c2deac82`) & `https://github.com/ducalex/retro-go.git` (`4ced120669750ca7228fd0414211430c1d923166`) | zlib License (`license.txt`) |
| **Snes9x** | Super Nintendo (`ROM_SNES`) | `firmware/BmoGameboy/src/third_party/snes9x/` | `https://github.com/Automated-Test/Cardputer-Emulators` (`74b47763c3f2ccbaed6505b83ef0ef37c2deac82`, `src/snes/snes9x/`) | Snes9x Non-Commercial License (`LICENSE`) |
| **Gwenesis** | Sega Genesis / Mega Drive (`ROM_GENESIS`) | `firmware/BmoGameboy/src/third_party/gwenesis/` | `https://github.com/bzhxx/gwenesis` (`168e46661a086a0b793a9ee127a206604ebe7df1`) via `Cardputer-Emulators` (`74b47763c3f2ccbaed6505b83ef0ef37c2deac82`) | GNU GPL v3 (`LICENSE`) |
| **Oswan** | WonderSwan / Color (`ROM_WSWAN`) | `firmware/BmoGameboy/src/third_party/oswan/` | Oswan (`3e4d7bb8271ad390c1d5623179fb19e2cca1acb32`) via `Cardputer-Emulators` (`74b47763c3f2ccbaed6505b83ef0ef37c2deac82`) | GNU GPL v2 (`LICENSE`) |
| **RACE** | Neo Geo Pocket / Color (`ROM_NGP`) | `firmware/BmoGameboy/src/third_party/race/` | `libretro/RACE` (`c7810dd7f172827bfa2004813bc000b13786636b`) / `Cardputer-Emulators` (`74b47763c3f2ccbaed6505b83ef0ef37c2deac82`) | GNU GPL v2 (`LICENSE` / `license.txt`) |
| **Stella 2014** | Atari 2600 (`ROM_ATARI`) | `firmware/BmoGameboy/src/third_party/stella2014/` | `https://github.com/libretro/stella2014-libretro` (`7d1361e407e63f29e52892655069e5fb4096e691`) | GPL-2.0-or-later (`license.txt`) |
| **FAKE-08 + z8lua** | PICO-8 (`ROM_PICO`) | `firmware/BmoGameboy/src/third_party/fake08/` | `https://github.com/jtothebell/fake-08` (`814991a2571ad3970e3b6cef48f3b148aa1c27b9`, `MIT`); `z8lua` (`e6928578d46b61fd5ea30cfcf547e855a30a0553`, `MIT`); Eris persistence (`MIT`); Zepto-8 charmap (`WTFPL2`); LodePNG (`zlib`) | MIT / zlib / WTFPL2 (`LICENSE.MD`) |

Per-core adaptation notes and pinned commit SHAs are also maintained in each engine directory's `PROVENANCE.md` file alongside upstream license texts.

## Simulator & Third-Party Libraries

| Component | Location in Repo | Upstream / Copyright | License | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Three.js (`three.min.js`)** | `simulator/three.min.js` | Three.js Authors (Revision `128`, 2010–2021) | MIT | SHA-256: `9274BBCEC8D96168626C732B5D31C775AA8CFB7EAA0599BEC0C175908A2C1CE2` |
| **Three.js (`OrbitControls.js`)** | `simulator/OrbitControls.js` | Three.js Authors (Revision `128`) | MIT | SHA-256: `02BB4ADE710F3E607329E37A21F098BC3AC70EB6E33DAF8A65E79F4DB785E7B2` |

## External Build Dependencies (Installed via Arduino CLI)

Configured in [`firmware/BmoGameboy/sketch.yaml`](firmware/BmoGameboy/sketch.yaml):

- **Espressif `esp32:esp32` core (`3.3.11`)** — Apache-2.0 / LGPL-2.1 / MIT (ESP-IDF & Arduino-ESP32 components)
- **Adafruit BusIO (`1.17.4`)** — MIT
- **Adafruit GFX Library (`1.12.6`)** — BSD-2-Clause / BSD-3-Clause
- **Adafruit ST7735 and ST7789 Library (`1.11.0`)** — MIT
