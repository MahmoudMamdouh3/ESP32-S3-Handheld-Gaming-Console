# Archived changelog (history archived 2026-11-07)

## [Full Repo Objective Audit, Firmware Bug Fixes & AI Harness Overhaul] - 2026-10-21
### Fixed (Firmware & Emulator Engines)
- **`src/core/bmo_face.cpp`**:
  - Fixed BGR565 byte-swap unpacking bug in particle alpha-blending (`renderMascotInternal`), which previously unpacked byte-swapped BGR565 values without un-swapping bytes first and swapped R/B channels.
  - Fixed `BmoFace::SLEEPING` anti-aliasing halo bug where SDF edge blending hardcoded `COL_MINT_BG` instead of `COL_DUSK_BG`.
  - Fixed unaligned 32-bit pointer dereference (`*(uint32_t*)&row[c]`) when `destX` or `stride` is odd.
  - Removed unused dead variables (`bgR`, `bgG`, `bgB`).
- **`src/core/display_emu.cpp`**:
  - Removed per-frame `BmoFace::setGaze(0.35f, -0.28f)` call inside `drawConsoleSelectMenu()`, which previously overwrote left/right carousel glances set in `BmoGameboy.ino` every frame and forced `s_dirty = true` at 60 Hz.
  - Added `waitForDisplay()` and `SpiArbiter::lock()` / `unlock()` around `showSDCardWarning()` to prevent concurrent Core 0 / Core 1 FSPI bus corruption.
- **`src/core/sd_card.cpp`**:
  - Reset `s_favoritesCount = 0` in `SDCard::begin()` to prevent unbounded accumulation across re-initializations.
  - Fixed `s_favoritesCount` desynchronization between `loadIndex()` and `loadFavorites()`.
  - Added fast directory existence pre-check for `/boxart` and `/covers` in `scanRoms()`, avoiding up to 65,536 redundant `SD.exists()` FAT32 lookups on first boot when cover directories are absent.
- **Classic DOOM (`src/vendor/doom/src/`, `src/emulators/emu_doom.cpp`)**:
  - Wrapped `W_StdC_OpenFile`, `W_StdC_CloseFile`, and `W_StdC_Read` (`w_file_stdc.c`) in `Doom_SpiLock()` / `Doom_SpiUnlock()` (`SpiArbiter`) to eliminate shared FSPI bus collisions between Core 1 SD WAD reads and Core 0 async display transfers.
  - Routed `ExtendLumpInfo` (`w_wad.c`) `lumpinfo` allocation (40–95 KB) to Octal PSRAM (`Doom_MallocPSRAM`) instead of internal DRAM `calloc()`.
  - Implemented complete DOOM session teardown (`DoomEmu::destroy()`, `doomgeneric_Destroy()`, `W_Shutdown()`, `I_ShutdownZone()`) releasing the 6 MiB PSRAM zone, 64 KB `DG_ScreenBuffer`, `lumpinfo`, `exit_funcs`, and open WAD file handles.
  - Resolved all 4 Cppcheck error-severity findings (`i_system.c:77,291` null-pointer checks on `malloc`, `am_map.c:377-378` signed integer overflow guard when `numvertexes <= 0`, `p_switch.c:117` explicit `alphSwitchList` array bounds guard).
- **PC Engine (`src/vendor/pce/pce.c`)**:
  - Fixed initial VCE palette initialization overflow where 3-bit RGB values were pre-multiplied by 36 before calling `pce_rgb_to_bgr565()`.
  - Fixed `pce_load_rom()` mapping `MPR[7]` to ROM bank `0x00` (instead of RAM `0xF8`) and reading the 65C02/HuC6280 reset vector from bank 0 offset `0x1FFE` (`$FFFE`).
- **Sega Genesis (`src/vendor/genesis/genesis.c`)**:
  - Masked VDP register 23 (`vdp_regs[23] & 0x7F`) during 68K-to-VDP DMA source calculation so DMA mode bit 7 does not shift into the 24-bit ROM address.
  - Fixed 68000 16-bit displacement branch calculation (`BRA`, `BSR`, `Bcc`) so displacement is relative to `instruction_pc + 2` rather than `instruction_pc + 4`.
- **Bandai WonderSwan (`src/vendor/wswan/wswan.c`, `src/emulators/emu_wswan.cpp`)**:
  - Fixed unreachable `else if (port == 0xC1)` in `wswan_out()`, wired `bank_rom` / `bank_sram` segment banking into `wswan_read8()` / `wswan_write8()`, initialized `bank_rom[0,2,3] = 0xFF` in `wswan_reset()` so the `$F0000` boot alias is active on reset, fixed port `0xB5` active-high multiplexed keypad scan, and corrected `Buttons::START` bitmask (`0x0080`) in `WSwanEmu::update()`.
