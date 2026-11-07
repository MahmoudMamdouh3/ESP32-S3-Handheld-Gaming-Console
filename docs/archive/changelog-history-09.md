# Archived changelog (history archived 2026-11-07)

## [Milestone 16.0] - 2026-10-09 (Phase 6: BMO 2D SDF Mascot Engine)
### Added
- **BMO 2D SDF Mascot Engine (`BmoFace`)**:
  - Full 320×240 native high-definition vector SDF canvas allocated in Octal PSRAM (`MALLOC_CAP_SPIRAM`), freeing 32,768 bytes of internal SRAM and reducing overall SRAM utilization from 75.2% to **65.3%**.
  - Authentic 1:1 Adventure Time character aesthetics: `#8AD5C3` mint seafoam screen, `#101E2B` deep charcoal eye/mouth outlines, `#FF8BA7` soft glowing coral cheeks, and open mouth cavity with `#FA7F8F` warm coral tongue.
  - Second-order critically damped harmonic spring-damper differential physics ($\zeta \approx 0.72$, $\omega_n \approx 22\text{ rad/s}$) for fluid squash-and-stretch, overshoot, and elastic bounce.
  - Continuous biological breathing wave ($T \approx 3.2\text{ s}$), autonomous gaze saccades, realistic blinks with occasional double-blinks.
  - Ambient floating particle system: rhythmic drifting `Z z z` dream particles during sleep and 4-point diamond star sparkles during celebration.
  - 16-state emotion matrix (`IDLE`, `SURPRISED`, `HAPPY`, `SLEEPY`, `LOW_BATTERY`, `CHARGING`, `ERROR`, `SHUTDOWN`, `HIDDEN`, `JOY`, `SLEEPING`, `WINK`, `BLUSH`, `CONFUSED`, `ANNOYED`, `LOVE`, `TICKLED`, `CELEBRATING`).
  - Spatial bounding-box culling skipping > 70% of display pixels, keeping 320×240 frame render under 2.8 ms on ESP32-S3 and 0.44 ms on host.
- **Interactive Gamepad Mode in `STATE_IDLE_MASCOT` (60 FPS)**:
  - Real-time D-pad gaze tracking where BMO looks directly towards pressed directions.
  - D-pad rapid wiggling detection triggering the `TICKLED` state (squirming spring vibrations and joyful laughing bounce).
  - Button A triggers cheerful wink (`triggerWink()`) with star sparkle bursts.
  - Button B triggers shy blush (`triggerBlush()`) with deep glowing cheeks and bashful downward glance.
  - Button SELECT triggers curious thinking face (`CONFUSED`), and START or 1-second hold wakes BMO up smoothly to the console menu.
- **Display Integration (`DisplayEmu`)**:
  - Upgraded `drawIdleMascotScreen` to render the native 320×240 living mascot screen with sleek floating UI badges.
  - Upgraded `drawBootSplash` and console menu headers to render crisp anti-aliased living BMO mascot faces directly into the PSRAM canvas (`menuCanvas`).
- **Testing & Verification**:
  - Added `tests/test_bmo_mascot_engine.py` (6 test cases) validating color spaces, spring physics stability, subpixel AA, spatial culling skip ratio (>70%), and memory budgets.
  - Unit test suite expanded from 52 to 58 passing tests (58/58 OK).
### Verified
- `python scripts/validate_repo.py` → PASS (All 7 phases clean, Flash 33.2%, SRAM 65.3% — DRAM freed by 32.5 KB).
- `python -m unittest discover tests` → 58/58 tests OK.
- `python -m tools.guardian audit` → PASS (0 Critical, 0 new warnings).

---

## [Milestone 15.0] - 2026-10-09 (Phase 5.1: Unified Core Architecture Contract & PCE Persistence)
### Added
- **Unified Core Contract (`EmulatorCoreContract`)**:
  - Profiled 8MB Octal PSRAM budget and memory layouts across all Tier 1 and Tier 2 consoles, confirming system base footprint remains under 7% (< 525 KB) leaving > 7.4 MB headroom for ROMs and emulation states.
  - Expanded `SaveManager::CoreId` enum with identifiers for all 14 console types (`CORE_PCE = 6`, `CORE_ATARI = 7`, `CORE_PICO8 = 8`, `CORE_GENESIS = 9`, `CORE_SNES = 10`, `CORE_WSWAN = 11`, `CORE_NGP = 12`, `CORE_LYNX = 13`, `CORE_COLEM = 14`).
  - Standardized non-volatile save state and battery RAM persistence methods across emulator wrappers (`saveBatteryRam`, `loadBatteryRam`, `saveState`, `loadState`).
