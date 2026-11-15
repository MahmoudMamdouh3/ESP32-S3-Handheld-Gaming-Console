# System Architecture

This document describes the architecture of the ESP32-S3 handheld firmware (`firmware/BmoGameboy/`), standalone bring-up sketch (`firmware/bringup/`), host unit tests (`tests/`), browser simulator (`simulator/`), and hardware CAD/layout tooling (`hardware/`, `scripts/`).

---

## 1. Module Boundaries

| Module / Directory | Ownership & Responsibilities | Key Interfaces |
| :--- | :--- | :--- |
| `firmware/BmoGameboy/BmoGameboy.ino` | Arduino sketch entrypoint calling `BmoApp::setup()` and `BmoApp::loop()` | `BmoApp::setup()`, `BmoApp::loop()` |
| `firmware/BmoGameboy/src/app/` | System state machine (`SystemState`), boot SPI safe-mode policy, ROM launch, `SELECT`-chord hotkeys, frame pacing (`FramePacer`), pause quit confirmation | `bmo_app.cpp`, `boot_safe_mode.h`, `core_registry.cpp` |
| `firmware/BmoGameboy/src/hal/` | Pin/feature truth (`config.h`, `pins_check.h`), shared FSPI recursive mutex (`SpiArbiter`), direct-register debounced button polling (`Buttons`), runtime telemetry (`profiler.cpp`), dormant battery scaffold (`battery.cpp`) | `SpiArbiter::ScopedLock`, `Buttons::update()` |
| `firmware/BmoGameboy/src/storage/` | SD mount & FAT32 scan, `BMOIDX03` index (`sd_catalog.cpp`), variable-length PSRAM path arena (`sd_mount.cpp`), `.sav` & `BSS2` save states (`save_manager.cpp`), crash-safe rename/CRC32 (`atomic_fs.h`), `64×64` BGR565 box art (`box_art.cpp`) | `SDCard::begin()`, `SaveManager`, `AtomicFs` |
| `firmware/BmoGameboy/src/ui/` | Core 0 async SPI/DMA worker (`display_worker.cpp`), menu rendering (`display_menus.cpp`), animated BMO mascot (`bmo_face.cpp`), diagnostics screen (`ui_diagnostics.cpp`), token palette (`theme.h`) | `DisplayEmu`, `BmoFace`, `Theme` |
| `firmware/BmoGameboy/src/cores/` | Thin wrappers adapting 14 upstream emulator engines into `CoreRegistry::CoreDescriptor` (video buffer, controller mapping, save-state serialization, `Doom` PSRAM zone sizing) | `CoreRegistry`, `DoomEmu::computeZoneBytes()` |
| `firmware/BmoGameboy/src/third_party/` | Unmodified or minimally adapted upstream cores (`peanut_gb`, `walnut_cgb`, `agnes`, `prboom-go` DOOM, `smsplus`, `huexpress`, `stella`, `fake08`, `gwenesis`, `snes9x`, `oswan`, `ngp`, `handy`, `colem`) — all excluded from default ROMs (`BMO_ENABLE_BAKED_ROMS=0`) | Profile macros `BMO_EXPERIMENTAL_CORES`, `BMO_ENABLE_DOOM` |
| `firmware/bringup/` | Minimal zero-dependency hardware bring-up sketch for first power-on (display color fill, SD mount/file list, live GPIO tester) | `firmware/bringup/bringup.ino` |
| `tests/` & `tools/guardian/` | Host CPU C++17 unit tests, WASI 11-core 50-frame smoke harness, Python repo validators, static AST/PSRAM/bus analyzers | `python tasks.py check`, `python tasks.py test` |
| `simulator/` & `hardware/` | Browser Canvas 2D + WASM UI port, Three.js internal teardown viewer, parametric OpenSCAD/Python enclosure generator, `layout.json` Perfboard & SVG schematics | Simulator, CAD, wiring artifacts |

---

## 2. Boot Sequence (`BmoApp::setup()`)

Every boot executes sequentially on **Core 1** (`firmware/BmoGameboy/src/app/bmo_app.cpp`):

