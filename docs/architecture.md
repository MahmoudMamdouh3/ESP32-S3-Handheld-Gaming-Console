# System Architecture

This document summarizes how the ESP32-S3 firmware, host test runners, and browser/Wokwi simulators are structured and verified.

## Firmware Execution & Dual-Core Split

- **Core 1 (`loop()` in `firmware/BmoGameboy/BmoGameboy.ino`):**
  - Polls active-low button inputs via `Buttons::update()` (`src/core/buttons.cpp`), handling `GPIO_IN_REG` (GPIO `0–31`) and `GPIO_IN1_REG` (GPIO `42, 47, 48`).
  - Owns the system state machine (`STATE_BOOT_SPLASH`, `STATE_CONSOLE_MENU`, `STATE_CONSOLE_MUSEUM`, `STATE_GAME_MENU`, `STATE_EMULATOR`, `STATE_PAUSE_MENU`, `STATE_DIAGNOSTICS`, `STATE_IDLE_MASCOT`).
  - Executes the selected emulator core, processes `SELECT`-chord hotkeys, mounts MicroSD / loads ROMs, and persists save files (`SaveManager`).
- **Core 0 (`BMO_Display` task in `firmware/BmoGameboy/src/core/display_emu.cpp`):**
  - Receives asynchronous display frame jobs via a single-slot FreeRTOS command queue.
  - Converts indexed 8-bit buffers (NES/DOOM) to wire BGR565 and drives the ST7789 panel over FSPI (`320x240` landscape).
  - Game Boy / Game Boy Color scanlines stream synchronously on the calling task while holding `SpiArbiter`; other cores submit full/partial framebuffers via PSRAM staging buffers or synchronous fallback.

## Shared SPI Bus Arbitration

Display (`TFT_CS=10`) and MicroSD (`SD_CS=13`) share FSPI `SCK=12` and `MOSI=11`.

- All SPI bus transactions across Core 0 and Core 1 must acquire `SpiArbiter::lock()` and release `SpiArbiter::unlock()` (`src/core/spi_arbiter.cpp`).
- Failure to initialize or lock the mutex stops execution cleanly rather than continuing unlocked (`HOST_TESTED` via `tests/test_spi_arbiter.py`).
- Before tearing down an emulator wrapper (`destroyActiveEmulator()`), firmware drains any queued asynchronous display work so freed PSRAM framebuffers are never read by Core 0.

## Memory & Partition Architecture

- **Flash (`16 MiB` Quad SPI, `partitions.csv`):**
  - `nvs` (`20 KiB`), `otadata` (`8 KiB`), `app0` (`8 MiB`), `ffat` (`~7.9 MiB`).
  - Default MIT firmware build (`BMO_ENABLE_BAKED_ROMS=0`, `BMO_ENABLE_DOOM=0`) verified via `arduino-cli` (`TARGET_COMPILED`).
- **DRAM (`327,680 B` usable):**
  - Reserved for FreeRTOS stacks, DMA/SPI descriptors, and static variables; static bloat is checked via `arduino-cli` reports and `python -m tools.guardian profile-elf`.
- **Octal PSRAM (`8 MiB`, `MALLOC_CAP_SPIRAM`):**
  - Stores the ROM catalog pool (`SDCard::computePoolCapacity()`, `256..36,000` `RomFile` entries @ `72 B`), `BmoFace` `320x240` BGR565 canvas (`153,600 B`), `BoxArt` `64x64` cache (`8,192 B`), display staging buffers, active loaded ROM buffer, and DOOM heap (`DoomEmu::computeZoneBytes()`, `2.5–6 MiB` clamped with `512 KiB` safety margin).
  - Run `python -m tools.guardian psram-budget` to compute the exact source-derived PSRAM ledger (`HOST_TESTED` via `tests/test_psram_budget.py`).

## Separate Execution Environments (Important Boundaries)

| Environment | What It Executes | Evidence Scope |
| :--- | :--- | :--- |
| ESP32-S3 Firmware (`firmware/BmoGameboy/`) | Full Arduino-ESP32 sketch, FreeRTOS tasks, FSPI/ST7789/SD drivers, 14 emulator wrappers | `TARGET_COMPILED` (CI/local `arduino-cli`); `HARDWARE_TESTED`: none recorded |
| Host C/C++ Tests (`tests/`) | Isolated first-party logic (`rom_index`, `battery_save`, `path_key`, `bmo_face`, `spi_arbiter`, `frame_pacing`) + 11 C engines in `tools/host_cores_runner.c` | `HOST_TESTED` only; does not prove full cartridge compatibility or ESP32 timing |
| Browser Simulator (`tools/bmo_simulator/`) | Canvas 2D UI port + WASM-compiled `bmo_face`, Peanut-GB, Agnes, Stella, Coleco + Three.js CSG CAD teardown | `HOST_TESTED` UI/WASM smoke checks; menus & save slots are browser-only |
| Wokwi Model (`diagram.json`, `wokwi.toml`) | Real ESP32-S3 `.bin` with ILI9341 display substitute and wired/optional GPIO switches | Optional interactive simulation; not ST7789 electrical timing |

## Note on Guardian Tooling

During repository cleanup (Phase 2), the static hardware/safety analyzers in `tools/guardian/` (`ast_linter.py`, `bus_model.py`, `psram_budget.py`, `elf_analyzer.py`, `cppcheck_runner.py`, `clang_tidy_runner.py`) were retained while generated JSON knowledge graphs, rule bundles, and unverified status ledgers were removed.
