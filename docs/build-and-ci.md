# Build System, Tasks CLI, and CI Architecture

All firmware builds, host checks, formatting, ELF size audits, and simulator launches are wrapped by [`tasks.py`](../tasks.py) on top of [`firmware/BmoGameboy/sketch.yaml`](../firmware/BmoGameboy/sketch.yaml) and [`arduino-cli`](https://arduino.github.io/arduino-cli/).

## Daily Commands

```powershell
# 1. One-time workstation bootstrap (installs Git hooks, downloads verified arduino-cli, syncs core + libs from sketch.yaml)
python tasks.py setup --install-arduino-cli

# 2. Fast local gate (< 1 min, no firmware compile): syntax, safety hard stops, #include case check, links, color tokens, Guardian audit
python tasks.py check

# 3. Full host unit test suite (C/C++ helpers, 11 WASI cores, Guardian, repo validators)
python tasks.py test

# 4. Firmware compile (default all 15 platforms, strict warnings on first-party code)
python tasks.py build
python tasks.py build --profile all          # default + no-doom + MIT + dormant audio/battery/debug compile-only matrix
python tasks.py build --compile-commands     # generate build/compile_commands.json for IDE/clangd

# 5. Binary footprint & PSRAM budget report
python tasks.py size

# 6. First-party code formatting (never touches src/third_party/)
python tasks.py format --check
python tasks.py format

# 7. Device flashing, serial monitor (115200 baud), and browser simulator
python tasks.py flash --port COM3
python tasks.py monitor --port COM3
python tasks.py sim
```

## Build Configuration Single Source of Truth (`sketch.yaml`)

- Board target & memory layout live in [`firmware/BmoGameboy/sketch.yaml`](../firmware/BmoGameboy/sketch.yaml) (`FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi`).
- Both CI (`.github/workflows/ci.yml`) and `tasks.py setup` parse core (`esp32:esp32`) and library pins directly from `sketch.yaml` rather than duplicating version numbers in scripts or workflow YAML.
- Never override `build.extra_flags` (which wipes out Arduino-ESP32 board macros such as `ARDUINO_USB_CDC_ON_BOOT` and PSRAM definitions). Always pass feature overrides through `compiler.c.extra_flags` and `compiler.cpp.extra_flags`.

## Dormant Hardware Safety Modules

- [`config.h`](../firmware/BmoGameboy/src/hal/config.h) defaults `FEATURE_AUDIO` and `FEATURE_BATTERY_MONITOR` to `0` (`#ifndef`-wrapped defaults keep checked-in values at literal `0`, satisfying hardware hard-stop guards).
- Enabling either dormant module for compile-only testing (`-DFEATURE_AUDIO=1` or `-DFEATURE_BATTERY_MONITOR=1`) requires an explicit matching hardware confirmation define (`-DBMO_HARDWARE_CONFIRMED_AUDIO=1` / `-DBMO_HARDWARE_CONFIRMED_BATTERY=1`), enforced by `#error` guards in [`audio_i2s.cpp`](../firmware/BmoGameboy/src/hal/audio_i2s.cpp) and [`battery.cpp`](../firmware/BmoGameboy/src/hal/battery.cpp).
- `ci.yml` compiles `FEATURE_AUDIO=1` (+ confirm), `FEATURE_BATTERY_MONITOR=1` (+ confirm), and `LOG_LEVEL=LOG_LEVEL_DEBUG` as compile-only matrix checks so unbuilt driver paths never silently rot while keeping release artifacts hard-stopped at `0`.

## Build Tool Evaluation: Arduino CLI vs. PlatformIO vs. ESP-IDF

**Decision:** Keep **Arduino CLI + `sketch.yaml`**, wrapped by [`tasks.py`](../tasks.py) and `.github/workflows/ci.yml`. Do **not** migrate to PlatformIO or native ESP-IDF CMake in Phase 4.

| Option | Strengths | Costs / Risks for This Repo | Recommendation |
| :--- | :--- | :--- | :--- |
| **Arduino CLI + `sketch.yaml`** | Native support for `.ino` + pinned `esp32:esp32@3.3.11` and Adafruit libraries in one 13-line file; lowest barrier for non-programmer owner & external contributors; supports `--only-compilation-database` and repeatable `--build-property` overrides. | Requires a small Python wrapper (`tasks.py`) for multi-profile matrices, warning flags (`--warnings all`), and pre-push checks; slower clean builds on Windows without cached `--build-path`. | **Keep (Recommended).** Already working, pinned, and understood. |
| **PlatformIO** | Built-in multi-env INI runner and library manager. | Official `espressif32` PlatformIO stopped updating Arduino-ESP32 (`2.x`); using Arduino `3.3.11` / IDF `5.x` requires third-party `pioarduino` forks, duplicating library pins outside `sketch.yaml` and introducing toolchain drift. | **Reject.** High risk of toolchain/core mismatch vs. `sketch.yaml`. |
| **Native ESP-IDF (CMake)** | Fine-grained `menuconfig`, component isolation, direct SPI/DMA ISR control, fast incremental `ninja` builds. | Requires porting Arduino drivers (`Wire`, `SPI`, `SD`, `FS`, `Preferences`, `Adafruit_GFX`, `Adafruit_ST7789`) or wrapping `arduino-esp32` as an IDF component plus writing CMake glue for 13 third-party emulators; steep contributor friction. | **Defer.** Revisit only if physical bench measurements show Arduino SPI/FreeRTOS overhead blocks 60 FPS after Phase 7 async buffering. |