1. **Serial & Version Banner:** Starts UART at `115200` baud (`Serial.setTxTimeoutMs(0)`), waits `500 ms` (`3000 ms` when `CORE_DEBUG_LEVEL > 0`), and logs `BMO_VERSION_STRING` / `BMO_GIT_SHA`.
2. **Reset Reason & Crash Breadcrumb:** Calls `esp_reset_reason()` and translates via `BmoApp::resetReasonName()`. Inspects `RTC_NOINIT_ATTR BootBreadcrumb s_bootBreadcrumb` (`magic == 0x424D4F42`, `"BMOB"`). On abnormal resets (brownout, panic, watchdog), logs the prior `state` and `emulatorIndex`, then refreshes the breadcrumb.
3. **SPI Bus Init:** Calls `SpiArbiter::init()` to create the recursive mutex (`abort()` on allocation failure), then starts FSPI on `TFT_SCK=12`, `SD_MISO=15`, `TFT_MOSI=11`, `SS=-1`.
4. **Input & Boot SPI Safe Mode:** Calls `Buttons::begin()` and `Buttons::update(false)`. `resolveBootDisplaySpiClock()` checks whether `SELECT` or `B` is held low at boot:
   - Held -> forces `TFT_SPI_FALLBACK_HZ` (`40 MHz`) and writes it to NVS (`persistDisplaySpiClock()`).
   - Not held -> uses saved NVS clock (`40 MHz`) or default `TFT_SPI_HZ` (`80 MHz`, per `config.h`).
5. **Display & Worker Launch:** Calls `DisplayEmu::begin(spiDecision.spiHz)`, which allocates ping-pong PSRAM frame buffers (`s_asyncEmuBuf[0..1]`), creates `s_displayQueue` (depth `1`) and `s_displayDoneSem`, spawns `BMO_Display` pinned to **Core 0**, locks `SpiArbiter`, initializes the ST7789 (`tft.rotation = 3`, `MADCTL = MY | MV | BGR`), clears the screen, allocates double-buffered PSRAM `menuCanvas` (`s_menuCanvasArr[0..1]`), and pops a boot toast if safe mode or an abnormal reset occurred.
6. **Mascot & Dormant Subsystems:** Calls `Battery::begin()` (no-op while `FEATURE_BATTERY_MONITOR=0`) and `BmoFace::begin()`, setting expression `IDLE`.
7. **MicroSD Mount & Catalog:** Calls `SDCard::begin()` (under `SpiArbiter::ScopedLock`): mounts FAT32, loads or rebuilds `/.bmo_index` (`BMOIDX03`), initializes `SaveManager::begin()` (`/saves`), and sizes `s_visibleRomIndexes` / `s_visibleGames` in PSRAM (`syncVisiblePoolCapacity()`).
8. **Telemetry Log:** Prints free PSRAM, largest contiguous PSRAM block, ROM pool capacity, and free internal IRAM before entering `BmoApp::loop()` at `STATE_BOOT_SPLASH`.

---

## 3. Dual-Core Threading, Shared State, and Synchronization

The firmware splits emulation/control (**Core 1**) from ST7789 SPI pixel transfer (**Core 0**):

```
Core 1 (Arduino setup/loop)                        Core 0 ("BMO_Display" @ prio MAX-2, 4 KiB stack)
  ├─ Buttons, state machine, emulator step          └─ displayWorkerTask():
  ├─ Copies framebuffer -> s_asyncEmuBuf[next]           ├─ waits on s_displayQueue (250 ms poll)
  ├─ Waits on s_displayDoneSem (1000 ms timeout)         ├─ converts 8-bit NES/DOOM palette -> BGR565
  ├─ Enqueues DisplayTaskCmd into s_displayQueue         ├─ acquires SpiArbiter::ScopedLock
  └─ SDCard / SaveManager under SpiArbiter::ScopedLock   ├─ SPI.writeBytes(...) -> ST7789
                                                         └─ releases lock & gives s_displayDoneSem
```

### Shared-State & Synchronization Inventory

