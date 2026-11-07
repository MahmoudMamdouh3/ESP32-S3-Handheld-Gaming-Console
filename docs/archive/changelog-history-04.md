# Archived changelog (history archived 2026-11-07)

## [AI Development Environment Overhaul, 11-Core Native C Harness & MCP Server] - 2026-11-03
### Added
- **Model Context Protocol (MCP) Server for BMO Guardian (`tools/guardian/mcp_server.py`, `.mcp.json`, `tools/guardian/cli.py`)**:
  - Implemented a zero-dependency JSON-RPC 2.0 stdio MCP server (`python -m tools.guardian mcp`) exposing 7 structured tools (`bmo_prompt_pack`, `bmo_lookup`, `bmo_explain`, `bmo_patch_check`, `bmo_run_host_cores`, `bmo_memory_budget`, `bmo_doctor`) and added `.mcp.json` at the repository root for automatic IDE and agent discovery.
- **LLVM Clang-Tidy Deep Static Analyzer Integration (`tools/guardian/core/clang_tidy_runner.py`, `python -m tools.guardian clang-tidy`)**:
  - Integrated `clang-tidy` with embedded C/C++ defect checks (`clang-analyzer-core.*`, `clang-analyzer-unix.Malloc`, `clang-analyzer-cplusplus.NewDelete`, `bugprone-assert-side-effect`, `bugprone-infinite-loop`, `bugprone-sizeof-expression`, `bugprone-string-constructor`, `bugprone-suspicious-memset-usage`, `bugprone-use-after-move`) across all 11 host-portable C emulator cores and `tools/host_cores_runner.c`.
- **Host-Portable `agnes.c` (NES 6502 + PPU + Mapper 0/1/2/4) & Save-State Determinism Harness (`firmware/BmoGameboy/src/vendor/agnes/agnes.c`, `tools/host_cores_runner.c`, `tests/test_native_cores_execution.py`)**:
  - Guarded ESP-IDF headers and attributes (`esp_heap_caps.h`, `esp_attr.h`, `IRAM_ATTR`, `__attribute__((aligned(32)))`) in `agnes.c` with `#if defined(ESP32) || defined(ARDUINO)` so the 2,600-line NES engine compiles natively on x64 `cl.exe` and `gcc`.
  - Upgraded `tools/host_cores_runner.c` and `tests/test_native_cores_execution.py` from 10 to 11 native C cores (575 frames total) and added `test_nes_agnes_core()` verifying 6502 instruction execution, PPU rendering, `$6000` `agnes_get_prg_ram()` persistence, and bit-exact save-state determinism (`save @ frame 25 -> run to 50 -> restore @ 25 -> re-run to 50 -> assert FNV-1a framebuffer hash and PRG-RAM match 100%`).

### Fixed
- **Eliminated Split-Brain Contradictions & Harmful Rules Across `.agents/rules/` (`tools/guardian/core/doc_verifier.py`, `.agents/rules/*.md`)**:
  - Fixed the boot-bricking `FlashMode=opi` copy-paste compile command in `.agents/rules/06_verification_standards.md:110` to `FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi`.
  - Removed the rule in `.agents/rules/07_task_protocol.md` and `.agents/rules/12_extensibility_contract.md` that forbade agents from fixing bugs discovered outside their immediate prompt, replacing it with a proactive defect resolution rule.
  - Corrected obsolete symbol and state names across `.agents/rules/02_architecture.md`, `03_conventions.md`, `10_symbol_reference.md`, `12_extensibility_contract.md`, `26_emulator_exit_contract.md`, `27_codebase_map.md`, `30_common_agent_mistakes.md`, `31_quick_start_primer.md`, `35_bmo_face_contract.md`, and `.agents/rules/README.md` (`teardownCurrentEmulator` -> `destroyActiveEmulator`, `STATE_CONSOLE_SPECS` -> `STATE_CONSOLE_MUSEUM`, `submitFrameAsync` -> `streamGBFrame`/`streamRawFrameAsync`, `Slots 0-4` -> `Slots 1-5`, `/.favorites.txt` -> `/favorites.txt`).
  - Expanded `DocTruthVerifier` to scan all active `.agents/rules/*.md` files and reject unguarded `FlashMode=opi` commands and obsolete symbol references.

---

