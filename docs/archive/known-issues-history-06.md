# Archived known-issue history (source archived 2026-11-07)

## [OPEN-QUESTION-UI-FRAMEWORKS] - 2026-10-21 - LVGL / Tactility / MicroPythonOS Never Evaluated
- OPEN: The custom UI was never compared against LVGL, Tactility, or MicroPythonOS, and no ideas were borrowed from them. Not adopting them is unverified, not a documented decision.
- NEXT: See `docs/research_embedded_ui_frameworks_unevaluated.md` for hypotheses and an evaluation plan (budget check, then isolated LVGL prototype, then recorded decision). No firmware changes made.

## [PERF-DOOM-FRAMEBUFFER] - 2026-10-21 - DOOM Indexed Framebuffer Overallocated
- FIXED_UNVERIFIED: With `CMAP256`, `pixel_t` is `uint8_t`; the 320x200 screen buffer now allocates `width * height * sizeof(*DG_ScreenBuffer)`, reducing its request from 256,000 to 64,000 bytes (192,000 fewer PSRAM bytes).
- EVIDENCE: Arduino compile output: `Sketch uses 5601544 bytes (33%) of program storage space.` Runtime behavior and physical hardware remain unverified.

## [DOOM-INPUT-GUARDS] - 2026-10-21 - Invalid Map Types and WAD List Growth
- FIXED_UNVERIFIED: `P_RespawnSpecials` and `P_SpawnMapThing` now stop before indexing `mobjinfo` for an unknown map type. WAD checksum file-list growth now reports allocation failure without losing the previous list pointer.
- EVIDENCE: Arduino compile output: `Global variables use 215200 bytes (65%) of dynamic memory, leaving 112480 bytes for local variables.` Full-source Cppcheck no longer reports the two `p_mobj.c` indexes or `w_checksum.c` realloc at the reviewed locations.

## [GUARDIAN-SCAN-RESULTS] - 2026-10-21 - Full Firmware Static Scan Triage
- VERIFIED_HOST: Guardian Cppcheck scans `firmware/BmoGameboy/src`, uses a bounded 600-second timeout, filters informational branch-limit notes, surfaces execution failures, and propagates command exit status. All 4 previously reported Cppcheck errors (`i_system.c`, `am_map.c`, `p_switch.c`) were fixed and verified (`Cppcheck findings: 54 total, 0 errors`).

## [RESOLVED-DOOM-TEARDOWN] - 2026-11-03 - DOOM Core Session Teardown Implemented
- RESOLVED: `DoomEmu::destroy()` now invokes `doomgeneric_Destroy()`, which closes all open WAD files via `W_Shutdown()` (`W_CloseFile`), frees `lumpinfo`, releases the 64 KB `DG_ScreenBuffer`, frees the 6 MiB `s_zone_block` in `I_ShutdownZone()`, and resets `first_ call` / `wadfiles` state for clean re-entry.

## [RESOLVED-WSWAN-CORE-STATUS] - 2026-11-03 - WonderSwan Core Host-Verified
- VERIFIED_HOST: `wswan.c` banking (`bank_rom[0..3]`, `bank_sram`, `$F0000` reset vector alias), port `0xC1` handler order, active-high keypad scan (`0xB5`), and `emu_wswan.cpp` `START` bit (`0x0080`) were fixed and behaviorally verified via the native C harness `tools/host_cores_runner.c` (`test_real_c_cores_host.py`).

## [PHASE-5.1-UNIFIED-CONTRACT-PCE] - 2026-10-09 - Unified Core Contract & PC Engine Persistence
- RESOLVED: Unified `EmulatorCoreContract` standardized across all platform engines (begin, updateJoypad, runFrame, destroy, saveState, loadState, saveBatteryRam, loadBatteryRam).
- EXPANDED CORE IDENTIFIERS: `SaveManager::CoreId` enum expanded with all 14 console platform identifiers (`CORE_PCE = 6`, `CORE_ATARI = 7`, `CORE_PICO8 = 8`, `CORE_GENESIS = 9`, `CORE_SNES = 10`, `CORE_WSWAN = 11`, `CORE_NGP = 12`, `CORE_LYNX = 13`, `CORE_COLEM = 14`).
- PCE PERSISTENCE: `PceEmu` fully wired with battery RAM backup and 5-slot snapshot support (`saveBatteryRam`, `loadBatteryRam`, `saveState`, `loadState`), auto-save on exit, quick-save, quick-load, and transparent boot auto-load.
- VERIFIED: validate_repo.py PASSED (all 7 phases clean, Flash 33.1%, SRAM 75.2%). unittest 52/52 OK. Guardian audit clean.
---

