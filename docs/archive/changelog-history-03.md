# Archived changelog (history archived 2026-11-07)

## [Playable Browser Atari 2600 Core] - 2026-11-03
### Added
- **Atari 2600 WebAssembly gameplay (`tools/bmo_simulator/atari_wasm.cpp`, `scripts/build_wasm_cores.py`)**: Compiles the same vendored Stella engine used by firmware and executes uploaded `.a26` images with active-low Atari joystick mapping, firmware TIA palette, centered 160×192 output, and session save/restore.
- **Generated Atari regression ROM (`tests/test_native_cores_execution.py`)**: Exercises Atari frame execution, rendered color output, input mapping, state restore, supported ROM image sizes, and the existing Game Boy/NES gameplay paths without bundling game ROMs.

### Changed
- The browser accepts `.gb`, `.nes`, and `.a26` ROMs. Atari ROM sizes not supported by the firmware core are explicitly rejected.
- Updated simulator support labels and documentation to describe three playable cores and the remaining firmware-only systems.

### Known limitation
- Browser gameplay supports only the firmware Peanut-GB (`.gb`), Agnes (`.nes`), and Stella Atari 2600 (`.a26`) cores. It does not execute the ESP32 firmware, the other 11 console cores, physical SPI/SD timing, or reproduce the physical enclosure electrically. Wokwi still uses an ILI9341 display substitute rather than the physical ST7789.

## [Playable Browser NES Core] - 2026-11-03
### Added
- **NES WebAssembly gameplay (`tools/bmo_simulator/nes_wasm.cpp`, `scripts/build_wasm_cores.py`)**: Compiles the same vendored Agnes C core used by firmware and executes uploaded `.nes` images with controller input, the firmware palette, a centered 256×240 viewport, frame pacing, and session save/restore.
- **Generated NES regression ROM (`tests/test_native_cores_execution.py`)**: Exercises NES frames, framebuffer output, button mapping, state restore, unsupported mappers, and the existing Game Boy gameplay path without bundling game ROMs.

### Changed
- The browser accepts `.gb` and `.nes` ROMs up to 8 MiB and routes each to its firmware-derived WebAssembly core. Other systems remain explicitly unsupported.
- Updated simulator descriptions and setup documentation to distinguish the two playable browser systems from the other firmware-only cores and from full hardware simulation.

### Known limitation
- Browser gameplay supports only the firmware Peanut-GB (`.gb`) and Agnes NES (`.nes`) cores. It does not execute the ESP32 firmware, the other 12 console cores, physical SPI/SD timing, or reproduce the physical enclosure electrically. Wokwi still uses an ILI9341 display substitute rather than the physical ST7789.

### Verification
- `python -m unittest tests.test_native_cores_execution.TestNativeCoresExecutionAndPhaseD`: `Ran 10 tests in 8.998s`; `OK`.
- `python scripts/validate_repo.py`: `Flash: 5,698,412 / 16,777,216 bytes (34.0%)`; `VALIDATION PASSED in 25607 ms — all phases clean.`
- `python -m tools.guardian ui-audit`: `UI-AUDIT CLEAN`.

## [Playable Browser Game Boy Core] - 2026-11-03
### Added
- **Real Game Boy WebAssembly execution (`tools/bmo_simulator/gameboy_wasm.cpp`, `scripts/build_wasm_cores.py`)**: Compiles the same vendored Peanut-GB implementation used by firmware and runs uploaded `.gb` ROMs frame-by-frame with the browser's D-pad and A/B/Start/Select inputs.
- **ROM and core regression coverage (`tests/test_native_cores_execution.py`)**: Builds a small generated homebrew ROM and verifies actual frame progression, rendered pixels, input state, and save/restore behavior without bundling a commercial ROM.

### Changed
- Replaced the synthetic framebuffer and fabricated game catalog with a ROM-backed Game Boy library. The browser now shows only user-loaded ROMs and rejects unsupported formats, malformed headers, and unsupported cartridge mappers with an explicit message.
- Updated the browser interface and setup instructions to distinguish playable Game Boy emulation from the canvas-rendered menus and the separately simulated ESP32 firmware.

### Known limitation
- Browser gameplay currently supports only the firmware Peanut-GB core (`.gb`). It does not execute the ESP32 firmware, the other 13 console cores, physical SPI/SD timing, or reproduce the physical enclosure electrically. Wokwi still uses an ILI9341 display substitute rather than the physical ST7789.

## [Firmware-Backed Wokwi Hardware Diagram] - 2026-11-03
### Added
- **Wokwi hardware wiring (`diagram.json`, `wokwi.toml`)**: Added the ESP32-S3 board, SPI TFT model, microSD card, and eight buttons wired from `config.h`; the display and SD share MOSI/SCK and use separate chip selects.
- **Wokwi SD-card staging (`sdcard/README.md`)**: Added a dedicated card-content directory so Wokwi does not copy the repository's ROM and build trees into the virtual card.
- **Hardware wiring contract test (`tests/test_wokwi_hardware_parity.py`)**: Checks simulator GPIO connections against firmware macros and rejects assignments to reserved or disabled peripheral pins.

### Changed
- The browser canvas is labeled as a UI preview, not a 1:1 firmware simulator. Removed its battery gauge and synthesized audio because battery monitoring and I2S audio are disabled on the physical board.
- Corrected the simulator's native-C-core verification endpoint from 10 cores to the actual 11-core test.

### Known limitation
- Wokwi does not provide an ST7789 model; the diagram uses its ILI9341 SPI display model, which does not reproduce the physical panel reset/backlight connections.

