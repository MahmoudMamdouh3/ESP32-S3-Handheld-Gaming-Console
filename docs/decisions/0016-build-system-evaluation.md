# ADR 0016: Build System Evaluation (`arduino-cli` + `sketch.yaml` vs `PlatformIO` vs Native `ESP-IDF`)

- **Status:** Accepted
- **Evidence Level:** `HOST_TESTED` / `TARGET_COMPILED`
- **Source:** `firmware/BmoGameboy/sketch.yaml`, `tasks.py`, `.github/workflows/ci.yml`

## Context

The repository builds ESP32-S3 firmware through `arduino-cli` (`v1.1.1`) pinned via `firmware/BmoGameboy/sketch.yaml` (`esp32:esp32@3.3.11`, `Adafruit BusIO@1.17.4`, `Adafruit GFX Library@1.12.5`, `Adafruit ST7735 and ST7789 Library@1.11.0`) and orchestrated by `python tasks.py`. We evaluated whether migrating to **PlatformIO** or **native ESP-IDF (`idf.py` / CMake)** would improve reproducibility, build times, or beginner usability before hardware bring-up.

## Comparison Matrix

| Criterion | `arduino-cli` + `sketch.yaml` + `tasks.py` (Current) | `PlatformIO` (`platformio.ini`) | Native `ESP-IDF` (`idf.py` + `CMakeLists.txt`) |
| :--- | :--- | :--- | :--- |
| **Dependency Pinning** | Exact core (`esp32:esp32@3.3.11`) and library versions pinned in `sketch.yaml`; `arduino-cli` SHA-256 verified in `tasks.py` | Pinned via `platform_packages` / `lib_deps`, but PlatformIO registry lags Espressif Arduino 3.x releases (`pioarduino` community fork required for Arduino-ESP32 3.3.x) | Exact ESP-IDF git submodule / component manager lockfile (`dependencies.lock`) |
| **Profile Matrix (`default`, `no-doom`, `mit`, `bringup`)** | Supported via `tasks.py build --profile ...` and `firmware/bringup/sketch.yaml` | Supported via `[env:...]` sections | Requires custom `sdkconfig.defaults` per profile and porting Arduino `SD.h`/`SPI.h`/`Preferences.h` calls |
| **Compilation Database (`compile_commands.json`)** | Supported (`python tasks.py build --compile-commands` + `.clangd`) | Supported (`pio run -t compiledb`) | Native CMake output |
| **Owner / Beginner Ergonomics** | Single `arduino-cli.exe` binary + `python tasks.py`; also compatible with standard Arduino IDE 2.x | Requires PlatformIO Core virtualenv + VS Code extension | Steep learning curve (`idf.py menuconfig`, ESP-IDF environment activation) |

## Decision

1. **Retain `arduino-cli` + `sketch.yaml` + `tasks.py` (`Accepted`):**
   - `sketch.yaml` already pins `esp32:esp32@3.3.11` and every library version deterministically.
   - Standard PlatformIO's official `espressif32` platform does not natively ship Arduino-ESP32 `3.3.11` without third-party `pioarduino` URLs, which would increase supply-chain and maintenance risk.
   - Rewriting `SD.h`, `SPI.h`, `Preferences.h`, and `Adafruit_GFX` for native `ESP-IDF` before the first physical board bring-up would introduce unnecessary churn across 25+ first-party files.
2. **IDE & Language Server Support:** Commit `.clangd` (stripping Xtensa-only GCC flags `-mlongcalls`, `-fno-tree-switch-conversion`, `-fstrict-volatile-bitfields` that trip host `clangd`), `.vscode/settings.json`, `.vscode/tasks.json`, and `.devcontainer/devcontainer.json` so `clangd` and VS Code work out of the box after `python tasks.py build --compile-commands`.

## Consequences

- Zero build-system churn during hardware bring-up.
- Any future migration to native `ESP-IDF` or `PlatformIO` remains an explicit owner decision in `docs/owner-decisions.md`.
