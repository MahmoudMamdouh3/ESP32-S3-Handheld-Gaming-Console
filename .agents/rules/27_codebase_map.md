# 27. Codebase Map

This page maps current ownership. Use live headers for exact declarations and the Software Design Document for subsystem contracts.

## Main firmware

- firmware/BmoGameboy/BmoGameboy.ino: setup(), loop(), eight-state launcher, platform selection, wrapper dispatch, input chords, pacing, and emulator teardown.
- firmware/BmoGameboy/partitions.csv: NVS, OTA metadata, 8 MiB app0, and FFat partition boundaries.
- src/core/config.h: GPIO assignments, feature flags, display dimensions, and configured bus clocks.
- src/core/display_emu.*: ST7789 setup, BMO_Display task and queue, staging/canvas buffers, stream functions, menu drawing, and Diagnostics.
- src/core/buttons.*: debounced active-low controls. GPIO banks are sampled sequentially.
- src/core/spi_arbiter.*: mutex for the shared FSPI bus.
- src/core/sd_card.* and rom_index.h: ROM type classification, depth-limited scan, dynamic catalog, BMOIDX02 index, favorites/history, and SD ROM ownership.
- src/core/save_manager.* and path_key.h: BSS1 save states, battery files, screenshots, CRC checks, staged writes, backup recovery, and shared path-derived keys.
- src/core/box_art.*: raw/BMP artwork decoding and artwork loading.
- src/core/theme.h: firmware UI colors and color conversion helpers.
- src/emulators/*.h/.cpp: fourteen wrappers. SMS and Game Gear share one wrapper.
- src/engine/: project-owned/adapted engines, including Walnut-CGB.
- src/vendor/: vendored or adapted emulator engines.
- src/assets/: firmware-resident arrays and other compiled assets. Do not add game data without the owner's separate ROM policy.

## Launcher state and platform indices

There are eight states: BOOT_SPLASH, CONSOLE_MENU, CONSOLE_MUSEUM, GAME_MENU, EMULATOR, PAUSE_MENU, DIAGNOSTICS, and IDLE_MASCOT. Favorites plus fifteen platform entries are listed in the CONSOLES array.

| selectedEmulatorIndex | Platform | Wrapper | Wrapper source |
| ---: | :--- | :--- |
| 0 | Game Boy Color | WalnutEmu | `emu_walnut` |
| 1 | Game Boy DMG | PeanutEmu | `emu_peanut` |
| 2 | NES | NesEmu | `emu_nes` |
| 3 | DOOM | DoomEmu | `emu_doom` |
| 4 | Master System | SmsEmu | `emu_sms` |
| 5 | Game Gear | SmsEmu | `emu_sms` |
| 6 | PC Engine | PceEmu | `emu_pce` |
| 7 | Atari | AtariEmu | `emu_atari` |
| 8 | PICO-8-style runtime | PicoEmu | `emu_pico` |
| 9 | Genesis | GenesisEmu | `emu_genesis` |
| 10 | SNES | SNESEmu | `emu_snes` |
| 11 | WonderSwan | WSwanEmu | `emu_wswan` |
| 12 | Neo Geo Pocket | NGPEmu | `emu_ngp` |
| 13 | Lynx | LynxEmu | `emu_lynx` |
| 14 | ColecoVision / SG-1000 | ColemEmu | `emu_colem` |

The extension list is owned by sd_card.cpp. The wrapper mapping and frame methods are also visible in SDD Section 09. The presence of an extension or wrapper does not establish compatibility with every image.

## Display and input path

The display task is named BMO_Display and is pinned to Core 0. GB/GBC invoke synchronous scanline streaming inside startFrame()/endFrame(); other wrappers generally submit complete frames to DisplayEmu. Do not assume transfer overlap or measured FPS.

Buttons::update() samples GPIO_IN_REG and GPIO_IN1_REG sequentially. GPIO42/47/48 are in the second bank. The shared bus uses SCK=12 and MOSI=11, display CS=10, SD CS=13, and SD MISO=15. SpiArbiter protects TFT and SD transactions.

## Storage and ownership

SDCard owns the dynamic ROM catalog and the heap buffer returned by loadRom(); the launcher passes that ROM pointer to a wrapper and frees it through SDCard::freeRom(). Wrappers free only their own allocations. The index uses BMOIDX02 with 36-byte header and 72-byte entries.

SaveManager stores files under /saves using shared RomKey values derived from the relative ROM path. Battery RAM is written only for supported nonvolatile regions. Save-state payloads are core-specific and not portable between arbitrary firmware builds.

## Tools and docs

- tools/guardian/: CLI, source-derived checks, symbol extraction, UI audit, memory budget, and verification.
- tools/bmo_simulator/: local browser server, UI, WebAssembly adapters, and visual console model.
- scripts/validate_repo.py: repository phases 0–8; Phase 0 compiles the pinned target profile or reports an explicit skip in advisory mode.
- tests/ and tests/host/: Python tests and selected host C/C++ fixtures.
- docs/software-design-document.md: architecture index.
- docs/generated/: generated symbol pages.
- docs/decisions/: architecture decisions.
- docs/hardware_measurements.md: physical measurement procedure and record.
- docs/archive/: dated historical snapshots; may contain superseded claims.

Read docs/software-design-document.md before changing a subsystem. Use python -m tools.guardian prompt-pack and lookup for task-specific source context.
