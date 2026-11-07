# Archived changelog (history archived 2026-11-07)

## [Phase 5.2 Complete: All 14 Real Emulator Engines, Unified 14-Core Persistence & QoL Backlog] - 2026-10-21
### Added (Phase A — 6 Remaining Emulator Engines Replaced with Real CPU + Video/I/O Cores; 0 Stubs Remaining)
- **Atari 2600 (`src/vendor/stella/atari.h`, `src/vendor/stella/atari.c`)**:
  - Implemented MOS 6507 CPU interpreter (13-bit address space `$0000-$1FFF`, full 6502 ALU/branch/stack/addressing modes), MOS 6532 RIOT (`$0080-$00FF` 128B zero-page RAM, `SWCHA`/`SWCHB`, interval timer `TIM1T/TIM8T/TIM64T/T1024T`/`INTIM`), F8/F6/F4 bankswitching (`$1FF4-$1FFB`), and Atari TIA scanline renderer (`WSYNC`, Playfield `PF0/PF1/PF2` reflect/score modes, `GRP0/GRP1`, `M0/M1`, `BL`, `HMOVE`) into 160×192 PSRAM BGR565 framebuffer.
- **ColecoVision / SG-1000 (`src/vendor/colem/colem.h`, `src/vendor/colem/colem.c`)**:
  - Implemented Zilog Z80A CPU interpreter (3.58 MHz, `$0066` VBlank NMI, `$6000-$7FFF` 8KB RAM, `$8000-$FFFF` Cart ROM, `$A0-$BF` VDP ports, `$80/$C0/$E0/$FC` keypad/joystick strobes) and Texas Instruments TMS9918A VDP (16KB VRAM, Graphics Mode 1 & Mode 2 with 3 screen thirds, 32 hardware sprites 8×8/16×16, 16-color NTSC BGR565 palette) into 256×192 PSRAM framebuffer.
- **Atari Lynx (`src/vendor/lynx/lynx.h`, `src/vendor/lynx/lynx.c`)**:
  - Implemented WDC 65SC02 CMOS CPU interpreter (4 MHz, 64KB unified RAM, CMOS opcodes `BRA`, `STZ`, `INC A`, `DEC A`, `PHX/PLX/PHY/PLY`, `(zp)`), Mikey (`$FD00-$FDFF` 16-color RGB444 palette cache, `DISPADRL/H` `$FD94-$FD95` 4bpp video DMA scanout 160×102), and Suzy (`$FC00-$FCFF` 16×16 hardware multiply at `$FC52-$FC63`, `JOYSW` `$FCB0` input, and linked Sprite Control Block SCB blitter on `SPRGO` `$FC91`).
- **SNK Neo Geo Pocket Color (`src/vendor/ngp/ngp.h`, `src/vendor/ngp/ngp.c`)**:
  - Implemented Toshiba TLCS-900/H 32-bit CISC CPU interpreter (`XWA..XSP`, 24-bit bus `$004000-$007FFF` Work RAM, `$006F82` Joypad, `$008000-$00BFFF` K1GE/K2GE Video ASIC, `$200000` Cart ROM) and K1GE/K2GE 160×152 scanline renderer (Scroll Plane 1 `$9000`, Scroll Plane 2 `$9800`, 64 hardware sprites `$8800`, 512 2bpp character tiles `$A000-$BFFF`, 12-bit RGB444 palette RAM `$8200-$83FF`).
- **Super Nintendo / SFC (`src/vendor/snes/snes.h`, `src/vendor/snes/snes.c`)**:
  - Implemented WDC 65C816 16-bit CPU interpreter (8/16-bit `M`/`X` mode flags, `XCE`, `REP`, `SEP`, `JSL/RTL`, `JML`, 24-bit `PBR:PC` and `DBR:Addr` LoROM bus), 8-channel General Purpose DMA (`$420B` `MDMAEN`), and S-PPU 256×224 scanline renderer (64KB VRAM + 32KB SRAM + 128KB WRAM in Octal PSRAM, CGRAM 256-color BGR555 to pre-swapped BGR565 `palette_cache`, BG Modes 0/1 2bpp & 4bpp tilemaps, and 128 OAM 4bpp sprites).
- **PICO-8 Fantasy Console (`src/vendor/pico/pico.h`, `src/vendor/pico/pico.c`)**:
  - Implemented 32KB PICO-8 RAM map (`$0000-$1FFF` sprite sheet, `$2000-$2FFF` map, `$3000` flags, `$5F00` draw state/palettes/clip/camera, `$5F4C` input, `$6000-$7FFF` 128×128 4bpp screen), C graphics primitives (`pico_cls`, `pico_pset`, `pico_pget`, `pico_rectfill`, `pico_circfill`, `pico_spr`, `pico_map`), `.p8` text section parser (`__gfx__`, `__map__`, `__gff__`), and compact draw command stream runner at `$4300`.

### Added (Phase B — Unified 14-Core Persistence Contract)
- **`src/emulators/emu_*.h` & `.cpp`, `BmoGameboy.ino`**:
  - Implemented `saveBatteryRam`, `loadBatteryRam`, `saveState`, and `loadState` across `AtariEmu`, `PicoEmu`, `GenesisEmu`, `SNESEmu`, `WSwanEmu`, `NGPEmu`, `LynxEmu`, and `ColemEmu` (preserving live PSRAM pointers during state restore).
  - Wired all 13 cartridge emulators into `autoSaveCurrentBatteryRam()`, `quickSaveState()`, `quickLoadState()`, and boot auto-load in `BmoGameboy.ino`.