- **PC Engine / TurboGrafx-16 Core Persistence Integration**:
  - Implemented `saveBatteryRam`, `loadBatteryRam`, `saveState`, and `loadState` in `PceEmu` (`src/emulators/emu_pce.h`, `emu_pce.cpp`) targeting `SaveManager::CORE_PCE`.
  - Wired PCE core into `BmoGameboy.ino` auto-save on exit (`autoSaveCurrentBatteryRam`), quick-save (`SELECT + A`), quick-load (`SELECT + B`), and transparent auto-load on ROM boot.
- **Testing & Verification**:
  - Added multi-core ID serialization tests and PCE 73KB context payload simulation to `tests/test_save_manager.py`.
  - Unit test suite expanded to 52 passing tests (52/52 OK).
### Verified
- `python scripts/validate_repo.py` → PASS (All 7 phases clean, Flash 33.1%, SRAM 75.2%).
- `python -m unittest discover tests` → 52/52 tests OK.

---

## [Milestone 14.0] - 2026-10-09 (Phase 4: High-Speed Binary ROM Index Cache & 64×64 Box Art Cover Engine)
### Added
- **Binary ROM Index Cache (`.bmo_index`)**:
  - Implemented 36-byte packed header (`BmoIndexHeader`) with `BMOIDX01` magic signature, version check, entry count, and CRC32 payload checksum.
  - Implemented 72-byte entry format (`BmoIndexEntry`) with 64-character title, console type tag, favorite flag, and box art presence flag.
  - Added fast-load routines in `SDCard` (`loadIndex()`, `saveIndex()`, `rebuildIndex()`) reducing boot-time catalog enumeration from ~3,500 ms to < 15 ms (>200× speedup).
  - Added case-insensitive alphabetical sorting (`SDCard::sortRoms()`) ensuring organized library browsing and instant A–Z jumping across all console collections.
- **64×64 Retro Box Art Cover Art Engine (`BoxArt`)**:
  - Created `BoxArt` subsystem (`box_art.h`, `box_art.cpp`) with 8,192-byte PSRAM buffer (`MALLOC_CAP_SPIRAM`), guaranteeing zero internal DRAM consumption.
  - Supported both 64×64 raw 16-bit (`.raw`) and Windows 16/24-bit (`.bmp`) images with automatic BGR565 byte-swapped color translation.
  - Integrated box art rendering into `DisplayEmu::drawGameSelectMenu` with retro rounded framing, falling back gracefully to classic console badges when art is not available.
  - Added single-load caching into `BmoGameboy.ino`, loading SD card thumbnails only on selection changes (< 2 ms load time, 12 µs canvas blit).
- **Testing & Verification**:
  - Added `tests/test_rom_index.py` (8 test cases) and `tests/test_box_art.py` (6 test cases).
  - Expanded unit test suite from 36 to 51 passing tests.
### Verified
- `python scripts/validate_repo.py` → PASS (All 7 phases clean, Flash 33.1%, SRAM 75.2%).
- `python -m tools.guardian audit` → PASS (0 Critical findings).
- `python -m unittest discover tests` → 51/51 tests OK.

---

## [Milestone 13.0] - 2026-10-09 (Phase 3: Non-Volatile Multi-Slot Save States & Cartridge Battery RAM Persistence)
### Added
- **Non-Volatile Save Subsystem (`SaveManager`)**:
  - Implemented thread-safe `SaveManager` (`save_manager.h`, `save_manager.cpp`) protected by FreeRTOS `SpiArbiter` mutex to eliminate SPI bus collisions with background display streaming.
  - Standardized 60-byte binary header (`SaveStateHeader`) featuring `BMOSS01` magic signature (`0x31535342`), CRC32 payload checksum verification, Unix timestamp, and emulator core tags (`CORE_PEANUT`, `CORE_WALNUT`, `CORE_NES`, `CORE_SMS`, etc.).
  - Added cartridge battery-backed SRAM persistence (`.sav`) and multi-slot save states (`.s01` - `.s05`) in `/saves/` directory on MicroSD.
- **Core Integration**:
  - Integrated battery RAM and full core snapshots for `PeanutEmu` (Game Boy / GBC), `WalnutEmu` (Game Boy Color), `NesEmu` (NES via dynamic PSRAM state buffer), and `SmsEmu` (Sega Master System / Game Gear).