## [PHASE-4-INDEX-BOXART] - 2026-10-09 - High-Speed Binary ROM Index Cache & 64×64 Box Art Cover Engine
- RESOLVED: Binary ROM Index Cache (`.bmo_index`) with 36-byte header (`BMOIDX01`), 72-byte entries, and CRC32 payload checksum.
- SPEEDUP: Boot-time catalog enumeration latency reduced from ~3,500 ms to < 15 ms (>200× acceleration).
- CATALOG ORDER: Case-insensitive alphabetical sorting (`sortRoms()`) active across all SD collections for instant A–Z jumping.
- COVER ART: `BoxArt` engine supporting 64×64 raw 16-bit and 16/24-bit BMP images in 8KB PSRAM buffer with zero DRAM consumption.
- THREAD SAFETY: All SD card index and box art I/O protected under `SpiArbiter` mutex lock.
- Verified: validate_repo.py PASSED (all 7 phases clean, Flash 33.1%, SRAM 75.2%). unittest 51/51 OK. Guardian audit 0 Critical.
---

## [PHASE-3-SAVES] - 2026-10-09 - Non-Volatile Multi-Slot Save States & Cartridge Battery RAM Persistence
- RESOLVED: Non-volatile cartridge battery RAM (.sav) and 5-slot emulator state snapshots (.s01-.s05) implemented in `SaveManager`.
- THREAD-SAFETY: All save/load operations protected by `SpiArbiter` mutex lock (`spiBusMutex`) preventing corruption from Core 0 display DMA transfers.
- INTEGRITY: 60-byte binary header with `BMOSS01` magic and CRC32 payload validation prevents loading truncated or corrupted states.
- IN-GAME UI: Quick pause modal menu (`STATE_PAUSE_MENU`) with hotkeys (`SELECT+START` menu, `SELECT+A` save, `SELECT+B` load, `SELECT+UP` save&quit).
- Verified: validate_repo.py PASSED (all 7 phases clean, Flash 33.1%, SRAM 75.2%). unittest 36/36 OK. Guardian audit 0 Critical.
---

## [PERF-Session-2] - 2026-09-18 - 10 Performance Fixes (heap_caps_free + O3 pragmas)
- CRITICAL: heap_caps_free() fix in 8 emulator destroy() functions (peanut, walnut, genesis, snes, wswan, ngp, lynx, colem)
  - All had heap_caps_malloc(MALLOC_CAP_SPIRAM) paired with plain free() — allocator mismatch
- NON-CRITICAL: Added #pragma GCC optimize("O3,unroll-loops") to bmo_face.cpp (SDF renderer ~468-1241µs hot path)
- NON-CRITICAL: Completed display_emu.cpp pragma from O3 -> O3,unroll-loops
Verified: guardian audit PASS (0 Critical). validate_repo.py PASS. unittest 32/32 OK.
---

## [PERF-Session] - 2026-09-18 - 15 Performance Issues Resolved
- C-1: getFavoritesCount O(N)->O(1) s_favoritesCount counter (sd_card.cpp)
- C-2: visibleGames[] pointer-copy guarded by dirty flag (BmoGameboy.ino)
- H-1: drawFittedCentered strlen-in-loop -> int len (display_emu.cpp)
- H-2: DG_GetKey 256-entry scan -> O(1) 16-slot ring buffer (emu_doom.cpp)
- H-3: Arduino String heap allocs -> stack char[] (sd_card.cpp)
- H-4: sdfEllipse 2x sqrtf -> 1x (~80K cycles/render saved) (bmo_face.cpp)
- H-5: BmoFace::update() guarded against STATE_EMULATOR (BmoGameboy.ino)
- M-1: millis() cached per canPress() block (BmoGameboy.ino)
- M-3: consoleCountsDirty file-scope; toggleFavorite invalidates badge (BmoGameboy.ino)
- M-4: DG_SleepMs() logs warning on Doom pacing (emu_doom.cpp)
- M-5: Boot delay conditional on CORE_DEBUG_LEVEL>0 (BmoGameboy.ino)
- L-1: consoles[16] static const (display_emu.cpp)
- L-3: SMS updateJoypad double-poll safety comment (emu_sms.cpp)
- L-5: String::trim fixed as part of H-3 (sd_card.cpp)
Verified: validate_repo.py PASSED. Flash 33.0%. SRAM 75.1%. unittest 32/32.
---

## 1. Ground-Truth Hardware State & Hard Stops

The physical soldered hardware configuration (`src/core/config.h` & `.agents/rules/01_hardware.md`) is the ground truth. Any software assuming missing hardware components will fail or crash the MCU.

