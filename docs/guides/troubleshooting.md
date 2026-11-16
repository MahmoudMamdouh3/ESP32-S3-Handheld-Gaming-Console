# Developer & Hardware Troubleshooting Guide

> [!NOTE]
> Diagnostic checks, pin checks, and software error paths below come directly from first-party firmware (`firmware/BmoGameboy/src/`), `firmware/bringup/bringup.ino`, and host tooling (`tasks.py`). Tool/host procedures are `HOST_TESTED` and firmware compilation flags are `TARGET_COMPILED`, while **physical hardware measurements remain `HARDWARE_TESTED: 0` records (`NOT_TESTED` on physical silicon)** until logged in [`docs/hardware_measurements.md`](../hardware_measurements.md).

For first-power-on multimeter sequencing, start with [`docs/guides/safety.md`](safety.md) and [`docs/guides/bringup.md`](bringup.md). For every compile flag, FQBN option, and profile macro, see [`docs/build-and-ci.md`](../build-and-ci.md).

---

## 1. Workstation Setup, Linters & Host Test Failures

| Symptom / Command Output | Cause | Step-by-Step Resolution |
| :--- | :--- | :--- |
| `arduino-cli not found. Run python tasks.py setup --install-arduino-cli` | Neither `arduino-cli` on `PATH` nor `.tools/arduino-cli/` is populated. | Run `python tasks.py setup --install-arduino-cli` (downloads SHA-256 verified `v1.6.0` and installs pinned `esp32:esp32@3.3.11` plus Adafruit libraries from [`sketch.yaml`](../../firmware/BmoGameboy/sketch.yaml)). Verify with `python tasks.py doctor`. |
| `clang-format check` fails in `tasks.py format --check` or `.githooks/pre-commit` | Unformatted first-party C/C++ changes (`firmware/BmoGameboy/src/` excluding `third_party/`). | Run `python tasks.py format` and re-stage files. `clang-format` is auto-discovered from `PATH` or `.tools/wasi-sdk-*/bin/`. |
| `ruff`, `mypy`, `lizard`, or `cppcheck` missing or outdated | Development packages not installed from the pinned lockfile. | Run `python -m pip install -r requirements.lock` (`cppcheck`, `clang-tidy`, `lizard`, `ruff`, `mypy`, `pillow`, `numpy`, `trimesh`, `manifold3d`), then re-run `python tasks.py doctor`. |
| `scripts/sync_ai_codemap.py --check failed (run --write)` | Files were added, removed, or resized under `firmware/`, `scripts/`, or `tests/host/`. | Run `python scripts/sync_ai_codemap.py --write` and stage updated [`docs/ai/CODEMAP.md`](../ai/CODEMAP.md) (`<= 200` lines) and [`docs/ai/STATE.md`](../ai/STATE.md) (`<= 60` lines). |
| `extract_claims.py --fail-on-false` reports `[STALE]` or `[FALSE]` | Documentation cited superseded binary sizes, line counts, `< 1 min` timings, retired `S7V7F5`/`U3V40F5` parts, or unsafe `FEATURE_*` / prohibited flash-mode settings without warning context. | Inspect the stderr location printed by `python scripts/extract_claims.py --fail-on-false`, update the claim or baseline qualifier in docs (or `MEASURED_VALUES` in [`scripts/extract_claims.py`](../../scripts/extract_claims.py) after an intentional build/test change), and re-run. |
| `#include "..." mismatches exact disk case` in `tasks.py check` | Windows NTFS is case-insensitive, so mismatched header casing compiles locally but breaks Linux CI. | Rename the `#include "..."` string or header file to match exact disk case. |
| `lizard complexity check failed (CCN > 25 or length > 100)` | A staged C/C++/JS/Python function exceeds `CCN = 25` or `100` lines. | Decompose the function into small helpers (`<= 60` lines target, `CCN <= 15` target) and re-run `lizard -C 25 -L 100 --warnings_only` on the modified directory. |
| `ASan+UBSan requires GCC or Clang host compiler cleanly (use Linux CI or MinGW/Clang)` | Running `python tasks.py check --sanitizers` or `test --sanitizers` on Windows MSVC (`cl.exe`), where CMake ASan+UBSan flags use GCC/Clang driver options (`CMakeLists.txt`). | Run standard Release host tests on Windows (`python tasks.py check` / `python tasks.py test`), or install MinGW/Clang (`CC`/`CXX`); Linux CI (`ci.yml`) runs `--sanitizers` automatically. |
| `BENCHMARK BUDGET CHECK PASSED` fails on `wasm_init_ms` right after building WASM | Antivirus scanning (e.g., Windows Defender on newly written `.wasm`) or CPU contention caused a cold startup outlier. | Re-run `python tasks.py bench --check` once on an idle CPU (without `--with-wasm-build`) after the `.wasm` scan settles, or inspect `benchmarks/latest_results.json` vs `benchmarks/budgets.json`. |