## [Original Box Art Pipeline & Firmware-Matched Simulator Library Screen] - 2026-11-03
### Added
- **Full-resolution box art pipeline (`scripts/fetch_covers.py`)**:
  - Downloads the original Libretro box art PNGs (256x256 up to 512x731) for 61 titles across every console, follows Libretro symlink entries (Atari Lynx), and builds a 640x640 Virtual BMO cover from `resources/Virtual BMO GBS Source Code.zip`.
  - Writes masters to `roms/boxart/hd/`, Lanczos-resampled 64x64 `.raw` (byte-swapped BGR565) and `.bmp` files to `roms/boxart/` for `BoxArt::load()`, and simulator PNGs to `tools/bmo_simulator/covers/`. `--target-size WxH` supports future higher-resolution panels.
- **Original Box Art panel in the Web Simulator (`tools/bmo_simulator/index.html`, `simulator.css`, `simulator.js`)**: shows the full-resolution scan and its pixel size for the selected game. `?game=<n>` URL parameter selects a game for screenshots.
- **`TestCoverArtPipeline` (`tests/test_native_cores_execution.py`)**: covers resize, `.raw`/`.bmp` output sizes, simulator/script slug parity, and PNG validity when covers are present.

### Changed
- **Simulator game library screen now mirrors `DisplayEmu::drawGameSelectMenu()`**: same cover card geometry, 64x64 art slot, status pill, scroll track, and footer button pills, replacing the earlier 5-row list that did not exist on the device.
- `tools/bmo_simulator/covers/` is gitignored (22 MB); run `python scripts/fetch_covers.py` to regenerate.

### Fixed
- **`BmoGameboy.ino`**: when a ROM fails to load or an emulator fails to start, the game menu is re-entered with the selected game's box art reloaded (it was unloaded before launch and stayed blank).

---

## [Curated Top Picks Library Filter, MBC7 Accelerometer Mapper & Real Simulator Screenshots] - 2026-11-03
### Added
- **Curated Top Picks (`[TOP PICK]`) Library Filter & Hall-of-Fame Catalog (`src/core/sd_card.h`, `src/core/sd_card.cpp`, `src/core/display_emu.h`, `src/core/display_emu.cpp`, `BmoGameboy.ino`, `tools/bmo_simulator/simulator.js`)**:
  - Added `bool isPopular` to `struct RomFile` and implemented `SDCard::isPopularGame()` and `SDCard::getPopularCountForType()` across all 14 console platforms (`ROM_GB` through `ROM_WAD`) so users with large SD card libraries can identify iconic titles immediately.
  - Added `DisplayEmu::setTopPicksFilter()` / `DisplayEmu::getTopPicksFilter()`, `TOP PICK` badge rendering on game cards, and one-button `START` toggling in `STATE_GAME_MENU` (`rebuildVisibleGames()`) and the Web Simulator (`tools/bmo_simulator/simulator.js`).
- **MBC7 (`0x22`) Accelerometer + EEPROM Cartridge Support (`src/engine/walnut_cgb/walnut_cgb.h`, `src/vendor/peanut_gb/peanut_gb.h`)**:
  - Implemented MBC7 (`0x22`, `MBC = 7`) ROM banking, D-Pad-driven ADXL202 2-axis accelerometer tilt registers (`$A020-$A050`, centered at `0x81D0` ± `0x0090`), and 93LC56 EEPROM ready status (`$A080`) so `Kirby - Tilt 'n' Tumble (USA).gbc` boots and plays with zero errors.

### Fixed
- **Extracted Zipped ROMs in `roms/gb/` and `roms/gbc/` & Verified All 19 Installed ROMs Natively**:
  - Extracted raw `.gb` and `.gbc` binaries alongside `.zip` archives in `roms/gb/` and `roms/gbc/` (and renamed `hotdog.zip` to `Kirby - Tilt 'n' Tumble (USA).zip`) so `SDCard::scanDirectory()` indexes all 13 SD ROMs directly, and verified all 13 SD ROMs + 6 flash fallback ROM headers across 180 frames in native `Walnut-CGB` / `Peanut-GB` execution (`0` header/global checksum or init errors).
- **Replaced AI-Generated Images in `docs/assets/` and `README.md` with Real Captures (`docs/assets/*.png`, `README.md`, `tools/bmo_simulator/simulator.css`)**:
  - Removed all 8 fake AI-generated images from `docs/assets/` (`bmo_hardware_showcase.jpg`, `bmo_living_companion.jpg`, `bmo_carousel_menu.jpg`, `bmo_game_library.jpg`, `hardware_and_states.png`, `hardware_bmo_face.png`, `hardware_charging.png`, `hardware_gameplay.png`) and replaced them with real headless Chrome screenshots of the 1:1 Web Simulator (`sim_full_workstation.png`, `sim_living_companion.png`, `sim_console_carousel.png`, `sim_top_picks_library.png`, `sim_console_museum.png`) and a 2×2 native C emulator RGB565 framebuffer grid (`native_emu_gameplay_grid.png`).
- **Repository Root & Hierarchy Cleanup (`.gitignore`, `LICENSE`, `wokwi.toml`, `resources/aseprite/`)**:
  - Removed committed build/binary artifacts and log dumps from the repository root (`arduino-cli.exe` untracked from Git while kept locally, `arduino-cli-bin/`, `arduino-cli.zip`, `zig.zip`, `host_test.exe`, `host_test.pdb`, `cppcheck.txt`, `sizes.txt`, `test.bmp`, `LICENSE.txt`, and `scratch/doomgeneric`).
  - Relocated and fixed spelling on `"BMO asperite files/"` to `resources/aseprite/bmo_palette.aseprite` and `resources/aseprite/bmo_sprite_0001.aseprite`, removed leftover AI-generated media in `resources/`, updated `LICENSE` copyright holder, and updated `wokwi.toml` ELF path.

---