- **Sega Master System / Game Gear (`src/vendor/smsplus/sms.c`, `src/vendor/smsplus/sms.h`)**:
  - Expanded Z80 instruction decoder and wired VDP data/control ports (`0xBE`/`0xBF`) and joypad I/O ports (`0xDC`/`0xDD`) so ROM code can upload tiles/palettes and read input.
  - Added pre-swapped BGR565 `palette_cache[32]` updated on CRAM writes (supporting both 6-bit SMS and 12-bit Game Gear RGB444 CRAM formats), eliminating 49,152 per-pixel RGB multiplications and bit-shifts per frame in `sms_render_scanline()`.
- **Hot-Path Menu, BoxArt & CRC32 Performance (`BmoGameboy.ino`, `src/core/box_art.cpp`, `src/core/save_manager.cpp`)**:
  - Added `s_pauseMetaDirty` caching (`s_pauseHasSave`, `s_pauseHasBatt`) in `STATE_PAUSE_MENU` so idle pause frames do not stall `waitForDisplay()` and execute 60 FAT32 `SD.exists()` checks per second.
  - Gated `BoxArt::load()` calls in `BmoGameboy.ino` on `selectedGame()->hasBoxArt` (precomputed in `.bmo_index`), and guarded the 8KB PSRAM `memset` in `BoxArt::unload()` with `hadArt`, eliminating SD card bus locks and PSRAM clears when scrolling past ROMs without cover art.
  - Replaced bitwise branching `SaveManager::computeCRC32()` inner loop with a 16-entry (64-byte `.rodata`, 0 DRAM) branchless nibble lookup table (`kCrc32NibbleTable`), speeding up `.bmo_index` and save-state CRC32 verification ~5×.
  - Fixed additional DOOM array-bounds off-by-one bugs (`f_finale.c` `c >= HU_FONTSIZE`, `p_inter.c` `ammo >= NUMAMMO`, `p_spec.c` `h < MAX_ADJOINING_SECTORS`) and `malloc` null check in `m_config.c`.

### Fixed (AI Guardian Environment, Safeguards, Scripts & Tests)
- **`scripts/generate_ai_knowledge_base.py` & `.agents/rules/04_known_issues.md`**:
  - Eliminated hardcoded false GPIO pin numbers (`TFT_SCLK=13, TFT_DC=9, START=0`, etc.) and false `FlashMode=opi` claims. `generate_ai_knowledge_base.py` now parses `firmware/BmoGameboy/src/core/config.h` dynamically as the single ground-truth source.
  - Auto-detects emulator `production` vs `stub` status from vendor source files rather than stale hardcoded dictionaries.
- **`tools/guardian/core/ast_linter.py` & `scripts/benchmark_repo.py`**:
  - Excluded `.venv`, `tools/zig`, `scratch`, `build`, `assets`, `roms`, `arduino-cli-bin`, `resources` from recursive file scans, accelerating repo indexing and AST linting by >10×.
  - Added new hardware/embedded AST guardrails (detecting unguarded `tft.` calls outside `SpiArbiter`, unaligned 32-bit pointer casts, and preprocessor nesting tracking).
- **`tools/guardian/cli.py`**:
  - Added weak-model harness commands (`doctor`, `explain`, `verify`) for one-command orientation, file dependency/symbol inspection, and fast verification.
- **`tests/` & `scripts/` Portability**:
  - Fixed `NESEmu::destroy()` -> `NesEmu::destroy()` typo in `tests/test_tier1_validation.py`.
  - Replaced hardcoded `Path(r"E:\BMO Gameboy")` and user profile paths across all tests and scripts with dynamic repo-root resolution.
### Verification
- `python scripts/validate_repo.py`: `VALIDATION PASSED in 36329 ms — all phases clean` (`Flash: 5,605,280 / 16,777,216 bytes (33.4%)`, `SRAM: 215,200 / 327,680 bytes (65.7%)`).
- `python -m tools.guardian verify`: `Ran 96 tests in 7.429s` / `OK` (`[PASS] AST Linter: 0 Critical violations (11 warnings)`).
- `python -m tools.guardian doctor`: `DOCTOR RESULT: HEALTHY (0 blocking issues)`.
- `python -m tools.guardian cppcheck`: `Cppcheck findings: 46 total, 0 errors.` (exit code 0).

---

## [Agent Handoff] - 2026-10-21
### Changed
- Updated `.agents/rules/04_known_issues.md` with a current handoff, the AI-environment project goal, exact previously recorded verification, open triage tasks, corrected repository metrics, and preserved worktree/generated-index notes.
- Updated `.agents/rules/42_quality_of_life_and_roadmap_ledger.md` to prioritize AI development-environment reliability and distinguish feature availability from verification evidence.
- Updated `.agents/rules/31_quick_start_primer.md` so new agents can route AI-environment and Guardian work to the relevant rules and roadmap.
### Verification
- Documentation-only update; preceding code verification outputs are recorded in the handoff as prior-session results, not rerun here.

