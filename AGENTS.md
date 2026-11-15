# Repository Agent Guide

This repository contains ESP32-S3-N16R8 handheld firmware (`firmware/BmoGameboy/` and `firmware/bringup/`), host unit tests (`tests/`), a WebAssembly browser simulator (`simulator/`), a parametric CAD generator and wiring/layout files (`hardware/`), and static firmware analysis helpers (`tools/guardian/`).

- **Start Here for Context:** Read [`docs/ai/STATE.md`](docs/ai/STATE.md) (`<= 60` lines) and [`docs/ai/CODEMAP.md`](docs/ai/CODEMAP.md) (`<= 200` lines) before scanning files.
- **Context Discipline:** Never read `firmware/BmoGameboy/src/third_party/**`, `simulator/three.min.js`, `simulator/bmo_enclosure_meshes.js`, `build/**`, or `Book/**` in full. Use `git grep`, `head`, or symbol lookups.

---

## Hardware Hard Stops (Permanent Until Physical Board Changes)

- `FEATURE_BATTERY_MONITOR` MUST stay `0` in `firmware/BmoGameboy/src/hal/config.h` (no divider connected to GPIO 1).
- `FEATURE_AUDIO` MUST stay `0` in `firmware/BmoGameboy/src/hal/config.h` (no I2S DAC fitted on GPIO 38/39/40).
- Flash/PSRAM profile MUST stay `FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi` (`firmware/BmoGameboy/sketch.yaml`, `partitions.csv` `app0 = 8 MB`). Never use `FlashMode=opi`.
- Pin map in `config.h` (enforced by compile-time `static_assert` in `src/hal/pins_check.h` and `tests/test_pin_snapshot.py`) is fixed to the soldered board:
  - Shared FSPI (`SpiArbiter` required): `TFT_SCK=12`, `TFT_MOSI=11`, `SD_MISO=15`, `TFT_CS=10`, `TFT_DC=8`, `TFT_RST=9`, `SD_CS=13`.
  - Wired buttons (`INPUT_PULLUP`): `UP=4`, `DOWN=5`, `LEFT=6`, `RIGHT=7`, `A=16`, `B=17`, `START=18`, `SELECT=21`.
  - Configured but unsoldered buttons: `X=14`, `Y=42`, `L=47`, `R=48`.
  - Reserved (never assign): Strapping pins `0, 3, 45, 46`, SPI Flash pins `26–32`, and Octal PSRAM pins `33–37`.
- Never write to or commit user data under `games/`, `roms/`, `sdcard/`, `.rom_cache/`, or `Book/`.
- Never flash or upload to hardware (`arduino-cli upload`, `esptool`, `tasks.py flash`) unless explicitly instructed by the owner during an interactive session.

### Hard-Stop Change Protocol

If a task requires changing a pin assignment, partition table, flash/PSRAM mode, feature flag, or default SPI clock:
1. **Stop** and do not modify `config.h`, `pins_check.h`, `partitions.csv`, or `sketch.yaml`.
2. Write or update an ADR in [`docs/decisions/`](docs/decisions/README.md) with the technical rationale, electrical/memory math, and rollback plan.
3. Add a plain-English decision entry to `docs/owner-decisions.md`.
4. Wait for the owner's explicit written approval before editing the hard-stop value and updating `tests/test_pin_snapshot.py` in the same commit.

---

## Verification Commands & Evidence Vocabulary

Always classify claims using these exact four levels and never upgrade one without running the underlying check:

1. `NOT_TESTED` — analytical, config inspection, or unexecuted path.
2. `HOST_TESTED` — verified on host CPU via Python/native C/C++/WASM tests (`tests/`).
3. `TARGET_COMPILED` — compiled with `arduino-cli` for `esp32:esp32:esp32s3`.
4. `HARDWARE_TESTED` — observed physically on the soldered ESP32-S3 perfboard and logged in [`docs/hardware_measurements.md`](docs/hardware_measurements.md) (`0` recorded yet).

### Cross-Platform Task Runner (`python tasks.py`)

```bash
# Environment check & fast pre-commit gate (< 20 s)
python tasks.py doctor
python tasks.py check --fast

# Full host verification, C/C++ formatting, and unit test suite
python tasks.py format --check
python tasks.py check
python tasks.py test

# Compile all 3 firmware profiles (default, no-doom, mit) with strict warnings & budget gates
python tasks.py build --profile all --warnings strict --fail-on-warnings
python tasks.py size --check-budgets
```

### Direct `arduino-cli` Commands (Windows PowerShell Reference)

```powershell
# Compile default firmware (all 15 platforms active; personal-use local build; see THIRD_PARTY_NOTICES.md)
.\arduino-cli.exe compile --profile esp32s3 firmware/BmoGameboy

# Compile with cores opted out (never set build.extra_flags; append to compiler.*.extra_flags)
.\arduino-cli.exe compile --profile esp32s3 --build-property compiler.c.extra_flags=-DBMO_ENABLE_DOOM=0 --build-property compiler.cpp.extra_flags=-DBMO_ENABLE_DOOM=0 firmware/BmoGameboy
.\arduino-cli.exe compile --profile esp32s3 --build-property "compiler.c.extra_flags=-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0" --build-property "compiler.cpp.extra_flags=-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0" firmware/BmoGameboy

# Compile standalone first-power-on bring-up sketch
.\arduino-cli.exe compile --profile esp32s3 firmware/bringup
```

---

## Coding, Complexity & Data Policies

- **Definition of Done:** Every code or doc change must pass `python tasks.py format --check`, `python tasks.py check --fast`, and (when touching firmware) `python tasks.py build --profile all --warnings strict --fail-on-warnings` + `python tasks.py size --check-budgets`.
- **Complexity Budgets:** Keep first-party source files `<= 1,000` lines and new functions `<= 100` lines. Use explicit units in identifiers (`_ms`, `_us`, `_hz`, `_bytes`, `_cm`, `_mm`) and centimetres (`cm`) in all user-facing docs ([`docs/conventions.md`](docs/conventions.md)).
- **Clean-Clone Buildability & Licensing:** Keep firmware buildable with `BMO_ENABLE_BAKED_ROMS=0` across `default`, `no-doom`, and `mit` (`-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0`). Only the `mit` profile may be redistributed as a pre-built binary ([`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)).
- **No Copyrighted Assets or Large Blobs:** Do not add copyrighted ROM headers, artwork scrapes, downloader scripts, or binaries `>= 1 MiB` (`.githooks/pre-commit` rejects staged blobs `>= 1,048,576` bytes and secret tokens).
- **UI Token Parity:** Keep UI colors synchronized across `firmware/BmoGameboy/src/ui/theme.h`, `simulator/simulator.css`, and `DESIGN.md` (`python scripts/verify_color_tokens.py`).
- See [`firmware/AGENTS.md`](firmware/AGENTS.md) for C/C++ RTOS/PSRAM rules and [`docs/decisions/README.md`](docs/decisions/README.md) for ADRs.
