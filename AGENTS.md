# Repository Agent Guide

This repository contains ESP32-S3-N16R8 handheld firmware (`firmware/BmoGameboy/`), host unit tests (`tests/`), a WebAssembly browser simulator (`tools/bmo_simulator/`), a parametric CAD generator (`tools/enclosure/`), and static firmware analysis helpers (`tools/guardian/`).

## Hardware Hard Stops (Permanent Until Physical Board Changes)

- `FEATURE_BATTERY_MONITOR` MUST stay `0` in `firmware/BmoGameboy/src/core/config.h` (no divider connected to GPIO 1).
- `FEATURE_AUDIO` MUST stay `0` in `firmware/BmoGameboy/src/core/config.h` (no I2S DAC fitted on GPIO 38/39/40).
- Flash/PSRAM profile MUST stay `FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi` (`firmware/BmoGameboy/sketch.yaml`, `partitions.csv` `app0 = 8 MB`). Never use `FlashMode=opi`.
- Pin map in `config.h` is fixed to the soldered board:
  - Shared FSPI (`SpiArbiter` required): `TFT_SCK=12`, `TFT_MOSI=11`, `SD_MISO=15`, `TFT_CS=10`, `TFT_DC=8`, `TFT_RST=9`, `SD_CS=13`.
  - Wired buttons (`INPUT_PULLUP`): `UP=4`, `DOWN=5`, `LEFT=6`, `RIGHT=7`, `A=16`, `B=17`, `START=18`, `SELECT=21`.
  - Configured but unsoldered buttons: `X=14`, `Y=42`, `L=47`, `R=48`.
  - Reserved (never assign): Strapping pins `0, 3, 45, 46` and Octal PSRAM pins `33–37`.
- Never write to or commit user data under `games/`, `roms/`, `sdcard/`, `.rom_cache/`, or `Book/`.

## Verification Commands & Evidence Vocabulary

Always classify claims using these exact four levels and never upgrade one without running the underlying check:

1. `NOT_TESTED` — analytical, config inspection, or unexecuted path.
2. `HOST_TESTED` — verified on host CPU via Python/native C/C++/WASM tests (`tests/`).
3. `TARGET_COMPILED` — compiled with `arduino-cli` for `esp32:esp32:esp32s3`.
4. `HARDWARE_TESTED` — observed physically on the soldered ESP32-S3 perfboard (none recorded as of 2026-11).

Key commands (Windows PowerShell):

```powershell
# Compile default MIT firmware (no baked commercial ROMs, no GPL DOOM engine)
.\arduino-cli.exe compile --profile esp32s3 firmware/BmoGameboy

# Compile with opt-in GPL DOOM engine (never set build.extra_flags; append to compiler.*.extra_flags)
.\arduino-cli.exe compile --profile esp32s3 --build-property compiler.c.extra_flags=-DBMO_ENABLE_DOOM=1 --build-property compiler.cpp.extra_flags=-DBMO_ENABLE_DOOM=1 firmware/BmoGameboy

# Host tests, validator, link & token parity checks
python -m unittest discover -s tests -v
python -m unittest discover -s tools/guardian/tests -v
python scripts/validate_repo.py
python scripts/check_markdown_links.py
python scripts/verify_color_tokens.py
```

## Coding & Data Policies

- Keep firmware buildable in a clean clone with `BMO_ENABLE_BAKED_ROMS=0` and `BMO_ENABLE_DOOM=0`.
- Do not add copyrighted ROM headers, artwork scrapes, downloader scripts, or binaries `>= 1 MiB` (`.githooks/pre-commit` rejects staged blobs `>= 1,048,576` bytes and secret tokens).
- Keep UI colors synchronized across `firmware/BmoGameboy/src/core/theme.h`, `tools/bmo_simulator/simulator.css`, and `DESIGN.md` (`python scripts/verify_color_tokens.py`).
- See [`firmware/AGENTS.md`](firmware/AGENTS.md) for C/C++ RTOS/PSRAM rules and [`docs/decisions/README.md`](docs/decisions/README.md) for ADRs.