| Component / Subsystem | Physical Soldered State | Software Constant / Rule | Operating Constraint |
| :--- | :--- | :--- | :--- |
| **Battery Divider (GPIO1)** | ❌ NOT SOLDERED | `FEATURE_BATTERY_MONITOR = 0` | **HARD STOP:** Never enable or read GPIO1. Floating ADC pin causes erratic voltages and kernel bootloops. |
| **I2S Audio DAC (MAX98357A)**| ❌ NOT SOLDERED | `FEATURE_AUDIO = 0` | **HARD STOP:** Never initialize I2S peripheral or write audio buffers. Output will hang DMA channels. |
| **Flash & PSRAM Interface** | ✅ SOLDERED (QIO Flash / OPI PSRAM) | `FlashMode=qio, PSRAM=opi` | **HARD STOP:** Never switch `FlashMode` to `opi` (Flash chip is Quad SPI; `opi` causes black screen/boot panic). Never switch `PSRAM` to `qpi`. |
| **Display (ST7789VW 2.4")** | ✅ SOLDERED (SPI mode) | `TFT_CS=10, TFT_DC=8, TFT_RST=9` | Shared FSPI bus (`TFT_MOSI=11, TFT_SCK=12`) with SD card. Driven at 80 MHz with explicit `SpiArbiter` locks. |
| **MicroSD Card Slot** | ✅ SOLDERED (SPI mode) | `SD_CS=13, SD_MISO=15, SPI=25MHz` | Shares FSPI `MOSI(11)` and `SCK(12)`. CS assertion and SPI transactions must be serialized via `SpiArbiter::lock()`. |
| **Tactile Buttons (8x GPIO)**| ✅ SOLDERED (Active LOW)| `UP=4, DN=5, LT=6, RT=7, A=16, B=17, START=18, SEL=21` | Sampled via single-cycle atomic `REG_READ(GPIO_IN_REG)`. Never use strapping pins `0, 3, 45, 46` or PSRAM pins `33-37`. |

---

## 2. Hardware & Electrical Limitations (HARDWARE-01 to HARDWARE-05)

### HARDWARE-01: Shared SPI Bus Contention (Display vs SD Card)
- **Status:** `MITIGATED (SPI_ARBITER_MUTEX)`
- **Description:** The ST7789 display controller and MicroSD card share the exact same hardware SPI peripheral (FSPI: MOSI GPIO11, MISO GPIO15, SCK GPIO12; TFT_CS GPIO10, SD_CS GPIO13).
- **Limitation:** The MCU cannot read ROM/WAD data from the SD card while Core 0 is streaming frame pixels to the display without bus serialization.
- **Remediation / Mitigation:** Protected by recursive FreeRTOS mutex `SpiArbiter::lock()` across Core 0 display worker, `SDCard`, `SaveManager`, `BoxArt`, and DOOM `w_file_stdc.c` WAD reads.

### HARDWARE-02: Floating ADC on GPIO1 (Battery Sense Line)
- **Status:** `MITIGATED (GUARDRAIL_ENFORCED)`
- **Description:** GPIO1 is designated as the analog battery voltage sensing line in the schematic, but the physical 100k/100k voltage divider is not populated on the perfboard.
- **Limitation:** Reading `analogRead(BATTERY_PIN)` returns floating noise, which causes low-voltage panic handlers to trigger false shutdowns and display sleep cycles.
- **Remediation / Mitigation:** Enforced by static AST linter rule `HARD_STOP_BATTERY_NONZERO` and CI Phase 2 guardrails. `FEATURE_BATTERY_MONITOR` must remain `0`.

### HARDWARE-03: Floating I2S Audio Bus
- **Status:** `MITIGATED (GUARDRAIL_ENFORCED)`
- **Description:** The MAX98357A I2S DAC is not populated on the perfboard.
- **Limitation:** Enabling I2S clock generators and DMA buffers wastes internal DMA descriptors and causes audio FIFO underflow interrupts that consume CPU cycles.
- **Remediation / Mitigation:** Enforced by static AST linter rule `HARD_STOP_AUDIO_NONZERO` and CI Phase 2 guardrails. `FEATURE_AUDIO` must remain `0`.

### HARDWARE-04: ESP32-S3 Strapping & Octal PSRAM Reserved Pins
- **Status:** `VERIFIED_HARDWARE`
- **Description:** GPIOs `0, 3, 45, 46` are ESP32-S3 boot strapping pins, and GPIOs `33–37` are reserved internally for Octal SPI PSRAM.
- **Limitation:** Wiring buttons or peripherals to strapping pins alters boot mode; touching GPIOs 33–37 crashes the PSRAM bus.
- **Remediation / Mitigation:** All 8 buttons are wired to safe GPIOs (`UP=4, DOWN=5, LEFT=6, RIGHT=7, A=16, B=17, START=18, SELECT=21`).

### HARDWARE-05: Dual-Core FreeRTOS Display & Emulation Pipeline
- **Status:** `RESOLVED_OPTIMIZED`
- **Description:** Core 1 runs `loop()` (emulation, button polling, UI canvas drawing) while Core 0 runs `BMO_Display` (`displayWorkerTask`) streaming double-buffered frames over 80 MHz FSPI under `SpiArbiter::lock()`.

---