| Variable / Primitive | Owner / Threads | Synchronization Mechanism | Lifetime / Invariant |
| :--- | :--- | :--- | :--- |
| `s_spiMutex` (`src/hal/spi_arbiter.cpp`) | Core 0 (`displayWorkerTask`), Core 1 (`SDCard`, `SaveManager`, sync `DisplayEmu` init/clear) | FreeRTOS recursive mutex via RAII `SpiArbiter::ScopedLock` | Created once in `SpiArbiter::init()`. Protects all FSPI transactions (`TFT_CS=10`, `SD_CS=13`). |
| `s_displayQueue` (`QueueHandle_t`, len `1`) | Producer: Core 1; Consumer: Core 0 | FreeRTOS queue (`kDisplayCmdTimeoutTicks = 1000 ms`) | Commands: `CMD_PUSH_CANVAS`, `CMD_PUSH_FULLSCREEN`, `CMD_STREAM_RAW16`, `CMD_STREAM_NES`, `CMD_STREAM_DOOM`. |
| `s_displayDoneSem` (`SemaphoreHandle_t`) | Signaled by Core 0 on frame finish; awaited by Core 1 | FreeRTOS binary semaphore (`kDisplayWaitTimeoutTicks = 1000 ms`) | Must be drained via `DisplayEmu::waitForDisplay()` before freeing any ROM/emulator framebuffer (`destroyActiveEmulator()`). |
| `s_asyncEmuBuf[0..1]` (`320×240×2` PSRAM) | Written by Core 1; read by Core 0 | Ping-pong (`s_activeEmuBuf = 1 - s_activeEmuBuf`) gated by `waitForDisplay()` | Staging buffer for `streamRawFrameAsync()` (`GB`, `GBC`, and all 11 experimental cores) plus `NES` / `DOOM` raw8 captures. |
| `s_menuCanvasArr[0..1]` (`PSRAMCanvas`) | Drawn by Core 1; streamed by Core 0 | Ping-pong (`s_activeDrawCanvas = 1 - s_activeDrawCanvas`) gated by `waitForDisplay()` | `320×240` BGR565 menu canvas (`153,600 B` each). |
| `s_displayBusy` (`volatile bool`) | Set/cleared on Core 0; sampled on Core 1 | Atomic byte/word read (`isDisplayBusy()`) | Non-blocking busy hint for frame-skip telemetry (`s_droppedFrames`). |
| `s_nesRowBuf[256]`, `s_doomLineBuf[320]`, `s_doomPalette[256]` | Private to Core 0 `displayWorkerTask` | Single task (`BMO_Display`) only | Static aligned DRAM row buffers for fast 8-bit-to-BGR565 scanline DMA bursts. |
| `s_currentState`, `s_currentRomBuffer`, `s_visibleGames`, `s_activeCore`, `s_pauseQuitState` (`bmo_app.cpp`) | Strictly **Core 1** only | Single-thread affinity (`Arduino loopTask`) | Never touched from Core 0 or ISRs. Core 0 only receives snapshot buffer pointers inside `s_asyncEmuBuf`. |
| `s_bootBreadcrumb` (`RTC_NOINIT_ATTR`) | Written by Core 1 | Single-thread affinity; validated by `kBreadcrumbMagic` | Survives software reset, WDT reset, and panic; indeterminate on cold power-off. |

All 11 experimental cores (`SMS`, `GG`, `PCE`, `Atari`, `PICO-8`, `Genesis`, `SNES`, `WonderSwan`, `NGP`, `Lynx`, `Coleco`) submit full frames asynchronously through `DisplayEmu::streamRawFrameAsync()` -> `s_asyncEmuBuf` -> `CMD_STREAM_RAW16` on Core 0.

---

## 4. ESP32-S3 Memory & Partition Architecture

- **Board Profile (`sketch.yaml`):** `esp32:esp32:esp32s3` with `FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi`.
- **Flash Partitions (`firmware/BmoGameboy/partitions.csv`, `16 MiB` QIO):**

