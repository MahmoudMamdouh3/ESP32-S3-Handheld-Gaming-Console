<div align="center">

# BMO Gameboy firmware and tools

[![ESP32-S3](https://img.shields.io/badge/MCU-ESP32--S3--N16R8-blue.svg)](https://www.espressif.com/)
[![Flash](https://img.shields.io/badge/Flash-16MB%20QIO-orange.svg)]()
[![PSRAM](https://img.shields.io/badge/PSRAM-8MB%20OPI-purple.svg)]()
[![Display](https://img.shields.io/badge/Display-ST7789%20320x240-teal.svg)]()

</div>

This repository contains Arduino-ESP32 firmware, fourteen emulator wrappers for fifteen platform selections, host checks, a browser simulator, and design and hardware documentation for an ESP32-S3-WROOM-1 N16R8 handheld. Eight physical buttons are recorded as wired. X, Y, L, and R are firmware inputs but are not soldered on the current board.

The browser simulator, Wokwi model, host C runners, and target firmware are separate execution environments. Their evidence does not establish hardware behavior for the permanent perfboard.

## Start here

- [Software Design Document](docs/software-design-document.md): system model, source ownership, hardware facts, storage, validation, and open limits.
- [Hardware notes](docs/hardware-notes.md): recorded assembly and measurement procedure.
- [Part 1: Simulator and hardware model](docs/readme/details-01.md)
- [Part 2: Systems, controls, build, and checks](docs/readme/details-02.md)
- [Active issue registry](.agents/rules/04_known_issues.md)
- [Current audit handoff](docs/handoffs/2026-11-07-remediation.md)
- [Generated firmware symbol index](docs/generated/symbols.md)
- [Decision records](docs/decisions/README.md)

## Build and check

The target profile uses `FlashMode=qio`, `FlashSize=16M`, the custom partition table, and `PSRAM=opi`. Install Arduino CLI, the Espressif ESP32 core version listed in the CI workflow, Adafruit GFX, and the Adafruit ST7735/ST7789 library before compiling.

```powershell
python scripts/validate_repo.py
python -m unittest discover -s tests -v
python -m tools.guardian verify
python -m tools.guardian ui-audit
python -m tools.guardian diff-audit
```

A passing host suite does not replace a successful Arduino target compile. Neither test type proves physical electrical behavior. Current command results and remaining work are in the dated handoff.

## ROMs and licensing

The project owner will provide a separate ROM policy and project-license direction. Existing ROM-bearing assets were not changed in this remediation. Do not add or download game data or infer redistribution permission from the code license.
