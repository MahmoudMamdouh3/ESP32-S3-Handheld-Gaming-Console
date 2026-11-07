# Archived known-issue history (source archived 2026-11-07)

## Storage Audit Handoff — 2026-11-03

- The local `games/` directory contains 31,326 firmware-recognized files (~16.1 GB); the attached FAT32 `GAMELIB` volume currently has 11,184 matching ROMs. 20,142 source ROMs are absent, and two same-name Game Boy Color files have different sizes than their local source versions. Do not claim the full library is deployed or replace these user files without permission.
- The SD card has 15,677 box-art assets. Their raw sizes and BMP headers meet the firmware decoder's accepted formats, and SHA-256 comparison matched all corresponding files in `roms/boxart`. 12,659 source art files are not on the card. The folder placement (`/boxart/` plus ROMs at root) is recognized by the firmware.
- Local root art in `roms/boxart/` contains 14,168 exact-size raw images and 14,168 BMPs with 64×64, 24-bit BI_RGB headers; no invalid files were found. Every filename in `sdcard/boxart/` exists in that source, but the staging folder has 112 fewer assets.
- Windows reports the volume healthy; a complete read scan found no I/O errors or zero-byte files. `chkdsk F:` could not start because this environment lacks permission, so a filesystem-level repair scan is not verified. The card currently has no `/.bmo_index`; firmware will perform its full initial scan and create the cache.
- `scripts/audit_all_games.py` uses shallow format heuristics and flagged 26 candidate files; treat them as unverified, not confirmed corrupt. It does not exercise every firmware emulator. PICO-8 handling is only a graphics/data plus custom draw-bytecode runtime, not a Lua interpreter; `.p8.png` is not supported despite older documentation.
- `scripts/deploy_to_sd.py` now defaults to dry-run and supports sequential staged/checksummed writes only with explicit `--apply` to an existing `GAMELIB` volume. It invalidates the derived index and does not delete extra files. No ROM or cover-art data is to be committed to Git.

## Previous Handoff — 2026-11-03 (Firmware-Backed Wokwi Hardware Simulation)

## Previous Handoff — 2026-11-03 (AI Environment Overhaul, 11-Core Native C Harness & MCP Server)

This was the previous repository handoff.

- **Box art and simulator parity (2026-11-03):** 61 original box arts are downloaded by `scripts/fetch_covers.py`; titles outside that catalog still show the console badge placeholder. The browser library screen follows `drawGameSelectMenu()`, while all browser-rendered screens remain a hand-written preview rather than firmware execution. The Wokwi project runs the compiled firmware with mapped GPIO controls; Wokwi's available ILI9341 display model is not the physical ST7789 controller. Only freely distributable ROMs are bundled; commercial ROM sets are not downloaded by the repository tooling.
- **AI Development Environment & Native Verification Upgrades (`tools/guardian/`, `tests/`, `.agents/rules/`, `agnes.c`):**
  1. **Eliminated Split-Brain Rule Contradictions & Bug-Fix Prohibitions (`.agents/rules/*.md`, `tools/guardian/core/doc_verifier.py`):**
     - Fixed the copy-paste compile command in `.agents/rules/06_verification_standards.md:110` from `FlashMode=opi` (which bootloops the Quad-SPI Flash ESP32-S3-N16R8) to `CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi`.
     - Removed the rule in `.agents/rules/07_task_protocol.md` and `.agents/rules/12_extensibility_contract.md` that forbade agents from fixing out-of-scope bugs (`"do NOT silently fix it inline"`), replacing it with a proactive defect resolution rule.
     - Synchronized all stale symbol and state names across `.agents/rules/02_architecture.md`, `03_conventions.md`, `10_symbol_reference.md`, `12_extensibility_contract.md`, `26_emulator_exit_contract.md`, `27_codebase_map.md`, `30_common_agent_mistakes.md`, `31_quick_start_primer.md`, `35_bmo_face_contract.md`, and `.agents/rules/README.md` (`destroyActiveEmulator()`, `STATE_CONSOLE_MUSEUM`, `streamGBFrame()` / `streamRawFrameAsync()`, `SDCard::freeRom(currentRom)`, `Slots 1–5`, `/favorites.txt`, 14 cores, 5 baked ROMs).
     - Upgraded `DocTruthVerifier` (`tools/guardian/core/doc_verifier.py`) to scan all active `.agents/rules/*.md` files and reject unguarded `FlashMode=opi` flags and obsolete symbol references.
  2. **Made `agnes.c` Host-Portable & Upgraded Native C Test Harness to 11 Cores + Save-State Determinism (`agnes.c`, `tools/host_cores_runner.c`, `tests/test_native_cores_execution.py`):**
     - Guarded ESP-IDF headers and macros in `firmware/BmoGameboy/src/vendor/agnes/agnes.c` (`#if defined(ESP32) || defined(ARDUINO)`) so the 2,600-line NES 6502 + PPU + Mapper 0/1/2/4 engine compiles natively on x64 `cl.exe` / `gcc`.
     - Added `test_nes_agnes_core()` to `tools/host_cores_runner.c` and `tests/test_native_cores_execution.py`, testing 6502 instruction execution, PPU rendering, `$6000` `agnes_get_prg_ram()` persistence, and bit-exact save-state determinism (`save @ frame 25 -> run to 50 -> restore @ 25 -> re-run to 50 -> assert FNV-1a framebuffer hash and PRG-RAM match 100%`).
  3. **Added LLVM `clang-tidy` Analyzer & Guardian Model Context Protocol (MCP) Server (`tools/guardian/core/clang_tidy_runner.py`, `tools/guardian/mcp_server.py`, `.mcp.json`):**
     - Added `ClangTidyRunner` (`python -m tools.guardian clang-tidy`) running LLVM `clang-tidy` embedded defect checks across all 11 host-portable C cores and `tools/host_cores_runner.c` (`0` warnings/errors).
     - Added a zero-dependency JSON-RPC 2.0 stdio MCP server (`python -m tools.guardian mcp` and `.mcp.json`) exposing `bmo_prompt_pack`, `bmo_lookup`, `bmo_explain`, `bmo_patch_check`, `bmo_run_host_cores`, `bmo_memory_budget`, and `bmo_doctor`.