## [Pre-Flash Hardware Safety & Runtime Audit Fixes] - 2026-11-03
### Fixed
- **Game Select Menu Visibility, UTF-8 CP437 Font Corruption & Theme Token Compliance (`src/core/display_emu.cpp`, `src/core/bmo_face.cpp`, `BmoGameboy.ino`, `tools/guardian/core/ui_auditor.py`)**:
  - Fixed invisible `UI_BLACK`-on-`UI_BLACK` text (`#102624` on `#102624`) for the selected game title (`y = 180`), `"NO FAVORITES YET!"`, and `"NO GAMES"` in `DisplayEmu::drawGameSelectMenu()` by rendering primary titles in `UI_WHITE` and secondary subtitles/counters in `UI_MINT`, and upgraded the top-left header icon to use `BmoFace::renderToBuffer()`.
  - Replaced multibyte UTF-8 characters (`★` `\u2605`, `♪` `\u266a`) across `BmoGameboy.ino`, `display_emu.cpp`, and `bmo_face.cpp` with 7-bit ASCII (`*`, `~`) so Adafruit GFX 5×7 CP437 and `FreeSans9pt7b` fonts do not render multi-byte garbage on the ST7789 display.
  - Replaced raw un-swapped hex color literals (`0xFFE0`, `0x7BEF`) in `BmoGameboy.ino` `DisplayEmu::showToast()` calls with `Theme::BMO_DPAD_YELLOW` and `Theme::BMO_BODY_TEAL`, cleared `pauseToast` after `drawPauseMenu()` so repeated actions re-trigger toasts, and added `BmoGameboy.ino`, `showToast`, and `NON_ASCII_UI_TEXT` checks to `UiAuditor`.
- **Atari Lynx Boot Image Protection & NES 8 KB PRG-RAM `.sav` Persistence (`src/emulators/emu_lynx.cpp`, `src/emulators/emu_nes.cpp`, `src/vendor/agnes/agnes.h`, `src/vendor/agnes/agnes.c`, `src/core/save_manager.cpp`)**:
  - Relocated Atari Lynx 256-byte persistent battery RAM in `LynxEmu::saveBatteryRam`/`loadBatteryRam` to `&lynxRam[0xFE00]` so loading `.sav` on game launch never overwrites the ROM boot code loaded at `$0200`.
  - Added 8 KB cartridge `prg_ram` (`$6000-$7FFF`) support across all mappers in `agnes.c` (`agnes_get_prg_ram()`, 0 extra bytes in `sizeof(agnes_t)` union) and updated `NesEmu::saveBatteryRam`/`loadBatteryRam` to persist only the 8 KB `prg_ram` instead of dumping/restoring a full 86 KB CPU/PPU snapshot on cold boot.
  - Updated `SaveManager::loadState()` to stage both `stateData` and `ramData` in temporary PSRAM buffers and verify the CRC32 checksum before copying into live emulator buffers.