| Name | Type / SubType | Offset | Size | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| `nvs` | `data / nvs` | `0x009000` | `20 KiB` (`0x5000`) | Persisted preferences (`TFT_SPI_HZ` fallback selection) |
| `otadata` | `data / ota` | `0x00E000` | `8 KiB` (`0x2000`) | Boot partition selection state |
| `app0` | `app / ota_0` | `0x010000` | `8,388,608 B` (`8 MiB`, `0x800000`) | Firmware application image |
| `ffat` | `data / fat` | `0x810000` | `8,257,536 B` (`~7.9 MiB`, `0x7E0000`) | Reserved internal FAT partition (unused when MicroSD is active) |

- **Internal RAM (`327,680 B` DRAM + IRAM):** Used for FreeRTOS task stacks, DMA descriptors, static C globals/BSS, emulator hot tables moved out selectively via `-Wshadow`/budging checks, and `s_nesRowBuf`/`s_doomLineBuf`. Measured post-remediation static DRAM: `185,272 B` (`56.5%`, `default`), `94,952 B` (`29.0%`, `no-doom`), `69,856 B` (`21.3%`, `mit`).
- **Octal PSRAM (`8 MiB`, `MALLOC_CAP_SPIRAM`):**
  - **UI & Display (~`768 KiB` static/boot):** `s_menuCanvasArr[0..1]` (`2 × 153,600 B`), `s_asyncEmuBuf[0..1]` (`2 × 153,600 B`), `BmoFace` `320×240` BGR565 framebuffer (`153,600 B`), `BoxArt` `64×64` buffer (`8,192 B`) + key table, and `FramePercentiles` ring window.
  - **ROM Catalog Pool (`SDCard` + `bmo_app.cpp`):** Dynamic capacity via `SDCard::computePoolCapacity()` (`256..36,000` entries): `RomFile` (`16 B` on 32-bit ESP32) + variable-length `s_pathArena` UTF-8 strings + `s_visibleRomIndexes`/`s_visibleGames`.
  - **Active Game / Core Heap:** Holds loaded ROM buffer (except `ROM_WAD`, which streams/indexes from SD) plus active emulator RAM/state. For DOOM (`emu_doom.cpp` + `psram_zone_budget.h`), `DoomEmu::computeZoneBytes(largestFreeBlock)` reserves a contiguous `2,560 KiB .. 6,144 KiB` heap while enforcing a `512 KiB` safety headroom (`DOOM_ZONE_MARGIN_BYTES`). Exact static ledger check: `python -m tools.guardian psram-budget`.

---

## 5. Binary Data Formats & Atomic Storage

- **Binary ROM Index (`/.bmo_index`, `src/storage/rom_index.h`):**
  - **Magic & Version:** `"BMOIDX03"` (`version = 3`). Automatically detects and upgrades legacy `"BMOIDX02"` via SD re-scan.
  - **Layout:** `BmoIndexHeader` (packed, includes `entryCount`, `entrySize = 20`, `stringTableBytes`, payload `crc32`, root `timestamp`, 64-bit `totalBytes`, and `fingerprint`) followed by `entryCount` × `BmoIndexEntry` (`pathOffset`, `pathLength`, `type`, `flags` incl. `FLAG_FAVORITE`, `fileSize`, `crc32Prefix` of first `4,096 B`) + NUL-terminated UTF-8 path string table.
- **Save States (`/saves/*.state*`, `src/storage/save_state_format.h` & `save_manager.cpp`):**
  - **Current Format (`BSS2`, magic `0x32535342`, `version = 2`):** `HeaderV2` (`72 B`, checked by `static_assert`) storing `coreId`, `pathKeyHash`, `romSize`, `stateSize`, `ramSize`, payload `crc32`, and `romTitle[32]`, followed by serialized core state and SRAM blobs.
  - **Legacy Read Support:** Reads `BSS1` (`60 B` `HeaderV1`, magic `0x31535342`) with `stateSize`/`ramSize`/`crc32` checks before rejecting corrupted files.
- **Battery RAM Saves (`/saves/*.sav`) & Box Art (`/.bmo_art`):**
  - `.sav` SRAM dumps and `BMOIDX03` / save states write through `AtomicFs::commitTransaction()` (`src/storage/atomic_fs.h`): writes `<path>.tmp`, verifies CRC32/size via `verifyFilePayloadCrc()`, rotates live `<path>` -> `<path>.bak`, promotes `.tmp` -> `<path>`, re-verifies final file, and deletes `.bak` (rolling back on rename failure). `recoverTransaction()` repairs interrupted `.tmp`/`.bak` pairs at mount/open.
  - Box art (`src/storage/box_art.cpp`) renders `64×64` byte-swapped BGR565 raw pixel sprites (`8,192 B` each) indexed for fast lookup during menu browsing.

