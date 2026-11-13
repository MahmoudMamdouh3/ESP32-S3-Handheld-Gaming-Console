# Platform Compatibility & Core Status

The launcher defines `CONSOLES[]` with Favorites (`ROM_FAVORITES`, index `0`) plus fifteen platform entries mapping to fourteen C++ wrappers (`SmsEmu` handles both Master System and Game Gear).

**Status labels:**
- `stable`: Upstream/well-known open-source engine in `src/third_party/`; enabled by default.
- `beta`: Upstream third-party GPL-2.0 engine (`src/third_party/doom/`); disabled by default (`BMO_ENABLE_DOOM=0`) to keep public binaries MIT-compatible.
- `experimental`: First-party minimal C engine in `src/engines/` with synthetic host smoke tests (`tests/host/host_cores_runner.c`), awaiting full commercial/homebrew compatibility verification; disabled by default (`BMO_EXPERIMENTAL_CORES=0`) and opt-in with `-DBMO_EXPERIMENTAL_CORES=1`.

**Evidence levels:** All targets below are `TARGET_COMPILED` and `HOST_TESTED` via synthetic harnesses or WASM builds where noted; physical hardware remains `NOT_TESTED` / zero `HARDWARE_TESTED` records.

## Master Platform Table

| `RomType` | Carousel | Wrapper | Source | Status | Extensions | Viewport | Target Period | Battery Save Gate | Evidence |
| :--- | ---: | ---: | :--- | :--- | :--- | ---: | ---: | :--- | :--- |
| `ROM_GB` | 1 | 1 | `src/cores/emu_peanut.cpp` | `stable` | `.gb` | 240×216 @(40,12) | 16,742 µs | Cartridge type + RAM header | `TARGET_COMPILED` + `HOST_TESTED` (WASM) |
| `ROM_GBC` | 2 | 0 | `src/cores/emu_walnut.cpp` | `stable` | `.gbc` | 240×216 @(40,12) | 16,742 µs | Cartridge type + RAM header | `TARGET_COMPILED` |
| `ROM_NES` | 3 | 2 | `src/cores/emu_nes.cpp` | `stable` | `.nes` | 256×240 @(32,0) | 16,667 µs | iNES byte 6 battery bit (`0x02`) | `TARGET_COMPILED` + `HOST_TESTED` (C/WASM) |
| `ROM_WAD` | 4 | 3 | `src/cores/emu_doom.cpp` | `beta` | `.wad` | 320×200 @(0,20) | 28,571 µs | Save disabled (opt-in `BMO_ENABLE_DOOM`) | `TARGET_COMPILED` |
| `ROM_SMS` | 5 | 4 | `src/cores/emu_sms.cpp` | `experimental` | `.sms` | 256×192 @(32,24) | 16,667 µs | Save states `.s01–.s05`; no `.sav` | `TARGET_COMPILED` + `HOST_TESTED` (synthetic C) |
| `ROM_GG` | 6 | 5 | `src/cores/emu_sms.cpp` | `experimental` | `.gg` | 160×144 @(80,48) | 16,667 µs | Save states `.s01–.s05`; no `.sav` | `TARGET_COMPILED` + `HOST_TESTED` (synthetic C) |
| `ROM_PCE` | 7 | 6 | `src/cores/emu_pce.cpp` | `experimental` | `.pce` | 256×240 @(32,0) | 16,667 µs | PCE BRAM (unfiltered; `HUBM` gate pending) | `TARGET_COMPILED` + `HOST_TESTED` (synthetic C) |
| `ROM_ATARI` | 8 | 7 | `src/cores/emu_atari.cpp` | `experimental` | `.a26` | 160×192 @(80,24) | 16,667 µs | Save states `.s01–.s05`; no `.sav` | `TARGET_COMPILED` + `HOST_TESTED` (synthetic C/WASM) |
| `ROM_PICO8` | 9 | 8 | `src/cores/emu_pico.cpp` | `experimental` | `.p8` | 128×128 @(96,56) | 33,333 µs | Custom text subset (no Lua VM) | `TARGET_COMPILED` + `HOST_TESTED` (synthetic C) |
| `ROM_GENESIS` | 10 | 9 | `src/cores/emu_genesis.cpp` | `experimental` | `.gen`, `.md`, `.smd` | 320×224 @(0,8) | 16,667 µs | Save states `.s01–.s05`; no `.sav` | `TARGET_COMPILED` + `HOST_TESTED` (synthetic C) |
| `ROM_SNES` | 11 | 10 | `src/cores/emu_snes.cpp` | `experimental` | `.sfc`, `.smc` | 256×224 @(32,8) | 16,667 µs | LoROM SRAM header (HiROM pending) | `TARGET_COMPILED` + `HOST_TESTED` (synthetic C) |
| `ROM_WSWAN` | 12 | 11 | `src/cores/emu_wswan.cpp` | `experimental` | `.ws`, `.wsc` | 224×144 @(48,48) | 16,667 µs | Save states `.s01–.s05`; no `.sav` | `TARGET_COMPILED` + `HOST_TESTED` (synthetic C) |
| `ROM_NGP` | 13 | 12 | `src/cores/emu_ngp.cpp` | `experimental` | `.ngp`, `.ngc` | 160×152 @(80,44) | 16,667 µs | Save states `.s01–.s05`; no `.sav` | `TARGET_COMPILED` + `HOST_TESTED` (synthetic C) |
| `ROM_LYNX` | 14 | 13 | `src/cores/emu_lynx.cpp` | `experimental` | `.lnx` | 160×102 @(80,69) | 16,667 µs | Save states `.s01–.s05`; no `.sav` | `TARGET_COMPILED` + `HOST_TESTED` (synthetic C) |
| `ROM_COLEM` | 15 | 14 | `src/cores/emu_colem.cpp` | `experimental` | `.col`, `.sg` | 256×192 @(32,24) | 16,667 µs | Save states `.s01–.s05`; no `.sav` | `TARGET_COMPILED` + `HOST_TESTED` (synthetic C/WASM) |

## Controls Inside Emulators

- `SELECT + START`: pause menu
- `SELECT + UP`: exit confirmation (press `A` twice within 3 seconds to confirm; supported battery RAM saves before teardown)
- `SELECT + A` / `SELECT + B`: quick-save / quick-load active slot (`1..5`)
- `SELECT + DOWN`: cycle DMG 4-shade palette (GB / GBC)
- `SELECT + LEFT`: toggle A/B turbo
- `SELECT + RIGHT` (hold): fast-forward (skips outer `FramePacing::targetFrameUs()` delay)