### Added (Phase C — Quality-of-Life Backlog Features & Tests)
- **Per-Core Button Remap (A/B Swap) & Auto-Fire Turbo (`src/core/buttons.h`, `src/core/buttons.cpp`, `BmoGameboy.ino`)**:
  - Added `Buttons::setGameplayMode`, `setSwapAB`, `isSwapAB`, `toggleSwapAB`, `setTurboEnabled`, `isTurboEnabled`, `toggleTurbo`, and `SELECT + LEFT` in-game hotkey.
- **Recently Played History Playlist (`src/core/sd_card.h`, `src/core/sd_card.cpp`, `BmoGameboy.ino`)**:
  - Added `SDCard::recordRecentLaunch`, `getRecentCount`, `getRecentFilename`, `loadRecentHistory`, `saveRecentHistory` backed by `/history.txt` on MicroSD.
- **In-Game Screenshot Dump (`src/core/save_manager.h`, `src/core/save_manager.cpp`)**:
  - Added `SaveManager::saveScreenshotBmp()` writing 24-bit BMP screenshots to `/screenshots/<game>_<ms>.bmp` under `SpiArbiter::lock()`.
- **Konami Code Easter Egg (`BmoGameboy.ino`)**:
  - Added `kKonamiSeq` (`UP, UP, DOWN, DOWN, LEFT, RIGHT, LEFT, RIGHT, B, A`) detector in `STATE_IDLE_MASCOT` triggering BMO's party dance, wink, and toast.
- **Test Suite Expansion (`tests/test_new_cores_and_qol.py`, `tests/test_all_tiers_validation.py`)**:
  - Added 15 unit tests verifying all 6 new emulator cores, unified persistence, and QoL features (`113/113` total unit tests passing).
### Added (Phase D — Native Host C Execution & Visual Regression, Folder Hierarchy Navigation, Custom Console Bezels & Developer Tools)
- **Native Host C Compilation & 50-Frame Visual Regression (`tools/host_cores_runner.c`, `tests/test_native_cores_execution.py`)**:
  - Compiles and executes all 10 custom C emulator cores (`atari.c`, `colem.c`, `lynx.c`, `ngp.c`, `snes.c`, `wswan.c`, `genesis.c`, `sms.c`, `pce.c`, `pico.c`) on the host x64 CPU across 50 frames, verifying CPU execution, RAM/register transitions, and deterministic FNV-1a framebuffer hashes (`VERIFIED_HOST`).
  - Fixed `wswan.c` color palette mirror offset (`0xFE00 + (pal_idx << 1)`) and expanded NEC V30MZ instruction decoding (`MOV reg8, imm8` `B0..B7`, `MOV AL/AX, [moffs16]` `A0..A3`, `INC/DEC reg16` `40..4F`, `PUSH/POP reg16` `50..5F`, and immediate ALU opcodes `04/2C/34/3C`).
  - Fixed `pce.c` reset `MPR0` mapping (`ctx->mpr[0] = 0xFF`) so Hardware I/O (`$0000-$1FFF` VDC/VCE/PSG/Timer/Joypad) is mapped on power-on.
  - Updated `lynx.c` Suzy 16×16 hardware multiply trigger on both `MATHD` (`$FC52`) and `MATHA` (`$FC55`) writes with little-endian product in `$FC60..$FC63`.
- **Recursive Folder Hierarchy Navigation (`src/core/sd_card.h`, `src/core/sd_card.cpp`, `src/core/display_emu.cpp`)**:
  - Added `SDCard::scanDirectory(dirPath, depth, hasArtDirs)` supporting recursive ROM folder discovery up to depth 3 while ignoring system directories (`/saves`, `/screenshots`, `/boxart`, `/covers`), sorting by basename, and stripping path prefixes in `DisplayEmu::sanitizeRomTitle`.
- **Custom Console Bezels (`1:1 Mode`) & Pause Menu QoL Controls (`src/core/display_emu.h`, `src/core/display_emu.cpp`, `BmoGameboy.ino`)**:
  - Added `DisplayEmu::setBezelEnabled`, `isBezelEnabled`, `toggleBezel`, `drawConsoleBezel`, and `getLastFramebuffer`.
  - Wired Pause Menu `LEFT` (A/B swap), `RIGHT` (Console Bezel toggle), and `SELECT` (Screenshot BMP capture).
- **Developer, Telemetry & Companion Tools (`scripts/fetch_covers.py`, `tools/telemetry_plotter.py`, `tools/bmo_simulator/`, `tools/guardian/cli.py`)**:
  - Refactored `scripts/fetch_covers.py` into a clean CLI/library generating 64×64 BGR565 `.raw` (8,192 B) and 24-bit `.bmp` covers plus Libretro Thumbnail URLs.
  - Created `tools/telemetry_plotter.py` for serial telemetry parsing, p50/p95/p99 frame latency, jitter analysis, and ASCII/JSON plotting.
  - Added WebSerial SD Manager (`connectWebSerial`, `ingestUploadedRoms`) and drag-and-drop ROM loader to `tools/bmo_simulator/`.
  - Added `python -m tools.guardian benchmark-ai` achieving 100.0% across intent routing, hardware pin ground-truth, symbol grounding, and production core coverage.
### Verification
- `python scripts/validate_repo.py`: `VALIDATION PASSED — all phases clean`.
- `python -m tools.guardian verify`: `120 tests` — `OK` (`0 Critical violations`).
- `python -m tools.guardian benchmark-ai`: `100.0% PASS across all 4 dimensions`.

---