---

## 6. Error Handling & Fault Policy

| Fault Condition | Implemented Response | Location |
| :--- | :--- | :--- |
| Shared SPI mutex creation or recursive take/give failure | Fail-fast `abort()` rather than corrupting SPI transfers | `src/hal/spi_arbiter.cpp:14-25` |
| Core 0 display queue saturated or DMA worker hung (`> 1,000 ms`) | Logs `LOG_WARN` with timeout counter (`s_displayQueueTimeouts` / `waitForDisplay`), skips stall cleanly without infinite spin | `src/ui/display_worker.cpp:91-97, 651-658` |
| Breadboard / long-wire ST7789 instability at `80 MHz` | Hold `SELECT` or `B` during boot to force `40 MHz` fallback (`TFT_SPI_FALLBACK_HZ`), persisted in NVS with yellow HUD toast | `src/app/bmo_app.cpp:1121-1132`, `boot_safe_mode.h` |
| Abnormal reset (brownout, task/interrupt WDT, panic) | Logs `esp_reset_reason()` + `s_bootBreadcrumb` prior state/emulator on serial and shows coral toast for `3.5 s` | `src/app/bmo_app.cpp:1096-1135` |
| Missing or unreadable ROM at launch / SD card swapped | Shows toast (`SD READ FAILED` / `SD CHANGED: RESCANNING...`), triggers `SDCard::rebuildIndex()`, restores `STATE_GAME_MENU` cleanly | `src/app/bmo_app.cpp:640-664` |
| Emulator `start()` returns `false` (OOM or invalid ROM header) | Frees `romData`, resets pointers, shows `EMULATOR START FAILED` toast, returns safely to `STATE_GAME_MENU` | `src/app/bmo_app.cpp:673-684` |

---

## 7. Execution Environments & Verification Boundaries

| Environment | What It Executes | Evidence Scope |
| :--- | :--- | :--- |
| **ESP32-S3 Firmware** (`firmware/BmoGameboy/`, `firmware/bringup/`) | Full Arduino-ESP32 sketch, FreeRTOS Core 0/1 tasks, FSPI/ST7789/SD drivers, 14 upstream emulator wrappers | `TARGET_COMPILED` (`arduino-cli`, `0` warnings across all profiles); `HARDWARE_TESTED`: `0` measurements recorded yet |
| **Host C/C++ & WASI Tests** (`tests/`, `scripts/`) | First-party modules (`rom_index`, `save_state_format`, `atomic_fs`, `bmo_face`, `spi_arbiter`, `frame_pacing`, `boot_safe_mode`) + 11 upstream C/C++ cores executed `50` frames in WASI | `HOST_TESTED` only; WASI harness uses synthetic minimal ROMs/programs, not full retail libraries or real ESP32 SPI/PSRAM latency |
| **Browser Simulator** (`simulator/`) | JS UI port + WASM-compiled `bmo_face`, `Peanut-GB`, `Agnes`, `Stella`, `Colem` + Three.js 3D enclosure & internal teardown views | `HOST_TESTED` UI/WASM/security smoke tests; menus/storage use browser IndexedDB, not FAT32 |
| **Wokwi Model** (`hardware/diagram.json`, `hardware/wokwi.toml`) | Real ESP32-S3 ELF with `ILI9341` display substitute and pushbutton switches | Interactive virtual check only; `ILI9341` does not validate `80 MHz` ST7789 electrical integrity |

**Static Analyzers (`tools/guardian/`):** Provides static source/ELF checks (`ast_linter.py`, `bus_model.py`, `psram_budget.py`, `elf_analyzer.py`, `cppcheck_runner.py`, `clang_tidy_runner.py`). All generated JSON knowledge graphs and fabricated runtime ledgers were removed in Phase 2 (`docs/audits/findings-register.md`).
