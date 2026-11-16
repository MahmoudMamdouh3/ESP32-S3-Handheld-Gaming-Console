# Build System, Compiler Flags, Tasks CLI & CI Architecture

All firmware builds, host tests, static analyzers, benchmarks, formatting, ELF size audits, and simulator launches are wrapped by [`tasks.py`](../tasks.py) (`tools/tasks/`) on top of [`firmware/BmoGameboy/sketch.yaml`](../firmware/BmoGameboy/sketch.yaml), [`CMakeLists.txt`](../CMakeLists.txt), and [`arduino-cli`](https://arduino.github.io/arduino-cli/). For symptom-driven compile, flashing, SPI, or SD failure fixes, see [`docs/guides/troubleshooting.md`](guides/troubleshooting.md).

---

## 1. Daily Developer Commands (`tasks.py`)

```powershell
# 1. One-time workstation bootstrap (hooks, verified arduino-cli v1.6.0, core + libs from sketch.yaml)
python tasks.py setup --install-arduino-cli
python tasks.py doctor                      # verify Python deps, Git hooks, arduino-cli, clang-format, cmake, ruff, cppcheck

# 2. Inner-loop & full static/host verification (no firmware compile)
python tasks.py check --fast                # ~18-30 s: syntax, hard stops, include case, links, color tokens, Guardian, ruff/mypy/tsc, fast Py tests
python tasks.py check --strict-lint --cppcheck  # full host check: + Cppcheck, 10/10 mutation sanity, 30 Release CTests

# 3. Full host test suite + strict repository & target-compile validator
python tasks.py test                        # 30 CTest + 149 Py tests + 12 Guardian tests + scripts/validate_repo.py

# 4. Firmware compile & ELF budget audit
python tasks.py build                       # default profile (all 15 platforms), strict -Wall -Wextra -Wshadow warnings
python tasks.py build --profile all --warnings strict --fail-on-warnings
python tasks.py build --compile-commands    # generates build/compile_commands.json for clangd / Guardian clang-tidy
python tasks.py size --check-budgets        # verifies Flash <= 85% app0 and Static DRAM <= 85%

# 5. Formatting, benchmarks, flashing, serial, and browser simulator
python tasks.py format --check              # clang-format check across first-party C/C++ (never edits third_party/)
python tasks.py format
python tasks.py bench --check               # host C++, Node/WASM simulator, and Python script regression check
python tasks.py flash --port COM3           # interactive device flash (safety-checked)
python tasks.py monitor --port COM3         # serial monitor at 115200 baud
python tasks.py sim                         # loopback 127.0.0.1:8080 simulator (auto-builds WASM if absent)
```

---

## 2. Firmware Build Profiles (`python tasks.py build --profile <name>`)

Defined cleanly in [`tools/tasks/__init__.py`](../tools/tasks/__init__.py) and compiled against `firmware/BmoGameboy/sketch.yaml` (`esp32s3`):

| Profile | Extra Preprocessor Flags | Output Build Directory | Platforms & Licensing Status | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| `default` | *(none; inherits `feature_flags.h` defaults)* | `build/target-compile/` | All `15` platforms (`BMO_ENABLE_DOOM=1`, `BMO_EXPERIMENTAL_CORES=1`); personal local use only (`GPL` + `NC`, see [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md)) | Standard full-feature handheld image (`3,275,568 B` App, `185,456 B` DRAM, `TARGET_COMPILED`) |
| `no-doom` | `-DBMO_ENABLE_DOOM=0` | `build/profile-no-doom/` | `14` console cores without DOOM (`~90 KiB` DRAM saved); personal local use only | Cuts DOOM `.bss`/data zone static overhead (`95,136 B` DRAM) |
| `mit` | `-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0` | `build/profile-mit/` | `GB`, `GBC`, `NES` (`Peanut-GB`, `Walnut-CGB`, `agnes` — **`MIT` only**) | **Only redistributable CI/Release binary** (`878,688 B` App, `69,504 B` DRAM) |
| `audio-compile-only` | `-DFEATURE_AUDIO=1 -DBMO_HARDWARE_CONFIRMED_AUDIO=1` | `build/profile-audio-compile-only/` | Compile-only hardware-stop verification (no I2S DAC fitted; do **not** flash) | Prevents I2S code rot while keeping hardware hard stop `0` |
| `battery-compile-only` | `-DFEATURE_BATTERY_MONITOR=1 -DBMO_HARDWARE_CONFIRMED_BATTERY=1` | `build/profile-battery-compile-only/` | Compile-only hardware-stop verification (no GPIO 1 divider fitted; do **not** flash) | Prevents ADC battery code rot while keeping hardware hard stop `0` |
| `debug-log-compile-only` | `-DLOG_LEVEL=LOG_LEVEL_DEBUG` | `build/profile-debug-log-compile-only/` | Verbose serial debug macros (`[DEBUG]`) compiled | Deep serial diagnostics during bench bring-up |
| `profiler-compile-only` | `-DBMO_PROFILE=1` | `build/profile-profiler-compile-only/` | Activates Core 0/1 zone timers & `600`-sample PSRAM ring (`185,472 B` DRAM) | `BMO_PROFILE=1` serial CSV & Diagnostics Frame Profiler captures |

---

## 3. Complete Firmware Preprocessor Flag Reference

All flags below are `#ifndef`-wrapped so they can be overridden cleanly via `arduino-cli` `--build-property compiler.c.extra_flags=...` and `compiler.cpp.extra_flags=...`.

> [!WARNING]
> **Never override `build.extra_flags`!** Doing so replaces Arduino-ESP32 board defines (`ARDUINO_USB_CDC_ON_BOOT`, Octal PSRAM setup) and breaks compilation/boot. Always append via `compiler.c.extra_flags` and `compiler.cpp.extra_flags` (or use `python tasks.py build`).

### 3.1 Feature, Platform & Safety Flags (`feature_flags.h`, `profiler.h`, `sd_mount.cpp`, `fram_save.cpp`, `bmo_app.cpp`)

| Flag | Authoritative Header / File | Default | Valid Values / Notes | Constraint / Effect |
| :--- | :--- | :--- | :--- | :--- |
| `FEATURE_SD_CARD` | [`feature_flags.h`](../firmware/BmoGameboy/src/hal/feature_flags.h) | `1` | `0` or `1` | Enables MicroSD FAT32 mount (`SD_CS=13`, `SD_MISO=15`), `BMOIDX03` catalog, box art, and saves behind `SpiArbiter`. |
| `FEATURE_AUDIO` | [`feature_flags.h`](../firmware/BmoGameboy/src/hal/feature_flags.h) | **`0`** | `0` (Hard Stop) / `1` | **Permanent Hard Stop (=0):** No I2S DAC on `GPIO 38/39/40`. Setting `1` triggers `#error` in `audio_i2s.cpp` unless `-DBMO_HARDWARE_CONFIRMED_AUDIO=1` is also set. |
| `BMO_HARDWARE_CONFIRMED_AUDIO` | [`audio_i2s.cpp`](../firmware/BmoGameboy/src/hal/audio_i2s.cpp) | unset (`0`) | `1` | Required safety interlock when compile-testing I2S audio; never use on unpopulated v1 board. |
| `FEATURE_BATTERY_MONITOR` | [`feature_flags.h`](../firmware/BmoGameboy/src/hal/feature_flags.h) | **`0`** | `0` (Hard Stop) / `1` | **Permanent Hard Stop (=0):** No divider on `GPIO 1`. Setting `1` triggers `#error` in `battery.cpp` unless `-DBMO_HARDWARE_CONFIRMED_BATTERY=1` is also set. |
| `BMO_HARDWARE_CONFIRMED_BATTERY` | [`battery.cpp`](../firmware/BmoGameboy/src/hal/battery.cpp) | unset (`0`) | `1` | Required safety interlock when compile-testing battery ADC; never use without external voltage divider hardware. |
| `BMO_ENABLE_DOOM` | [`feature_flags.h`](../firmware/BmoGameboy/src/hal/feature_flags.h) | `1` | `0` or `1` | Compiles PrBoom/`doomgeneric` (`ROM_WAD`). Setting `0` saves `~90 KiB` DRAM. |
| `BMO_EXPERIMENTAL_CORES` | [`feature_flags.h`](../firmware/BmoGameboy/src/hal/feature_flags.h) | `1` | `0` or `1` | Compiles `12` additional platforms (`ROM_SMS`..`ROM_COLEM`). Setting `0` alongside `BMO_ENABLE_DOOM=0` yields the redistributable `mit` build (`GB`/`GBC`/`NES`). |
| `BMO_ENABLE_BAKED_ROMS` | [`feature_flags.h`](../firmware/BmoGameboy/src/hal/feature_flags.h), [`sd_mount.cpp`](../firmware/BmoGameboy/src/storage/sd_mount.cpp) | **`0`** | `0` or `1` | Must stay `0` in public CI and repo builds (clean clone guarantee). When `1`, `sd_mount.cpp` embeds a gitignored local header (`src/storage/local_baked_rom.h`) if present via `__has_include`. |
| `BMO_PROFILE` / `FEATURE_PROFILER` | [`profiler.h`](../firmware/BmoGameboy/src/hal/profiler.h) | `0` | `0` or `1` | `FEATURE_PROFILER` aliases `BMO_PROFILE` (`0` by default -> `0 B` Flash/DRAM/CPU overhead). When `1`, allocates `600`-sample ring in Octal PSRAM and enables `[PROFILE_CSV]` + Frame Profiler UI. |
| `LOG_LEVEL` | [`feature_flags.h`](../firmware/BmoGameboy/src/hal/feature_flags.h) | `3` (`LOG_LEVEL_INFO`) | `0` (`NONE`), `1` (`ERROR`), `2` (`WARN`), `3` (`INFO`), `4` (`DEBUG`) | Controls compile-time elimination of `LOG_ERROR` / `LOG_WARN` / `LOG_INFO` / `LOG_DEBUG` serial macros (`115200` baud). |
| `FEATURE_FRAM` | [`fram_save.cpp`](../firmware/BmoGameboy/src/storage/fram_save.cpp) | unset (`0`) | `0` or `1` | Dormant I2C FRAM driver (`0x50` on proposed `GPIO 43/44`). Leave `0`/unset (`GPIO 43/44` collide with DevKitC-1 UART0 bridge; see [ADR 0007](decisions/0007-fram-wiring.md)). |
| `ENABLE_UNIT_TESTS` | [`bmo_app.cpp`](../firmware/BmoGameboy/src/app/bmo_app.cpp) | unset | defined / undefined | Legacy on-device bring-up test harness mode that skips the console UI and halts after `runAllTests()`; replaced for CI/dev by CTest (`python tasks.py test`). |

### 3.2 Display Timing & Version Metadata Flags (`board_timing.h`, `version.h`)

| Flag | Header | Default | Notes |
| :--- | :--- | :--- | :--- |
| `TFT_SPI_HZ` | [`board_timing.h`](../firmware/BmoGameboy/src/hal/board_timing.h) | `80000000UL` (`80 MHz`) | Default display clock. Values `> 62500000UL` emit an informational `#pragma message` noting the ST7789VW `62.5 MHz` datasheet limit ([ADR 0014](decisions/0014-display-spi-clock.md)). |
| `TFT_SPI_FALLBACK_HZ` | [`board_timing.h`](../firmware/BmoGameboy/src/hal/board_timing.h) | `40000000UL` (`40 MHz`) | Used by Safe Mode (hold `SELECT` or `B` at boot) and Diagnostics SPI Stress `LEFT/RIGHT` toggle (`persisted` in NVS `bmo_disp/spi_hz`). |
| `SD_SPI_HZ` | [`board_timing.h`](../firmware/BmoGameboy/src/hal/board_timing.h) | `25000000UL` (`25 MHz`) | MicroSD clock on the shared `FSPI` bus (`SpiArbiter::ScopedLock`). |
| `TFT_WIDTH` / `TFT_HEIGHT` | [`board_timing.h`](../firmware/BmoGameboy/src/hal/board_timing.h) | `240` / `320` | Physical portrait controller geometry rendered in `320×240` landscape (`setRotation`). |
| `BMO_VERSION_STRING` / `BMO_GIT_SHA` | [`version.h`](../firmware/BmoGameboy/src/app/version.h) | `"0.9.0-dev"` / `"nogit"` | Printed in boot serial banner (`BMO Handheld v...`); overridable in release/custom builds (`BMO_VERSION_MAJOR/MINOR/PATCH = 0.9.0`). |

---

## 4. Board Configuration (`sketch.yaml`) & Flash Partitions (`partitions.csv`)

- [`firmware/BmoGameboy/sketch.yaml`](../firmware/BmoGameboy/sketch.yaml) is the single source of truth for **both** `tasks.py` and `.github/workflows/ci.yml`:
  - Core: `esp32:esp32 (3.3.11)` via `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
  - Libraries: `Adafruit BusIO (1.17.4)`, `Adafruit GFX Library (1.12.6)`, `Adafruit ST7735 and ST7789 Library (1.11.0)`
  - Locked FQBN: `esp32:esp32:esp32s3:CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi,USBMode=default`
    - `FlashMode=qio,PSRAM=opi` is a **Hardware Hard Stop**: do **not** use `FlashMode=opi` in active config without a signed owner ADR and prior `esptool flash-id` evidence ([`bringup.md`](guides/bringup.md#boot-loop-fallback)).
    - `CDCOnBoot=cdc` + `Serial.setTxTimeoutMs(0)` ensures USB CDC logs work cleanly without stalling when no terminal is attached.
- [`firmware/BmoGameboy/partitions.csv`](../firmware/BmoGameboy/partitions.csv) defines the `16 MiB` flash layout (hard stop pending [ADR 0013](decisions/0013-partition-layout.md)):

| Name | Type / SubType | Offset | Size | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| `nvs` | `data / nvs` | `0x9000` | `0x5000` (`20 KiB`) | Stores NVS settings (`bmo_disp/spi_hz` `40 MHz` vs `80 MHz` Safe Mode toggle) |
| `otadata` | `data / ota` | `0xe000` | `0x2000` (`8 KiB`) | OTA boot data slot |
| `app0` | `app / ota_0` | `0x10000` | `0x800000` (`8,388,608 B` / `8 MiB`) | Firmware binary slot (`85%` budget gate checked by `tasks.py size --check-budgets`) |
| `ffat` | `data / fat` | `0x810000` | `0x7E0000` (`8,257,536 B` / `~7.9 MiB`) | Internal FAT partition (`64 KiB` unallocated tail at `0xFF0000`) |

---

## 5. Host C++, WASM, Python & Simulator Flags

- **Host C++17 Suite (`CMakeLists.txt`):**
  - `BMO_HOST_TEST=1`: Automatically defined across all `30` `CTest` executables (`tests/host/fakes/` provides stub FreeRTOS, SPI, SD, GFX headers).
  - `-DBMO_ENABLE_ASAN_UBSAN=ON`: Enables `-fsanitize=address,undefined` on Linux/GCC/Clang (`python tasks.py check --sanitizers`).
  - `-DBMO_BUILD_BENCHMARKS=ON` (default `ON`): Builds `bmo_bench_host` (`benchmarks/bench_firmware_host.cpp`).
  - `app_behavior_test` enables `-DFEATURE_PROFILER=1` explicitly to exercise PSRAM ring/diagnostics code paths under host tests; MSVC Release keeps `/UNDEBUG /W4 /WX` so `BMO_CHECK` assertions never elide.
- **WebAssembly Cores ([`scripts/build_wasm_cores.py`](../scripts/build_wasm_cores.py)):**
  - `WASI_SDK_PATH`: Overrides default WASI SDK discovery (`.tools/wasi-sdk-34.0-x86_64-windows` on Windows; `.tools/wasi-sdk-34.0-x86_64-linux` on CI). Caches incremental `.o` and `.d` dependency files under `build/`.
- **Python Script Environment Variables:**
  - `BMO_DATA_DIR`: Overrides default repo-relative ROM/box-art root (`games/`, `roms/boxart/`) inside [`repo_tools.py`](../scripts/repo_tools.py) and [`deploy_sd.py`](../scripts/deploy_sd.py).
  - `BMO_REQUIRE_TARGET_COMPILE=1`: Equivalent to `python scripts/validate_repo.py --require-target-compile`.
  - `ARDUINO_DIRECTORIES_DATA`: Custom Arduino data root checked by [`elf_analyzer.py`](../tools/guardian/core/elf_analyzer.py) when locating ELFs/tools.
  - `GITHUB_STEP_SUMMARY`: Auto-receives Markdown memory tables from `python tasks.py size`.
- **Browser Simulator URL Parameters ([`simulator/js/sim_controls.js`](../simulator/js/sim_controls.js)):**
  - `?state=splash|bmo|mascot|carousel|library|toppicks|museum|gameplay|dissect|internals` (plus 3D CAD preset keys), `&console=<0..14>`, `&game=<index>`.

---

## 6. CI & Release Pipelines (`.github/workflows/`)

- **[`ci.yml`](../.github/workflows/ci.yml)** (`ubuntu-latest`, SHA-pinned Actions, `permissions: contents: read`, [`requirements.lock`](../requirements.lock)):
  1. `lint`: Runs `python tasks.py check --no-tests --strict-lint`, `python -m tools.guardian audit --cppcheck`, `lizard -C 25 -L 100 --warnings_only ...`, and `python scripts/extract_claims.py --fail-on-false`.
  2. `host-tests`: Downloads SHA-256 verified WASI SDK `34`, builds & runs `30` CTest targets in Release + ASan/UBSan (`--sanitizers`), runs `149` `tests/` + `12` `tools/guardian/tests`, and verifies `python tasks.py bench --check`.
  3. `firmware-build`: Compiles all `7` profiles (`default`, `no-doom`, `mit`, plus dormant/debug/profiler checks) with `--warnings strict --fail-on-warnings`, checks `85%` flash/DRAM budgets on `default`, and uploads **only the `mit` artifact** (`ADR 0008`).
- **[`release.yml`](../.github/workflows/release.yml)** (tag `v*` / manual dispatch): Re-runs strict checks, compiles `default`, `no-doom`, and `mit`, and publishes **only** `bmo-handheld-esp32s3-mit.tar.gz`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, and `SHA256SUMS.txt`.

---

## 7. Build Tool Evaluation: Arduino CLI vs. PlatformIO vs. ESP-IDF

**Decision:** Keep **Arduino CLI + `sketch.yaml`**, wrapped by [`tasks.py`](../tasks.py) and `.github/workflows/ci.yml` ([ADR 0016](decisions/0016-build-system-evaluation.md)).

| Option | Strengths | Costs / Risks for This Repo | Recommendation |
| :--- | :--- | :--- | :--- |
| **Arduino CLI + `sketch.yaml`** | Native `.ino` + pinned `esp32:esp32@3.3.11` and Adafruit libraries in one 13-line file; lowest barrier for non-programmer owners & external contributors; supports `--only-compilation-database` and repeatable `--build-property` overrides. | Requires a small Python wrapper (`tasks.py`) for multi-profile matrices, `-Wshadow` strict warning filtering, and pre-push checks. | **Keep (Active).** Already working, pinned, and CI-enforced. |
| **PlatformIO** | Built-in multi-env INI runner and library manager. | Official `espressif32` PlatformIO stopped updating Arduino-ESP32 (`2.x`); using Arduino `3.3.11` / IDF `5.x` requires third-party `pioarduino` forks and duplicates `sketch.yaml` pins. | **Reject.** High risk of toolchain drift. |
| **Native ESP-IDF (CMake)** | Fine-grained `menuconfig`, component isolation, direct SPI/DMA ISR control, fast incremental `ninja` builds. | Requires porting Arduino drivers (`Wire`, `SPI`, `SD`, `FS`, `Preferences`, `Adafruit_GFX`, `Adafruit_ST7789`) or wrapping `arduino-esp32` plus writing CMake glue for 13 third-party emulators. | **Defer.** Revisit only if physical bench measurements show Arduino SPI/FreeRTOS overhead blocks 60 FPS after DMA bounce streaming. |
