# Platform Compatibility, SPI Bus Budget & Core Status

The launcher defines `CONSOLES[]` with Favorites (`ROM_FAVORITES`, index `0`) plus fifteen platform entries mapping to fourteen C++ emulator wrappers (`SmsEmu` handles both Sega Master System and Game Gear).

## 1. Evidence & Maturity Tiers

- **Evidence Levels:** Every platform wrapper below is `TARGET_COMPILED` across all applicable build profiles (`default`, `no-doom`, `mit`) and `HOST_TESTED` via native C/C++ or WASI/WASM harnesses (`tests/`). **Physical device records (`HARDWARE_TESTED`): `0` recorded yet (`docs/hardware_measurements.md`).**
- **Tier Classification:**
  - **Primary (`MIT` / Permissive, Core 0 Async Display Streaming):** Game Boy (`Peanut-GB`), Game Boy Color (`Walnut-CGB`), and NES (`agnes`). Included in all build profiles (`default`, `no-doom`, and redistributable `mit`).
  - **Experimental (`BMO_EXPERIMENTAL_CORES=1` or `BMO_ENABLE_DOOM=1`, `NOT_TESTED` on physical hardware):** Included in personal local builds (`default` / `no-doom`). Several 16-bit and handheld emulators (`SNES`, `Genesis`, `PC Engine`, `WonderSwan`, `Neo Geo Pocket`, `Atari Lynx`) push synchronous full-frame updates over SPI and have not yet been benchmarked on the physical ESP32-S3 perfboard.

---

## 2. Master Platform Compatibility & Bus Budget Table

Pure SPI wire transfer times (`Wire @ 80 MHz` and `Wire @ 40 MHz`) are computed by `python -m tools.guardian bus-calc` (`t = bits / f_SPI`). Compare against each platform's `Target Period`: when `Wire @ 40 MHz` exceeds or approaches the target frame period, full-rate 60 FPS requires either `80 MHz` SPI, Core 0 async overlap, or frame-skipping ([ADR 0014](decisions/0014-display-spi-clock.md)).

| `RomType` | Wrapper (`Source`) | Upstream Core & License | Maturity Tier | Viewport (`W×H`) | Target Period | Wire @ `80 MHz` / `40 MHz` | Transfer Mode | Evidence Level |
| :--- | :--- | :--- | :--- | :--- | ---: | ---: | :--- | :--- |
| `ROM_GB` | `emu_peanut.cpp` | `Peanut-GB` (`MIT`) | **Primary** (`mit` profile) | `240×216` @(40,12) | `16,742 µs` (`59.7 Hz`) | `10.37 ms` / `20.74 ms` | Core 0 async queue | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_GBC` | `emu_walnut.cpp` | `Walnut-CGB` (`MIT`) | **Primary** (`mit` profile) | `240×216` @(40,12) | `16,742 µs` (`59.7 Hz`) | `10.37 ms` / `20.74 ms` | Core 0 async queue | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_NES` | `emu_nes.cpp` | `agnes` (`MIT`) | **Primary** (`mit` profile) | `256×240` @(32,0) | `16,667 µs` (`60.0 Hz`) | `12.29 ms` / `24.58 ms` | Core 0 async 8-bit | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_WAD` | `emu_doom.cpp` | `doomgeneric` (`GPL-2.0`) | **Experimental** (`default`) | `320×200` @(0,20) | `28,571 µs` (`35.0 Hz`) | `12.80 ms` / `25.60 ms` | Core 0 async 8-bit | `TARGET_COMPILED` (`NOT_TESTED` on HW) |
| `ROM_SMS` | `emu_sms.cpp` | `SMS Plus` (`GPL-2.0` + Non-Comm Z80) | **Experimental** | `256×192` @(32,24) | `16,667 µs` (`60.0 Hz`) | `9.83 ms` / `19.66 ms` | Sync scanline SPI | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_GG` | `emu_sms.cpp` | `SMS Plus` (`GPL-2.0` + Non-Comm Z80) | **Experimental** | `160×144` @(80,48) | `16,667 µs` (`60.0 Hz`) | `4.61 ms` / `9.22 ms` | Sync scanline SPI | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_PCE` | `emu_pce.cpp` | `HuExpress` (`GPL-2.0`) | **Experimental** | `256×240` @(32,0) | `16,667 µs` (`60.0 Hz`) | `12.29 ms` / `24.58 ms` | Sync frame SPI | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_ATARI` | `emu_atari.cpp` | `Stella 2014` (`GPL-2.0`) | **Experimental** | `160×192` @(80,24) | `16,667 µs` (`60.0 Hz`) | `6.14 ms` / `12.29 ms` | Sync frame SPI | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_PICO8` | `emu_pico.cpp` | `Tac08` + `z8lua` (`MIT`) | **Experimental** | `128×128` @(96,56) | `33,333 µs` (`30.0 Hz`) | `3.28 ms` / `6.55 ms` | Sync frame SPI | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_GENESIS` | `emu_genesis.cpp` | `Gwenesis` (`GPL-3.0` + Non-Comm M68K) | **Experimental** (`NOT_TESTED` on HW) | `320×224` @(0,8) | `16,667 µs` (`60.0 Hz`) | `14.34 ms` / `28.67 ms` | Sync frame SPI | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_SNES` | `emu_snes.cpp` | `Snes9x 2005` (Non-Commercial) | **Experimental** (`NOT_TESTED` on HW) | `256×224` @(32,8) | `16,667 µs` (`60.0 Hz`) | `11.47 ms` / `22.94 ms` | Sync frame SPI | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_WSWAN` | `emu_wswan.cpp` | `Oswan` (`GPL-2.0` only) | **Experimental** (`NOT_TESTED` on HW) | `224×144` @(48,48) | `16,667 µs` (`60.0 Hz`) | `6.45 ms` / `12.90 ms` | Sync frame SPI | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_NGP` | `emu_ngp.cpp` | `RACE` (`GPL-2.0` only) | **Experimental** (`NOT_TESTED` on HW) | `160×152` @(80,44) | `16,667 µs` (`60.0 Hz`) | `4.86 ms` / `9.73 ms` | Sync frame SPI | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_LYNX` | `emu_lynx.cpp` | `Handy` (`Zlib`) | **Experimental** (`NOT_TESTED` on HW) | `160×102` @(80,69) | `16,667 µs` (`60.0 Hz`) | `3.26 ms` / `6.53 ms` | Sync frame SPI | `TARGET_COMPILED` + `HOST_TESTED` |
| `ROM_COLEM` | `emu_colem.cpp` | `ColEm` (`GPL-2.0` + Non-Comm Z80) | **Experimental** | `256×192` @(32,24) | `16,667 µs` (`60.0 Hz`) | `9.83 ms` / `19.66 ms` | Sync scanline SPI | `TARGET_COMPILED` + `HOST_TESTED` |

---

## 3. Controls Inside Emulators

See [`docs/reference/controls.md`](reference/controls.md) for the complete system and per-console button matrix:
- `SELECT + START`: Open Pause Menu
- `SELECT + UP`: Quick Quit confirmation (press `A` twice within 3 seconds to confirm; flushes battery `.sav` before teardown)
- `SELECT + A` / `SELECT + B`: Quick-save / quick-load active slot (`1..5`)
- `SELECT + DOWN`: Cycle DMG 4-shade palette (GB / GBC)
- `SELECT + LEFT`: Toggle A/B turbo
- `SELECT + RIGHT` (Hold): Fast-forward (skips outer `FramePacing::targetFrameUs()` delay)
