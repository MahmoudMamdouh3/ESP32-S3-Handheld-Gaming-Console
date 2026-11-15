# Platform Compatibility, SPI Bus Budget & Core Status

The launcher defines `CONSOLES[]` with Favorites (`ROM_FAVORITES`, index `0`) plus fifteen platform entries mapping to fourteen C++ emulator wrappers (`SmsEmu` handles both Sega Master System and Game Gear).

## 1. Evidence & Maturity Tiers

- **Evidence Levels:** Every platform wrapper below is `TARGET_COMPILED` across all applicable build profiles (`default`, `no-doom`, `mit`) and `HOST_TESTED` via native C/C++ or WASI/WASM harnesses (`tests/`). **Physical device records (`HARDWARE_TESTED`): `0` recorded yet (`docs/hardware_measurements.md`).**
- **Tier Classification:**
  - **Primary (`MIT` / Permissive, Core 0 Async Display Streaming):** Game Boy (`Peanut-GB`), Game Boy Color (`Walnut-CGB`), and NES (`agnes`). Included in all build profiles (`default`, `no-doom`, and redistributable `mit`).
  - **Experimental (`BMO_EXPERIMENTAL_CORES=1` or `BMO_ENABLE_DOOM=1`, `NOT_TESTED` on physical hardware):** Included in personal local builds (`default` / `no-doom`). Several 16-bit and handheld emulators (`SNES`, `Genesis`, `PC Engine`, `WonderSwan`, `Neo Geo Pocket`, `Atari Lynx`) push synchronous full-frame updates over SPI and have not yet been benchmarked on the physical ESP32-S3 perfboard.

---

## 2. Master Platform Compatibility & Bus Budget Table

Pure SPI wire transfer times (`Wire @ 80 / 40 MHz`) are computed by `python -m tools.guardian bus-calc` (`t = bits / f_SPI`). Compare against each platform's `Target Period`: when `Wire @ 40 MHz` exceeds or approaches the target frame period, full-rate 60 FPS requires either `80 MHz` SPI, Core 0 async overlap, or frame-skipping ([ADR 0014](decisions/0014-display-spi-clock.md)).