---

## 2. Firmware Compile, Flag & Preprocessor Errors

| Error / Warning | Source Guard | Explanation & Exact Fix |
| :--- | :--- | :--- |
| `#error "FEATURE_AUDIO=1 requires -DBMO_HARDWARE_CONFIRMED_AUDIO=1..."` | [`audio_i2s.cpp`](../../firmware/BmoGameboy/src/hal/audio_i2s.cpp) | **Hardware hard stop:** Current soldered board has no I2S DAC on `GPIO 38/39/40`. Remove `-DFEATURE_AUDIO=1`, or for compile-only testing use `python tasks.py build --profile audio-compile-only` (do not flash without fitted hardware). |
| `#error "FEATURE_BATTERY_MONITOR=1 requires -DBMO_HARDWARE_CONFIRMED_BATTERY=1..."` | [`battery.cpp`](../../firmware/BmoGameboy/src/hal/battery.cpp) | **Hardware hard stop:** Current board has no voltage divider on `GPIO 1`. Keep `FEATURE_BATTERY_MONITOR=0`, or use `--profile battery-compile-only` for compile-only checks. |
| `static_assert` failure in `pins_check.h` | [`pins_check.h`](../../firmware/BmoGameboy/src/hal/pins_check.h) | A GPIO pin collided with another peripheral, a strapping pin (`0, 3, 45, 46`), QIO SPI Flash (`26–32`), or Octal PSRAM (`33–37`). Keep [`pin_map.h`](../../firmware/BmoGameboy/src/hal/pin_map.h) aligned with physical hardware or follow the [`AGENTS.md`](../../AGENTS.md) Hard-Stop Change Protocol before modifying pin assignments. |
| `note: #pragma message: TFT_SPI_HZ exceeds the ST7789VW 62.5 MHz published serial-write limit; board timing is unmeasured` | [`board_timing.h:17`](../../firmware/BmoGameboy/src/hal/board_timing.h#L17) | Informational `#pragma message` (not a bug or warning failure) reminding you `TFT_SPI_HZ` defaults to `80000000UL` (`80 MHz`) above the ST7789VW `62.5 MHz` datasheet limit ([ADR 0014](../decisions/0014-display-spi-clock.md)). You can compile a `40 MHz` default test by passing `-DTFT_SPI_HZ=40000000UL` via `compiler.c.extra_flags` / `compiler.cpp.extra_flags`. |
| PSRAM / USB CDC stops working after passing custom flags to `arduino-cli` | [`build-and-ci.md`](../build-and-ci.md) | You overwrote `build.extra_flags`, wiping out Arduino-ESP32 board defines (`ARDUINO_USB_CDC_ON_BOOT`, PSRAM flags). **Always** append macros via `compiler.c.extra_flags` and `compiler.cpp.extra_flags` (or use `python tasks.py build`). |
| Firmware compiles into a `"Booting into Test Mode..."` loop with no UI | [`bmo_app.cpp:20,108`](../../firmware/BmoGameboy/src/app/bmo_app.cpp#L20) | `-DENABLE_UNIT_TESTS` was enabled manually, running legacy on-device `runAllTests()` and halting in `while (true)`. Remove `-DENABLE_UNIT_TESTS` for normal firmware; run host tests via `python tasks.py test`. |
| `app0` Flash or Static DRAM > `85.0%` in `tasks.py size --check-budgets` | [`tools/tasks/build.py`](../../tools/tasks/build.py), [`partitions.csv`](../../firmware/BmoGameboy/partitions.csv) | `app0` is `8,388,608 B` (`0x800000`) and internal DRAM is `327,680 B`. Move large lookup tables to `const` (`.flash.rodata`) or buffers `>= 1,024 B` to Octal PSRAM (`hal/psram_alloc.h`), or build smaller profiles (`--profile no-doom` or `--profile mit`). |
| `FRAM module not detected at 0x50!` | [`fram_save.cpp:24`](../../firmware/BmoGameboy/src/storage/fram_save.cpp#L24) | `FEATURE_FRAM=1` was set manually, which is unpopulated and tries `Wire.begin(43, 44)` on the DevKitC-1 UART0 TX/RX pins ([ADR 0007](../decisions/0007-fram-wiring.md)). Leave `FEATURE_FRAM` undefined/`0`. |

---

## 3. Flashing, USB CDC Serial & Boot-Loop Recovery

1. **No serial port (`COMx` / `/dev/ttyUSBx` / `/dev/ttyACMx`) appears:**
   - Ensure you are using a **data-capable USB-C cable** (many cheap USB-C cables only wire power) connected to the **`UART/COM`** port on the `ESP32-S3-DevKitC-1-N16R8`.
   - To manually enter ROM download mode on either `UART/COM` or `USB/OTG`: hold **`BOOT`**, tap **`RST`**, release **`RST`**, then release **`BOOT`**, and retry `.\arduino-cli.exe board list`.
   - Remove AA batteries / turn the battery switch **OFF** before plugging USB-C (see [`docs/guides/safety.md`](safety.md)).
2. **Continuous boot loop (`rst:0x...` repeating at `115200` baud):**
   - **Check USB power first (`ESP_RST_BROWNOUT`, reason `9`):** Plug directly into a PC motherboard USB port rather than an unpowered hub. High inrush during Octal PSRAM or TFT/SD init trips brownout on weak hubs.
   - **Verify actual silicon with read-only `esptool v5.3.1`:** Run [`bringup.md` Step 3](bringup.md#step-3-read-only-esptool-v531-chip--flash-verification-before-flashing-anything) (`flash-id`, `chip-id`) to confirm flash vendor/capacity before altering build settings.
   - **`FlashMode=qio` conflict fallback:** [`sketch.yaml`](../../firmware/BmoGameboy/sketch.yaml) locks `FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi` per the WROOM-1 datasheet, though owner notes recorded `qio` boot-looping on this physical unit while alternate flash modes booted. Never permanently edit `sketch.yaml` without an ADR; test diagnostic `FlashMode=dio` (or diagnostic alternate SPI mode) on `firmware/bringup` using `--fqbn` as documented in [`bringup.md` Boot-Loop Fallback](bringup.md#boot-loop-fallback).
   - **Check `ESP_RST_PANIC` / `TASK_WDT` crash breadcrumb:** On the next boot, firmware logs `Reset reason: <name> (<code>)` and reads `RTC_NOINIT_ATTR s_bootBreadcrumb` (`magic=0x424D4F42`, `prev_state`, `prev_emu`). See [`boot_safe_mode.h`](../../firmware/BmoGameboy/src/app/boot_safe_mode.h) for all reset reason codes (`0..15`) and `SystemState` IDs (`0..7`).
   - **Verify `bringup.ino` first:** If full firmware fails but you have not proven PSRAM/SPI/power on the board yet, flash [`firmware/bringup/bringup.ino`](../../firmware/bringup/bringup.ino) (`390,880 B`) to isolate `[PSRAM]`, `[TFT]`, `[SD]`, and `[BTN]`.

---

## 4. Display (`ST7789`), Shared SPI & Control Troubleshooting

1. **Screen stays black (no backlight glow):**
   - Unplug USB and verify display `VCC` connects to `3V3`, `GND` connects to `GND`, and display `BLK` (backlight) receives `3.3 V` (`0` backlight PWM pin is connected in v1; see [`docs/wiring/01-display-and-buttons.md`](../wiring/01-display-and-buttons.md)).
   - Inspect solder joints against [`docs/wiring/wire_list.csv`](../wiring/wire_list.csv).
2. **Backlight glows white/lit, but screen is blank, snowy, or tears (`80 MHz` vs `40 MHz`):**
   - **Force Safe Mode (`40 MHz`):** Hold **`SELECT` (`GPIO 21`)** or **`B` (`GPIO 17`)** during power-on/reset until Serial prints `[SAFE_MODE] SELECT/B held at boot; forced display SPI clock to 40 MHz` and the screen shows `"SAFE MODE: 40 MHz SPI"` (`persisted` in NVS `bmo_disp/spi_hz`).
   - **Verify wiring vs [`pin_map.h`](../../firmware/BmoGameboy/src/hal/pin_map.h):** `TFT_SCK=12`, `TFT_MOSI=11`, `TFT_CS=10`, `TFT_DC=8`, `TFT_RST=9`. Confirm `10 kΩ` pull-ups (`GPIO 10 -> 3V3` and `GPIO 13 -> 3V3`) and optional `22–33 Ω` series damping resistors on `SCK`/`MOSI` are intact, and keep SPI jumpers short (`< 6.0 cm`).
   - **Run On-Screen SPI Stress Test:** From the Console Carousel, press **`START`** to enter Diagnostics ([`app_diagnostics.cpp`](../../firmware/BmoGameboy/src/app/app_diagnostics.cpp)), press **`A`** to toggle the checkerboard `SPI STRESS PATTERN`, and press **`LEFT` / `RIGHT`** to flip between **`40 MHz`** and **`80 MHz`** live while watching for pixel shimmer.
   - **SD bus contention check:** Disconnect `SD_MISO` / remove the MicroSD card and re-test `bringup.ino` at `40 MHz`. If display works only without the SD module, check `10 kΩ` pull-ups on `SD_CS` (`GPIO 13`) and verify Type B SD `MISO` tristates cleanly ([`docs/wiring/02-sd-card.md`](../wiring/02-sd-card.md)).
3. **Buttons register phantom presses, stick LOW, or `X/Y/L/R` do nothing:**
   - All buttons use internal `INPUT_PULLUP` (`HIGH` = released, `LOW` = pressed to `GND`). Never wire a tactile switch to `3V3`. A continuously stuck button usually indicates two permanently shorted legs on a 4-pin tactile switch — use diagonal legs (`1–3` / `2–4`) verified with Continuity mode before soldering.
   - Open Diagnostics (**`START`** from Console Carousel) to inspect **`BTN MASK: XXX`** and the visual 12-key tester ([`ui_diagnostics.cpp`](../../firmware/BmoGameboy/src/ui/ui_diagnostics.cpp)).
   - Remember `X (GPIO 14)`, `Y (GPIO 42)`, `L (GPIO 47)`, and `R (GPIO 48)` are **configured in firmware but physically unsoldered** on the v1 8-button board; all UI/pause/save actions work with the 8 wired buttons ([`docs/reference/controls.md`](../reference/controls.md)). Note `GPIO 48` also connects to the onboard WS2812 RGB LED on DevKitC-1 v1.0 boards.

---

## 5. MicroSD, `BMOIDX03` Index, Box Art & Saves

### Firmware Error Toast & Serial Decoder

| On-Screen Toast / Serial Log | Source Location | Cause & Fix |
| :--- | :--- | :--- |
| `Failed to mount SD card!` / `SD CARD STILL NOT FOUND` | [`bmo_app.cpp:82`](../../firmware/BmoGameboy/src/app/bmo_app.cpp#L82), [`app_diagnostics.cpp:28`](../../firmware/BmoGameboy/src/app/app_diagnostics.cpp#L28) | Card formatted as `exFAT` instead of `FAT32`, capacity `> 32 GB` not reformatted to `FAT32`, missing `10 kΩ` pull-up on `SD_CS=13`, Type B `5 V` SD module wrongly powered on `3V3` (`~2.2 V` brownout), or loose `SCK=12` / `MOSI=11` / `MISO=15` (`<= 3.35 V` verified first per [`bringup.md`](bringup.md#step-2-power-on-with-sd-miso-disconnected--standalone-miso-test--335-v)). Fix card/wiring and press **`START`** inside Diagnostics to retry mount. |
| `RESCAN COMPLETE; SOME PATHS SKIPPED` | [`app_diagnostics.cpp:39`](../../firmware/BmoGameboy/src/app/app_diagnostics.cpp#L39) | One or more files exceeded `MAX_ROM_PATH_LEN = 255 B` UTF-8, sat deeper than `kMaxScanDepth = 3`, lived in ignored dirs (`.hidden`, `saves`, `screenshots`, `boxart`, `covers`), or failed extension/minimum-size filters (`rom_file_filter.h`). |
| `SD CHANGED: RESCANNING...` -> `LIBRARY UPDATED` | [`app_launcher.cpp:50`](../../firmware/BmoGameboy/src/app/app_launcher.cpp#L50) | Launched ROM size or 4 KiB prefix CRC32 differed from the cached `/.bmo_index` entry (card swapped or modified externally). Firmware automatically rebuilt `BMOIDX03`; re-select your game cleanly. |
| `SD READ FAILED: CHECK CARD` | [`app_launcher.cpp:112`](../../firmware/BmoGameboy/src/app/app_launcher.cpp#L112) | ROM file unreadable or PSRAM allocation (`loadRom`) failed. Verify card formatting, free PSRAM in Diagnostics (`PSRAM Free / MaxBlk`), and valid file size (`> 0`). |
| `EMULATOR START FAILED` | [`app_launcher.cpp:131`](../../firmware/BmoGameboy/src/app/app_launcher.cpp#L131) | Core rejected ROM header/mapper or exhausted PSRAM/arena memory (e.g., invalid WAD or oversized ROM). Inspect `LOG_ERROR` lines at `115200` baud (`-DLOG_LEVEL=LOG_LEVEL_DEBUG` via `--profile debug-log-compile-only` for full detail). |
| `DOOM CORE OFF` / `EXPERIMENTAL CORE OFF` | [`app_launcher.cpp:24,31`](../../firmware/BmoGameboy/src/app/app_launcher.cpp#L24) | Firmware was compiled with `-DBMO_ENABLE_DOOM=0` (`--profile no-doom`) or `-DBMO_EXPERIMENTAL_CORES=0` (`--profile mit`). Recompile `--profile default` (personal local build) to restore all 15 platforms. |

### `scripts/deploy_sd.py` & `convert_covers.py` Issues
- **Dry-run didn't write to SD card:** `scripts/deploy_sd.py` defaults to dry-run in `mirror` mode and checks FAT32 volume label `GAMELIB`. Pass `--apply` (and `--expected-label <YOUR_LABEL>` if renamed), or `--mode partitioned` for prebuilding `BMOIDX03` directly from a flat `games/` folder.
- **Box art missing for a game:** Ensure artwork is converted to `64×64` `.raw` (byte-swapped BGR565) or `.bmp` under `/boxart/` or `/covers/` named `<path_key>.raw` (computed identically by [`path_key.h`](../../firmware/BmoGameboy/src/storage/path_key.h) and [`rom_key.py`](../../scripts/rom_key.py)). Generate keyed covers via `python scripts/convert_covers.py --sd-root E:\` (requires local user-supplied artwork; no web scraper exists).

---

## 6. Performance Profiling & PSRAM Diagnostics (`BMO_PROFILE=1`)

1. Build and flash the profiler-enabled image (moves zero-overhead macros to active microsecond counters with the `600`-sample ring in PSRAM, `3,278,016 B` App / `185,472 B` DRAM):
   ```powershell
   python tasks.py build --profile profiler-compile-only --warnings strict --fail-on-warnings
   .\arduino-cli.exe upload --profile esp32s3 --port COM3 --input-dir build/profile-profiler-compile-only firmware/BmoGameboy
   ```
2. Open the Console Carousel -> press **`START`** (Diagnostics) -> press **`SELECT`** (outside SPI Stress mode) to view live **`600`-FRAME WINDOW (`EMU_FRAME`)** (`min` / `avg` / `max` / `p50` / `p95` / `p99` us) and per-zone timings (`EMU_FRAME`, `SPI_STREAM`, `BMO_FACE`, `MENU_DRAW`, `SD_READ`) ([`ui_diagnostics.cpp`](../../firmware/BmoGameboy/src/ui/ui_diagnostics.cpp)).
3. Check Serial Monitor (`115200` baud) for:
   - `[STACK] loopTask HWM=... words, displayTask HWM=... words` (printed every `5 s` while Diagnostics is open)
   - `PSRAM boot state: free=... largest_block=...` and `[PROFILE_CSV]` / `FPS:` / `Frame window:` lines
4. Parse saved serial logs into tables with:
   ```powershell
   python tools/telemetry_plotter.py --input serial_capture.log --output bench_telemetry.txt --json-out bench_summary.json
   ```
   Confirm the report says `Data source: CAPTURED` (no `--input` generates synthetic data only). Record physical findings in [`docs/hardware_measurements.md`](../hardware_measurements.md).

---

## 7. Browser Simulator & WebAssembly (`simulator/`)

| Issue | Resolution |
| :--- | :--- |
| Opening `simulator/index.html` directly over `file://` shows blank canvas / ES module error | Modern browsers block `<script type="module">` CORS requests on `file://`. Run `python simulator/run_simulator.py` (serves `127.0.0.1:8080`, auto-builds missing WASM, rejects path traversal). |
| `WASI SDK not found` when running `scripts/build_wasm_cores.py` | Point `WASI_SDK_PATH` to an unpacked `wasi-sdk-34.0-*` toolchain (Windows default lookup: `.tools/wasi-sdk-34.0-x86_64-windows`) or install WASI SDK 34 matching [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml). |
| Emulator or face changes in C++ aren't reflected in browser simulator | Run `python scripts/build_wasm_cores.py` (uses incremental `.o`/`.d` cache in `build/` and regenerates `simulator/bmo_playtest_cores.wasm` + `simulator/bmo_playtest_cores.b64.js`), then hard-reload the browser (`Ctrl+Shift+R`). |
| Testing a specific screen state directly via URL | Pass `?state=` (`splash`, `bmo`, `mascot`, `carousel`, `library`, `toppicks`, `museum`, `gameplay`, `dissect` / `internals`), optional `&console=<0..14>`, and `&game=<index>`. |
