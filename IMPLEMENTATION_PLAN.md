# Implementation Plan

> **Current Focus:** Phase 4 (High-Speed Binary ROM Index Cache & 64×64 Box Art Cover Art Engine) to eliminate SD directory crawl times and display real-time cartridge box art.

## Foundational Phases [COMPLETED]

- [x] **Phase 1: Stabilize the build environment** (Root-relative paths, CLI scripts, asset validation).
- [x] **Phase 2 (Legacy): Harden the ROM pipeline** (ROM integrity checks, deterministic headers, PROGMEM guards).
- [x] **Phase 2 (Async Engine): Display DMA & Dual-Core Asynchronous Blit Engine** (Core 0 worker task, SpiArbiter, double-buffered PSRAM canvases, 60 FPS non-blocking frame streaming).
- [x] **Phase 3: Non-Volatile Multi-Slot Save States & Cartridge Battery RAM Persistence** (`SaveManager`, slots 1–5, CRC32 verification, SpiArbiter concurrency, in-game pause menu UI & hotkeys).

---

## Completed Phase: Display DMA & Dual-Core Asynchronous Blit Engine [COMPLETE]

### Milestone 2.1: SPI Bus Arbiter & Thread Safety [COMPLETE]
- [x] 1. Implement `SpiArbiter` mutex (`spiBusMutex`) to guarantee exclusive SPI access between display streaming and MicroSD operations (Rule 28 / HARDWARE-01).
- [x] 2. Wrap SD card read/write access points with arbiter locks so background display transfers never corrupt FAT file I/O.

### Milestone 2.2: Core 0 Dedicated Display Worker Task [COMPLETE]
- [x] 1. Spawn `vDisplayTask` pinned to Core 0 (`xTaskCreatePinnedToCore`) with FreeRTOS command queue (`displayQueue`).
- [x] 2. Implement non-blocking asynchronous dispatch for display blits (< 50 µs queue dispatch vs 15.36 ms wire stall).

### Milestone 2.3: Double-Buffered PSRAM UI Canvases [COMPLETE]
- [x] 1. Allocate ping-pong `menuCanvasArr[2]` in Octal PSRAM (`MALLOC_CAP_SPIRAM`).
- [x] 2. Overlap UI layout generation on Core 1 with 80MHz SPI hardware transmission on Core 0.

### Milestone 2.4: Asynchronous Emulator Frame Pipeline [COMPLETE]
- [x] 1. Add non-blocking frame streaming API in `DisplayEmu` for Game Boy, NES, DOOM, and SMS.
- [x] 2. Enable 100% CPU time availability on Core 1 for emulation logic (reclaiming 10.37–15.36 ms per frame).

### Milestone 2.5: Verification & Benchmarking [COMPLETE]
- [x] 1. Pass full Guardian CI validator (`python scripts/validate_repo.py` - Flash 33.0%, SRAM 75.2%).
- [x] 2. Verify flash and SRAM budgets remain within safe limits (DRAM 75.2% < 76%).
- [x] 3. Update `04_known_issues.md` (BUS-01 status) and `CHANGELOG.md`.

---

## Completed Phase: Non-Volatile Multi-Slot Save States & Cartridge Battery RAM Persistence [COMPLETE]

### Milestone 3.1: Architecture & Safe Storage Subsystem (`SaveManager`) [COMPLETE]
- [x] 1. Implement `SaveManager` (`src/core/save_manager.h`, `save_manager.cpp`) with 60-byte binary header (`SaveStateHeader`, magic `BMOSS01`, CRC32, timestamp, slot indices 1–5).
- [x] 2. Coordinate all SD read/write operations under `SpiArbiter` mutex lock (`SpiArbiter::acquire()`, `release()`) to prevent display DMA bus contention.
- [x] 3. Support raw cartridge battery backup RAM `.sav` files under `/saves/<rom>.sav` and multi-slot snapshots under `/saves/<rom>.s01`-`.s05`.

### Milestone 3.2: Emulator Core Integration [COMPLETE]
- [x] 1. `PeanutEmu` (Game Boy / GBC): Hook MBC1/MBC3/MBC5 battery RAM flush/restore and complete core state serialize/deserialize.
- [x] 2. `WalnutEmu` (Game Boy Color): Hook cart RAM serialization and core snapshot save/load.
- [x] 3. `NesEmu` (NES / Agnes): Hook state dumps via dynamic PSRAM state buffer (`agnes_state_size()`) and restore logic.
- [x] 4. `SmsEmu` (Sega Master System / Game Gear): Hook SMS Plus state saving/loading.

### Milestone 3.3: In-Game Quick Pause & Save State UI Overlay [COMPLETE]
- [x] 1. Implement `DisplayEmu::drawPauseMenu` modal overlay (Slot selection, Quick Save, Quick Load, Resume, Quit to Launcher).
- [x] 2. Wire `STATE_PAUSE_MENU` state machine and gamepad hotkeys (`SELECT + START` for pause menu, `SELECT + A` for quick save, `SELECT + B` for quick load, `SELECT + UP` for auto-save & quit).

### Milestone 3.4: Host Unit Testing & Verification [COMPLETE]
- [x] 1. Comprehensive Python unit test suite `tests/test_save_manager.py` (36 tests verifying header packing, CRC32 corruption rejection, boundary checks, and filename hashing).
- [x] 2. Full AI Guardian CI validator clean pass (`python scripts/validate_repo.py` - Flash 33.1%, SRAM 75.2%).
- [x] 3. Zero critical findings in Guardian performance audit (`python -m tools.guardian audit`).

---

## Next Phase: High-Speed Binary ROM Index Cache (`.bmo_index`) & 64×64 Box Art Cover Art Engine

### Milestone 4.1: Binary ROM Index Cache (`.bmo_index`)
- [ ] 1. Design compact binary index format storing sorted ROM entries (title, path, console type, file size, CRC32, box art flag) to eliminate FAT32 directory walk latency (< 10 ms catalog instant-load).
- [ ] 2. Implement background indexing worker on Core 0 or during boot when SD changes are detected.
- [ ] 3. Support fast alphabetic paging and O(1) random access in PSRAM.

### Milestone 4.2: 64×64 Box Art Cover Art Engine
- [ ] 1. Implement RGB565 raw / BMP / PNG box art thumbnail loader from `/boxart/<rom_basename>.raw` or `/boxart/<rom_basename>.bmp`.
- [ ] 2. Render cover art seamlessly in launcher carousel with bilinear or nearest-neighbor aspect ratio letterboxing.
- [ ] 3. Integrate fallback placeholder icon if cover art is absent.


