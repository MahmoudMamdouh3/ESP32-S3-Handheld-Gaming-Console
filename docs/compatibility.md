# Platform Compatibility & Core Status

The launcher defines `CONSOLES[]` with Favorites (`ROM_FAVORITES`, index `0`) plus fifteen platform entries mapping to fourteen C++ wrappers (`SmsEmu` handles both Master System and Game Gear).

**Status labels:**
- `third_party`: Genuine upstream open-source emulator core in [`firmware/BmoGameboy/src/third_party/`](../THIRD_PARTY_NOTICES.md), compiled cleanly into firmware by default (`BMO_EXPERIMENTAL_CORES=1`, `BMO_ENABLE_DOOM=1`; opt-out via `-DBMO_EXPERIMENTAL_CORES=0` or `-DBMO_ENABLE_DOOM=0`).
- `active`: Enabled in the default `arduino-cli` firmware profile.

**Evidence levels:** All targets below are `TARGET_COMPILED` across default and opt-out profiles, and `HOST_TESTED` via native WASI 50-frame harnesses (`tests/host/`) or WASM simulator builds where noted; physical hardware remains `NOT_TESTED` / zero `HARDWARE_TESTED` records.

## Master Platform Table

| `RomType` | Carousel | Wrapper | Source | Status | Extensions | Viewport | Target Period | Battery Save Gate | Evidence |
| :--- | ---: | ---: | :--- | :--- | :--- | ---: | ---: | :--- | :--- |
| `ROM_GB` | 1 | 1 | `src/cores/emu_peanut.cpp` | `active` (`peanut_gb`) | `.gb` | 240×216 @(40,12) | 16,742 µs | Cartridge type + RAM header | `TARGET_COMPILED` + `HOST_TESTED` (WASM) |
| `ROM_GBC` | 2 | 0 | `src/cores/emu_walnut.cpp` | `active` (`walnut_cgb`) | `.gbc` | 240×216 @(40,12) | 16,742 µs | Cartridge type + RAM header | `TARGET_COMPILED` |
| `ROM_NES` | 3 | 2 | `src/cores/emu_nes.cpp` | `active` (`agnes`) | `.nes` | 256×240 @(32,0) | 16,667 µs | iNES byte 6 battery bit (`0x02`) | `TARGET_COMPILED` + `HOST_TESTED` (WASI/WASM) |
| `ROM_WAD` | 4 | 3 | `src/cores/emu_doom.cpp` | `active` (`doomgeneric`, `BMO_ENABLE_DOOM=1`) | `.wad` | 320×200 @(0,20) | 28,571 µs | Save disabled in DOOM wrapper | `TARGET_COMPILED` |
| `ROM_SMS` | 5 | 4 | `src/cores/emu_sms.cpp` | `active` (`smsplus`, `BMO_EXPERIMENTAL_CORES=1`) | `.sms` | 256×192 @(32,24) | 16,667 µs | Cartridge SRAM (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_GG` | 6 | 5 | `src/cores/emu_sms.cpp` | `active` (`smsplus`, `BMO_EXPERIMENTAL_CORES=1`) | `.gg` | 160×144 @(80,48) | 16,667 µs | Cartridge SRAM (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_PCE` | 7 | 6 | `src/cores/emu_pce.cpp` | `active` (`huexpress`, `BMO_EXPERIMENTAL_CORES=1`) | `.pce` | 256×240 @(32,0) | 16,667 µs | PCE BRAM gated on `"HUBM"` header | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_ATARI` | 8 | 7 | `src/cores/emu_atari.cpp` | `active` (`stella2014`, `BMO_EXPERIMENTAL_CORES=1`) | `.a26` | 160×192 @(80,24) | 16,667 µs | Save states `.s01–.s05`; no `.sav` | `TARGET_COMPILED` + `HOST_TESTED` (WASI/WASM) |
| `ROM_PICO8` | 9 | 8 | `src/cores/emu_pico.cpp` | `active` (`fake08` + `z8lua`, `BMO_EXPERIMENTAL_CORES=1`) | `.p8` | 128×128 @(96,56) | 33,333 µs | Cartdata 256 B (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI Lua VM) |
| `ROM_GENESIS` | 10 | 9 | `src/cores/emu_genesis.cpp` | `active` (`gwenesis`, `BMO_EXPERIMENTAL_CORES=1`) | `.gen`, `.md`, `.smd` | 320×224 @(0,8) | 16,667 µs | Cartridge SRAM (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_SNES` | 11 | 10 | `src/cores/emu_snes.cpp` | `active` (`snes9x`, `BMO_EXPERIMENTAL_CORES=1`) | `.sfc`, `.smc` | 256×224 @(32,8) | 16,667 µs | LoROM/HiROM/ExHiROM checksum header | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_WSWAN` | 12 | 11 | `src/cores/emu_wswan.cpp` | `active` (`oswan`, `BMO_EXPERIMENTAL_CORES=1`) | `.ws`, `.wsc` | 224×144 @(48,48) | 16,667 µs | Cartridge SRAM (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_NGP` | 13 | 12 | `src/cores/emu_ngp.cpp` | `active` (`race`, `BMO_EXPERIMENTAL_CORES=1`) | `.ngp`, `.ngc` | 160×152 @(80,44) | 16,667 µs | Cartridge Flash (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_LYNX` | 14 | 13 | `src/cores/emu_lynx.cpp` | `active` (`handy`, `BMO_EXPERIMENTAL_CORES=1`) | `.lnx` | 160×102 @(80,69) | 16,667 µs | Cartridge EEPROM (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_COLEM` | 15 | 14 | `src/cores/emu_colem.cpp` | `active` (`smsplus`, `BMO_EXPERIMENTAL_CORES=1`) | `.col`, `.sg` | 256×192 @(32,24) | 16,667 µs | Save states `.s01–.s05`; no `.sav` | `TARGET_COMPILED` + `HOST_TESTED` (WASI/WASM) |

## Controls Inside Emulators

- `SELECT + START`: pause menu
- `SELECT + UP`: exit confirmation (press `A` twice within 3 seconds to confirm; supported battery RAM saves before teardown)
- `SELECT + A` / `SELECT + B`: quick-save / quick-load active slot (`1..5`)
- `SELECT + DOWN`: cycle DMG 4-shade palette (GB / GBC)
- `SELECT + LEFT`: toggle A/B turbo
- `SELECT + RIGHT` (hold): fast-forward (skips outer `FramePacing::targetFrameUs()` delay)
