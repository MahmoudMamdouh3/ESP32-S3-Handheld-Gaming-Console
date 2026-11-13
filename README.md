<div align="center">

# BMO Handheld Firmware and Tools

[![ESP32-S3](https://img.shields.io/badge/MCU-ESP32--S3--N16R8-blue.svg)](https://www.espressif.com/)
[![Flash](https://img.shields.io/badge/Flash-16MB%20QIO-orange.svg)]()
[![PSRAM](https://img.shields.io/badge/PSRAM-8MB%20OPI-purple.svg)]()
[![Display](https://img.shields.io/badge/Display-ST7789%20320x240-teal.svg)]()

</div>

This repository contains Arduino-ESP32 firmware, fourteen emulator wrappers for fifteen platform selections, host checks, a browser simulator, and CAD/hardware documentation for a custom ESP32-S3-WROOM-1 N16R8 handheld console ("BMO Handheld"). Eight physical buttons are recorded as wired. X, Y, L, and R are firmware inputs that are not soldered on the current perfboard build.

The browser simulator, Wokwi model, host C runners, and target firmware are separate execution environments. Their evidence does not establish hardware behavior for the permanent perfboard.

> **Disclaimer:** BMO Handheld is an unofficial, non-commercial DIY fan hardware and software project. "BMO" / *Adventure Time* is a trademark of Cartoon Network / Turner Broadcasting System; "Game Boy" and related platform names referenced for file compatibility are trademarks of their respective owners. This repository is unaffiliated with and not endorsed by any console manufacturer or rights holder.

## Start here

- [Software Design Document](docs/software-design-document.md): system model, source ownership, hardware facts, storage, validation, and open limits.
- [Hardware notes](docs/hardware-notes.md): recorded assembly and measurement procedure.
- [Part 1: Simulator and hardware model](docs/readme/details-01.md)
- [Part 2: Systems, controls, build, and checks](docs/readme/details-02.md)
- [Third-party notices and licensing](THIRD_PARTY_NOTICES.md): upstream components, Three.js bundle provenance, and `BMO_ENABLE_DOOM` GPL-2.0-or-later notice.
- [Decision records](docs/decisions/README.md)

## Build and check

The target profile uses `FlashMode=qio`, `FlashSize=16M`, the custom partition table, and `PSRAM=opi`. Install Arduino CLI, the Espressif ESP32 core version listed in the CI workflow, Adafruit GFX, and the Adafruit ST7735/ST7789 library before compiling.

```powershell
# Default public MIT firmware build (no baked ROMs, no DOOM GPL engine):
.\arduino-cli.exe compile --profile esp32s3 firmware\BmoGameboy

# Optional local GPL-2.0-or-later DOOM-enabled build (only distribute binaries under GPL-2.0+):
.\arduino-cli.exe compile --profile esp32s3 --build-property compiler.c.extra_flags=-DBMO_ENABLE_DOOM=1 --build-property compiler.cpp.extra_flags=-DBMO_ENABLE_DOOM=1 firmware\BmoGameboy

python scripts/validate_repo.py
python -m unittest discover -s tests -v
```

A passing host suite does not replace a successful Arduino target compile. Neither test type proves physical electrical behavior.

## ROMs and licensing

- First-party source code in this repository is licensed under the [MIT License](LICENSE).
- No commercial ROMs, copyrighted artwork packs, or pre-installed game payloads are shipped in this repository. Users supply their own legally obtained ROM files on a FAT32 MicroSD card (`BMO_ENABLE_BAKED_ROMS` defaults to `0`).
- The optional DOOM engine (`src/vendor/doom/`) is licensed under GPL-2.0-or-later and is disabled by default (`BMO_ENABLE_DOOM=0`). Any firmware binary compiled with `BMO_ENABLE_DOOM=1` is governed as a whole by GPL-2.0-or-later. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