| `RomType` | Carousel | Wrapper | Source | Tier & License | Extensions | Viewport | Target Period | Wire @ `80 / 40 MHz` | Battery Save Gate | Evidence |
| :--- | ---: | ---: | :--- | :--- | :--- | :--- | ---: | :--- | :--- | :--- |
| `ROM_GB` | 1 | 1 | `src/cores/emu_peanut.cpp` | **Primary** (`peanut_gb`, `MIT`) | `.gb` | 240×216 @(40,12) | 16,742 µs | `10.37 / 20.74 ms` (Core 0 async) | Cartridge type + RAM header | `TARGET_COMPILED` + `HOST_TESTED` (WASM) |
| `ROM_GBC` | 2 | 0 | `src/cores/emu_walnut.cpp` | **Primary** (`walnut_cgb`, `MIT`) | `.gbc` | 240×216 @(40,12) | 16,742 µs | `10.37 / 20.74 ms` (Core 0 async) | Cartridge type + RAM header | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_NES` | 3 | 2 | `src/cores/emu_nes.cpp` | **Primary** (`agnes`, `MIT`) | `.nes` | 256×240 @(32,0) | 16,667 µs | `12.29 / 24.58 ms` (Core 0 async) | iNES byte 6 battery bit (`0x02`) | `TARGET_COMPILED` + `HOST_TESTED` (WASI/WASM) |
| `ROM_WAD` | 4 | 3 | `src/cores/emu_doom.cpp` | **Experimental** (`doomgeneric`, `GPL-2.0`) | `.wad` | 320×200 @(0,20) | 28,571 µs | `12.80 / 25.60 ms` (Core 0 async) | Save disabled in DOOM wrapper | `TARGET_COMPILED` (`NOT_TESTED` on HW) |
| `ROM_SMS` | 5 | 4 | `src/cores/emu_sms.cpp` | **Experimental** (`smsplus`, `GPL-2.0` + Non-Comm Z80) | `.sms` | 256×192 @(32,24) | 16,667 µs | `9.83 / 19.66 ms` (Sync scanline) | Cartridge SRAM (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_GG` | 6 | 5 | `src/cores/emu_sms.cpp` | **Experimental** (`smsplus`, `GPL-2.0` + Non-Comm Z80) | `.gg` | 160×144 @(80,48) | 16,667 µs | `4.61 / 9.22 ms` (Sync scanline) | Cartridge SRAM (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_PCE` | 7 | 6 | `src/cores/emu_pce.cpp` | **Experimental** (`huexpress`, `GPL-2.0`, `NOT_TESTED` on HW) | `.pce` | 256×240 @(32,0) | 16,667 µs | `12.29 / 24.58 ms` (Sync frame) | PCE BRAM gated on `"HUBM"` header | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_ATARI` | 8 | 7 | `src/cores/emu_atari.cpp` | **Experimental** (`stella2014`, `GPL-2.0`) | `.a26` | 160×192 @(80,24) | 16,667 µs | `6.14 / 12.29 ms` (Sync frame) | Save states `.s01–.s05`; no `.sav` | `TARGET_COMPILED` + `HOST_TESTED` (WASI/WASM) |
| `ROM_PICO8` | 9 | 8 | `src/cores/emu_pico.cpp` | **Experimental** (`fake08` + `z8lua`, `MIT`) | `.p8` | 128×128 @(96,56) | 33,333 µs | `3.28 / 6.55 ms` (Sync frame) | Cartdata 256 B (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI Lua VM) |
| `ROM_GENESIS` | 10 | 9 | `src/cores/emu_genesis.cpp` | **Experimental** (`gwenesis`, `GPL-3.0` + Non-Comm M68K, `NOT_TESTED` on HW) | `.gen`, `.md`, `.smd` | 320×224 @(0,8) | 16,667 µs | `14.34 / 28.67 ms` (Sync frame) | Cartridge SRAM (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_SNES` | 11 | 10 | `src/cores/emu_snes.cpp` | **Experimental** (`snes9x`, Non-Commercial, `NOT_TESTED` on HW) | `.sfc`, `.smc` | 256×224 @(32,8) | 16,667 µs | `11.47 / 22.94 ms` (Sync frame) | LoROM/HiROM/ExHiROM checksum header | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_WSWAN` | 12 | 11 | `src/cores/emu_wswan.cpp` | **Experimental** (`oswan`, `GPL-2.0` only, `NOT_TESTED` on HW) | `.ws`, `.wsc` | 224×144 @(48,48) | 16,667 µs | `6.45 / 12.90 ms` (Sync frame) | Cartridge SRAM (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_NGP` | 13 | 12 | `src/cores/emu_ngp.cpp` | **Experimental** (`race`, `GPL-2.0` only, `NOT_TESTED` on HW) | `.ngp`, `.ngc` | 160×152 @(80,44) | 16,667 µs | `4.86 / 9.73 ms` (Sync frame) | Cartridge Flash (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_LYNX` | 14 | 13 | `src/cores/emu_lynx.cpp` | **Experimental** (`handy`, `Zlib`, `NOT_TESTED` on HW) | `.lnx` | 160×102 @(80,69) | 16,667 µs | `3.26 / 6.53 ms` (Sync frame) | Cartridge EEPROM (`.sav`) + states `.s01–.s05` | `TARGET_COMPILED` + `HOST_TESTED` (WASI) |
| `ROM_COLEM` | 15 | 14 | `src/cores/emu_colem.cpp` | **Experimental** (`smsplus`, `GPL-2.0` + Non-Comm Z80) | `.col`, `.sg` | 256×192 @(32,24) | 16,667 µs | `9.83 / 19.66 ms` (Sync scanline) | Save states `.s01–.s05`; no `.sav` | `TARGET_COMPILED` + `HOST_TESTED` (WASI/WASM) |

---

## 3. Controls Inside Emulators

See [`docs/reference/controls.md`](reference/controls.md) for the complete system and per-console button matrix:
- `SELECT + START`: Open Pause Menu
- `SELECT + UP`: Quick Quit confirmation (press `A` twice within 3 seconds to confirm; flushes battery `.sav` before teardown)
- `SELECT + A` / `SELECT + B`: Quick-save / quick-load active slot (`1..5`)
- `SELECT + DOWN`: Cycle DMG 4-shade palette (GB / GBC)
- `SELECT + LEFT`: Toggle A/B turbo
- `SELECT + RIGHT` (Hold): Fast-forward (skips outer `FramePacing::targetFrameUs()` delay)