## Previous Handoff — 2026-11-03 (Pre-Flash Hardware Safety & Runtime Audit Fixes)

This is the current repository handoff. Read this section before continuing from an earlier changelog entry.

- **Pre-Flash Firmware Audit & Runtime Fixes (`firmware/BmoGameboy/`):**
  1. **Eliminated Stack Overflow Panics in `loadState()` (`emu_walnut.cpp`, `emu_peanut.cpp`, `emu_sms.cpp`, `emu_pce.cpp`):**
     - Replaced local stack allocations of `struct gb_s` (~50 KB in Walnut, ~17 KB in Peanut), `sms_context_t` (~56.5 KB in SMS/GG), and `pce_context_t` (~76 KB in PCE) inside `loadState()` with Octal PSRAM allocations via `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` + `heap_caps_free(...)`. Previously, invoking Quick Load (`SELECT + B` or Pause Menu -> Load State) on these cores overflowed the 8 KB `loopTask` stack on ESP32-S3.
  2. **Fixed PC Engine Battery Backup RAM Persistence (`emu_pce.cpp`):**
     - Updated `PceEmu::saveBatteryRam` and `PceEmu::loadBatteryRam` to persist the 2 KB non-volatile Backup RAM (`pce_ctx->bram`, MPR page `$F7`) rather than the 8 KB volatile work RAM (`pce_ctx->ram`, MPR page `$F8`).
  3. **Fixed DOOM WAD VFS Path Resolution, Header Pre-Validation, BGR565 Palette & SPI Bus Locking (`emu_doom.cpp`, `display_emu.cpp`, `m_misc.c`, `d_iwad.c`, `i_system.c`):**
     - Normalized `wadPath` in `DoomEmu::begin()` into a persistent static buffer with the ESP-IDF `/sd/` VFS mount prefix required by C `fopen()`, replacing the static `argv` initializer that pointed to a caller stack buffer and lacked the `/sd` prefix.
     - Added pre-launch `fopen` + 12-byte `IWAD`/`PWAD` header validation under `SpiArbiter::lock()` so missing or corrupt `.wad` files return `false` to the menu UI instead of triggering `I_Error` -> `exit(-1)` reboot panics.
     - Wrapped `M_MakeDirectory`, `M_FileExists`, `M_WriteFile`, and `M_ReadFile` in `m_misc.c` with `DOOM_SPI_LOCK()` / `DOOM_SPI_UNLOCK()` to prevent SPI2 bus collisions with Core 0 display transfers, guarded `ZenityErrorBox` in `i_system.c` on ESP32/Arduino, and guarded `BuildIWADDirList()` in `d_iwad.c`.
     - Fixed `s_doomPalette` packing in `display_emu.cpp` from RGB565 to byte-swapped BGR565 (`((c.b & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.r >> 3)`) so red and blue channels are not swapped on the ST7789 panel.
  4. **Fixed Game Gear 160×144 Viewport Stride Extraction (`display_emu.cpp`):**
     - Updated `DisplayEmu::streamSMSFrame()` when `isGameGear` is `true` to extract the 144 scanlines of 160 pixels centered at `(x=48, y=24)` from the 256-pixel-stride VDP framebuffer before streaming to the display.
  5. **Fixed In-Game BMP Screenshot Capture for 8-Bit Paletted Cores (`display_emu.cpp`):**
     - Tracked the active frame format (`FRAME_FMT_RAW16`, `FRAME_FMT_NES8`, `FRAME_FMT_DOOM8`) in `DisplayEmu` and converted 8-bit NES (`256×240`) and DOOM (`320×200`) index buffers on-demand in `DisplayEmu::getLastFramebuffer()` so Pause Menu screenshots (`SELECT`) produce valid BGR565 images across all 14 cores.
  6. **Complete Multi-Buffer Save State Serialization & Atomic Restore (`emu_colem.cpp`, `emu_wswan.cpp`, `emu_snes.cpp`, `emu_genesis.cpp`):**
     - Included VDP VRAM and auxiliary RAM in save states (`ColemEmu`: 8 KB RAM + 16 KB VRAM; `WSwanEmu`: 64 KB IRAM + 64 KB SRAM; `SNESEmu`: 128 KB WRAM + 64 KB VRAM + 32 KB SRAM; `GenesisEmu`: 64 KB 68K RAM + 64 KB VRAM + 8 KB Z80 RAM + 128 B CRAM + 80 B VSRAM) via temporary PSRAM staging buffers so failed loads never clobber live memory before CRC verification.
  7. **Fixed Favorites Unfavoriting Persistence & Negative-X Box Art Clipping (`sd_card.cpp`, `box_art.cpp`):**
     - Reset `isFavorite = false` and `s_favoritesCount = 0` before applying `/favorites.txt` in `SDCard::loadFavorites()` so unfavoriting default baked ROMs or indexed ROMs persists across reboots, updated baked ROM `hasBoxArt` flags on SD mount, and fixed horizontal source offset clipping when `x < 0` in `BoxArt::draw()`.
