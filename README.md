<div align="center">

# BMO Handheld Firmware and Tools

[![ESP32-S3](https://img.shields.io/badge/MCU-ESP32--S3--N16R8-blue.svg)](https://www.espressif.com/)
[![Flash](https://img.shields.io/badge/Flash-16MB%20QIO-orange.svg)]()
[![PSRAM](https://img.shields.io/badge/PSRAM-8MB%20OPI-purple.svg)]()
[![Display](https://img.shields.io/badge/Display-ST7789%20320x240-teal.svg)]()

</div>

Firmware, host test suite, browser WebAssembly simulator, and 3D-printable enclosure CAD for an ESP32-S3-N16R8 handheld console ("BMO Handheld"). Eight physical buttons are wired; `X`, `Y`, `L`, and `R` are configured in firmware but not soldered on the current perfboard build.

> **Evidence boundary:** Firmware targets are `TARGET_COMPILED` via `arduino-cli` and core helpers are `HOST_TESTED`. No physical device tests (`HARDWARE_TESTED`) have been recorded yet. Browser/Wokwi simulators do not verify physical SPI signal integrity, thermal limits, or soldered wiring.

> **Disclaimer:** Unofficial, non-commercial DIY fan project. "BMO" / *Adventure Time* is a trademark of Cartoon Network / Turner Broadcasting System; console names are trademarks of their respective owners.

## Documentation

| Topic | Document |
| :--- | :--- |
| Hardware pins, hard stops & SPI clock risks | [docs/hardware.md](docs/hardware.md) |
| System architecture & memory layout | [docs/architecture.md](docs/architecture.md) |
| Supported consoles, status & controls | [docs/compatibility.md](docs/compatibility.md) |
| Performance budgets & device measurements | [docs/performance.md](docs/performance.md) & [docs/hardware_measurements.md](docs/hardware_measurements.md) |
| Build system, tasks CLI & CI architecture | [docs/build-and-ci.md](docs/build-and-ci.md), [tasks.py](tasks.py) |
| Binary formats (`BMOIDX02`, `BSS1`, SD deployment) | [docs/formats.md](docs/formats.md) |
| 3D-printable enclosure & wiring guides | [docs/enclosure-3d-printing.md](docs/enclosure-3d-printing.md), [wiring/](docs/wiring/01-display-and-buttons.md) |
| Visual tokens & UI standards | [DESIGN.md](DESIGN.md) |
| Decision records (ADRs) & project roadmap | [docs/decisions/README.md](docs/decisions/README.md), [ROADMAP.md](ROADMAP.md) |
| Contributing, security & notices | [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) |

## Quick Start

### 1. Setup & Build Firmware (`TARGET_COMPILED`)

Requires Arduino CLI with `esp32:esp32@3.3.11`, `Adafruit BusIO@1.17.4`, `Adafruit GFX Library@1.12.6`, and `Adafruit ST7735 and ST7789 Library@1.11.0` (`firmware/BmoGameboy/sketch.yaml` enforces `FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi`). Use [`tasks.py`](tasks.py) or invoke `arduino-cli` directly:

```powershell
# Bootstrap hooks, Python dependencies, and pinned Arduino core/libraries:
python tasks.py setup

# Default firmware build (all 15 platforms active; BMO_EXPERIMENTAL_CORES=1, BMO_ENABLE_DOOM=1; GPL-2.0-or-later):
python tasks.py build
.\arduino-cli.exe compile --profile esp32s3 firmware/BmoGameboy

# Build without DOOM (all 14 other upstream third-party cores active):
python tasks.py build --profile no-doom

# Build minimal permissive MIT core subset only (GB, GBC, NES active):
python tasks.py build --profile mit
```

### 2. Run Fast Checks & Host Tests (`HOST_TESTED`)

```powershell
# Fast pre-push checks (< 1 min, no firmware compile):
python tasks.py check

# Full host test suites & repository validator:
python tasks.py test
```

### 3. Browser Simulator & 3D Internals Teardown

Serve locally on `127.0.0.1:8080` (automatically builds `simulator/bmo_playtest_cores.wasm` and `simulator/bmo_playtest_cores.b64.js` if missing, or run `scripts/build_wasm_cores.py` first before opening `simulator/index.html` over `file://`):

```powershell
python simulator/run_simulator.py
# Or generate WASM explicitly:
python scripts/build_wasm_cores.py
```

Click **BMO Mode** (`?state=bmo`) for the interactive 18-expression mascot test bench or **Dissect / Internals** (`?state=dissect`) for the animated CAD shell disassembly and 22-wire jumper guide. Load your own legally obtained `.gb`, `.nes`, `.a26`, or `.col` ROMs in the browser.

## Licensing & ROM Policy

- Combined default firmware distributions (`BMO_EXPERIMENTAL_CORES=1`, `BMO_ENABLE_DOOM=1`) are licensed under [GNU GPL v2 or later](LICENSE) because they integrate upstream copyleft emulator cores in `firmware/BmoGameboy/src/third_party/`. First-party host/utility scripts and the `-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0` GB/GBC/NES core subset contain only permissive MIT-compatible code.
- No commercial ROMs, copyrighted box art, or ROM downloader scripts exist in this repository (`BMO_ENABLE_BAKED_ROMS=0` by default). Supply your own legally obtained ROMs on a FAT32 MicroSD card via [`scripts/deploy_sd_complete.py`](scripts/deploy_sd_complete.py) and [`scripts/deploy_to_sd.py`](scripts/deploy_to_sd.py).
- Full upstream repository URLs, pinned commit SHAs, licenses, and local modifications for all 13 bundled engines are documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