- **Stack Overflow Prevention in `loadState()` (`src/emulators/emu_walnut.cpp`, `emu_peanut.cpp`, `emu_sms.cpp`, `emu_pce.cpp`)**:
  - Replaced stack-allocated `struct gb_s` (~50 KB / ~17 KB), `sms_context_t` (~56.5 KB), and `pce_context_t` (~76 KB) temporary buffers in `loadState()` with Octal PSRAM allocations (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` + `heap_caps_free(...)`) to prevent overflowing the 8 KB `loopTask` stack on ESP32-S3.
- **PC Engine Battery Backup RAM Persistence (`src/emulators/emu_pce.cpp`)**:
  - Updated `PceEmu::saveBatteryRam` and `PceEmu::loadBatteryRam` to save and restore the 2 KB Backup RAM (`pce_ctx->bram`, MPR page `$F7`) instead of volatile main RAM (`pce_ctx->ram`).
- **DOOM WAD VFS Path Resolution, Header Validation, BGR565 Palette & SPI Bus Locking (`src/emulators/emu_doom.cpp`, `src/core/display_emu.cpp`, `src/vendor/doom/src/m_misc.c`, `d_iwad.c`, `i_system.c`)**:
  - Normalized `wadPath` in `DoomEmu::begin()` into a persistent static buffer with the `/sd/` ESP-IDF VFS mount point prefix required by C `fopen()`, and added pre-launch `IWAD`/`PWAD` 12-byte header validation under `SpiArbiter::lock()`.
  - Wrapped SD filesystem operations (`mkdir`, `fopen`, `fread`, `fwrite`, `fclose`) in `m_misc.c` with `DOOM_SPI_LOCK()` / `DOOM_SPI_UNLOCK()`, guarded `ZenityErrorBox` in `i_system.c` on ESP32/Arduino, and guarded `BuildIWADDirList()` in `d_iwad.c`.
  - Fixed `s_doomPalette` packing in `display_emu.cpp` to byte-swapped BGR565 so red and blue channels render accurately on the ST7789 display.
- **Game Gear 160×144 Viewport Stride Extraction & NES/DOOM Screenshot Capture (`src/core/display_emu.cpp`)**:
  - Extracted the centered 160×144 Game Gear LCD viewport `(x=48, y=24)` row-by-row from the 256-stride VDP framebuffer in `DisplayEmu::streamSMSFrame()`.
  - Added on-demand 8-bit paletted-to-BGR565 conversion for NES (`256×240`) and DOOM (`320×200`) in `DisplayEmu::getLastFramebuffer()` so Pause Menu BMP screenshots work across all 14 cores.
- **Multi-Buffer Save States & Atomic Restore (`src/emulators/emu_colem.cpp`, `emu_wswan.cpp`, `emu_snes.cpp`, `emu_genesis.cpp`)**:
  - Included VDP VRAM and auxiliary RAM in save state payloads for ColecoVision, WonderSwan, SNES, and Genesis using PSRAM staging buffers so failed loads do not corrupt live RAM before CRC32 verification.
- **Favorites Persistence & Negative-X Box Art Clipping (`src/core/sd_card.cpp`, `src/core/box_art.cpp`)**:
  - Reset `isFavorite` flags before applying `/favorites.txt` in `SDCard::loadFavorites()` so unfavoriting persists across reboots, updated `hasBoxArt` for baked ROMs on SD mount, and fixed horizontal clipping in `BoxArt::draw()` when `x < 0`.

---

## [Phase F: Design System, Anti-AI-Slop Craft Standards & UI Auditor] - 2026-10-21
### Added
- **Design System Root Specification (`DESIGN.md`)**:
  - Authored comprehensive, single-file design standard synthesizing OnionUI BMO aesthetic and physical handheld console ergonomics (derived from `Awesome DESIGN.md`, `Google DESIGN.md`, `Open Design`, `UI/UX Pro Max Skill`).
  - Defined 1:1 color token matrix between physical ST7789 display (BGR565 byte-swapped for SPI DMA) and Web Simulator CSS custom properties.
  - Specified 8px base grid, 4px sub-grid, touch/pointer minimum targets ($44\times 44\text{ pt}$), typography ladder (`Press Start 2P`, `Outfit`, `JetBrains Mono`), and component interaction states.
  - Formalized second-order harmonic spring-damper motion dynamics ($\zeta = 0.72$, $\omega_n = 22.0\text{ rad/s}$) from `Taste Skill` and `GSAP Skills`.
- **Anti-AI UI Craft Standards (`.agents/rules/44_anti_ai_ui_design_standards.md`)**:
  - Established Rule 44 prohibiting AI visual anti-patterns: card-in-card nesting syndrome, generic purple/indigo gradients, low-contrast text (< 4.5:1 WCAG AA), missing keyboard focus or tactile active states, ungridded margins, and inlined raw hex colors.
  - Re-enforced physical hardware invariants: menu selection states must use `drawRoundRect` outline strokes rather than `fillRoundRect` full fills, saving $6.3\text{ ms}$ per frame.
- **Dedicated UI Design Auditor (`tools/guardian/core/ui_auditor.py`, `python -m tools.guardian ui-audit`)**:
  - Implemented automated static scanner verifying firmware drawing calls for raw inline hexes, simulator CSS for card nesting, missing `:focus-visible` states, and odd off-grid spacing.
  - Verifies exact design token parity between `firmware/BmoGameboy/src/core/theme.h` and `tools/bmo_simulator/simulator.css`.
  - Added unit test coverage in `tools/guardian/tests/test_guardian.py` (`test_17_ui_auditor_clean_baseline`, `test_18_ui_auditor_catches_violations`).
- **CI & Doctor Integration (`scripts/validate_repo.py` Phase 8, `tools/guardian/cli.py`)**:
  - Added Phase 8 (`Phase 8: UI Design System & Anti-AI-Slop`) to repository CI validator `scripts/validate_repo.py`.
  - Added UI audit step to `python -m tools.guardian audit`, `doctor`, and `patch-check`.
  - Updated `scripts/bundle_rules.py` to Ruleset v8 consolidating all 44 rules into `docs/consolidated-rules.md`.

### Enhanced
- **Web Simulator Keyboard Accessibility (`tools/bmo_simulator/simulator.css`)**:
  - Added high-contrast `:focus-visible` styles with 2px offset rings to `.control-pill`, `.dpad-btn`, `.round-btn`, `.pill-btn`, and global form inputs.
- **Firmware Theme Constants (`firmware/BmoGameboy/src/core/theme.h`, `display_emu.cpp`, `bmo_face.cpp`)**:
  - Added `makePanelColor()`, `PANEL_DEEP_TEAL`, `PANEL_DPAD_YELLOW`, `TEXT_SIZE_SMALL`, `TEXT_SIZE_BODY`, `TEXT_SIZE_TITLE`, `TARGET_TOUCH_MIN`, and `TARGET_EMBEDDED_MIN` to `Theme::`.
  - Replaced hardcoded panel colors in `DisplayEmu::showSDCardWarning()` and replaced "Living Mascot" terminology with "Animated Mascot".

---