## [Fixes & Audit] - 2026-10-21
### Fixed
- Corrected DOOM's 320x200 indexed framebuffer allocation to use `sizeof(pixel_t)`, reducing its PSRAM request by 192,000 bytes.
- Added invalid map-object bounds guards and allocation-failure handling for the DOOM WAD checksum list.
- Updated Guardian Cppcheck to scan all firmware source directories, report timeout/tool failures, ignore informational messages, and propagate non-zero command results. Removed the comment-only per-frame-filter false positive.
### Verification
- Firmware compile: `Sketch uses 5601544 bytes (33%) of program storage space.` Global variables: `215200 bytes (65%)`.
- Guardian tests: `Ran 10 tests in 3.879s` / `OK`; repository tests: `Ran 85 tests in 7.546s` / `OK`.
- Guardian AST audit: `0 Critical, 11 Warnings`. Full-source Cppcheck: `Cppcheck findings: 60 total, 4 errors.` Remaining static-analysis errors and DOOM teardown remain open; no hardware behavior is claimed.
- Repository validator: `VALIDATION PASSED in 16555 ms` (all phases clean).

---

## [Docs] - 2026-10-21 (Unevaluated UI Framework Backlog Note)
### Added
- `docs/research_embedded_ui_frameworks_unevaluated.md`: records that LVGL, Tactility, and MicroPythonOS were never evaluated or borrowed from, lists preliminary concerns as hypotheses, and gives an evaluation plan. No firmware changes.

---

## [Milestone 17.2] - 2026-10-09 (Milestone 5.2.2: Sega Genesis / Mega Drive 68000 CPU & VDP Core Integration in PSRAM)
### Added
- **Sega Genesis / Mega Drive Real Engine Bringup (`src/vendor/genesis/genesis.c`, `src/vendor/genesis/genesis.h`)**:
  - Replaced architectural blank-screen stub with functional Motorola 68000 16/32-bit CISC CPU interpreter running at 7.67 MHz NTSC clock.
  - Implemented 68000 instruction decoding: data movement (`MOVE`, `MOVEA`, `MOVEQ`, `MOVEM`, `LEA`, `PEA`), arithmetic (`ADD`, `SUB`, `CMP`, `CLR`, `NEG`, `SWAP`), logic/bit tests (`AND`, `OR`, `EOR`, `NOT`, `TST`, `BTST`, `BSET`, `BCLR`), branches (`BRA`, `BSR`, `Bcc` all 14 conditions, `DBcc`), subroutines (`JMP`, `JSR`, `RTS`, `RTE`), and system controls (`STOP`, `NOP`).
  - Implemented 24-bit memory bus: Cartridge ROM ($000000–$3FFFFF), Z80 RAM ($A00000–$A01FFF), I/O Ports ($A10000–$A1001F) with TH multiplexer controller support, VDP Ports ($C00000–$C00007), and 64KB Work RAM ($E00000–$FFFFFF).
  - Implemented Yamaha YM7101 / 315-5313 VDP with Plane A (Scroll A), Plane B (Scroll B), Window, and hardware sprite generator (80 sprites, linked list, sizes 1×1 to 4×4 cells).
  - Implemented 9-bit RGB CRAM color conversion with 64-entry pre-swapped BGR565 palette streaming directly to ST7789 display.
  - 100% Octal PSRAM memory allocation (`GENESIS_MALLOC`), consuming 0 bytes of internal DRAM.
  - Proper resource cleanup via `genesis_destroy()` invoked in `GenesisEmu::destroy()`.
- **Master Platform Test Suite & Manifest Synchronization**:
  - Created `tests/test_genesis_emu.py` (7 unit tests) verifying vector table extraction, 68000 memory bus decoding, VDP CRAM color math, VDP register command decoding, TH pin multiplexing, PSRAM memory budget (< 300 KB), and opcode table masks.
  - Promoted `emu_genesis` from `STUB_ENGINES` to `PRODUCTION_ENGINES` in `tests/test_all_tiers_validation.py`.
  - Updated `AGENT_MANIFEST.json` and `docs/software-design-document.md` to classify Sega Genesis as a production core (`fixed_unverified`).
  - Total test suite expanded to **78/78 tests passing**.
### Verified
- `python scripts/validate_repo.py` → PASS (All 7 phases clean, Flash 33.4%, SRAM 65.4%).
- `python -m unittest discover tests` → 78/78 tests OK.

---