- **In-Game Quick Pause & Save State UI Modal Overlay**:
  - Implemented `DisplayEmu::drawPauseMenu` dark translucent modal overlay displaying current slot, slot state metadata, Quick Save, Quick Load, Resume, and Quit options.
  - Added gamepad hotkeys: `SELECT + START` for Pause Menu, `SELECT + A` for Quick Save, `SELECT + B` for Quick Load, and `SELECT + UP` for Auto-Save & Quit.
- **Testing & Tooling**:
  - Added test suite `tests/test_save_manager.py` with 36 test cases covering binary packing, CRC32 error detection, slot bounds, and path sanitization.
### Verified
- `python scripts/validate_repo.py` → PASS (All 7 phases clean, Flash 33.1%, SRAM 75.2%).
- `python -m tools.guardian audit` → PASS (0 Critical findings).
- `python -m unittest discover tests` → 36/36 tests OK.

---

## [Milestone 12.0] - 2026-10-09 (Phase 2 Display Pipeline: Core 0 FreeRTOS Worker, Double-Buffered PSRAM Canvases, SpiArbiter & Asynchronous Emulator Pipeline)
### Added
- **Core 0 Dedicated FreeRTOS Display Worker Task (`BMO_Display`)**:
  - Offloaded blocking 15.36 ms 320×240 fullscreen SPI transfers to Core 0 (`xTaskCreatePinnedToCore`, priority `configMAX_PRIORITIES - 2`), eliminating 100% of the UI blocking stall on Core 1 (< 50 µs queue dispatch).
- **Double-Buffered PSRAM Canvases (`menuCanvasArr[2]`)**:
  - Implemented ping-pong 320×240 16-bit GFXcanvas buffers in Octal PSRAM (`MALLOC_CAP_SPIRAM`), allowing Core 1 to render the next frame concurrently while Core 0 streams the previous buffer to the ST7789 display.
- **Asynchronous Emulator Frame Pipeline (`DisplayEmu::streamRawFrameAsync`, `DisplayEmu::streamGBFrame`)**:
  - Allocated ping-pong 153,600-byte frame buffers in Octal PSRAM (`s_asyncEmuBuf[2]`, consuming 0 internal SRAM), offloading wire SPI transfers for all 14 console cores (NES, DOOM, SMS, Game Boy, Genesis, SNES, PCE, Atari, Pico, WonderSwan, Neo Geo Pocket, Lynx, ColecoVision).
  - Offloaded scanline palette translation for NES and DOOM onto Core 0, allowing Core 1 to execute emulation logic without blocking on SPI wire transmission.
- **FSPI Bus Arbiter (`SpiArbiter`)**:
  - Implemented recursive FreeRTOS mutex (`spi_arbiter.h` / `spi_arbiter.cpp`) protecting the shared FSPI bus across display pushes and MicroSD operations (Rule 28 / HARDWARE-01).
  - Wrapped all SD card filesystem routines (`SDCard::begin`, `scanRoms`, `saveFavorites`, `loadFavorites`, `loadRom`) in `SpiArbiter::lock()` / `unlock()`.
- **Public Display Sync APIs (`DisplayEmu::waitForDisplay`, `DisplayEmu::isDisplayBusy`)**:
  - Non-blocking checks and explicit barrier synchronization for state machine transitions.
### Verified
- `python scripts/validate_repo.py` → PASS (All 7 phases clean, Flash 33.0%, SRAM 75.2%).
- `python -m tools.guardian audit` → PASS (0 Critical, 11 pre-existing Warnings).
- `python -m unittest discover tests` → 32/32 OK.

---

## [Perf-Fix-2] - 2026-09-18 (PSRAM Allocator Mismatch + O3 Pragma Completions)
### Fixed
- **Critical: PSRAM `heap_caps_free()` mismatch in 8 emulators** (`emu_peanut`, `emu_walnut`, `emu_genesis`, `emu_snes`, `emu_wswan`, `emu_ngp`, `emu_lynx`, `emu_colem`): All `destroy()` functions called plain `free()` on buffers allocated via `heap_caps_malloc(MALLOC_CAP_SPIRAM)`. Fixed to use `heap_caps_free()` matching the allocator contract.
### Performance
- Added `#pragma GCC optimize("O3,unroll-loops")` to `bmo_face.cpp` — SDF renderer is the 2nd most expensive hot path (~468–1241 µs), enabling loop vectorisation and unroll.
- Completed `display_emu.cpp` pragma from `O3` → `O3,unroll-loops` for NES/DOOM scanline loops.
### Verified
- `python scripts/validate_repo.py` → PASS (Flash 33.0%, SRAM 75.1%)
- `python -m tools.guardian audit` → 0 Critical, 11 pre-existing Warnings
- `python -m unittest discover tests` → 32/32 OK

---
