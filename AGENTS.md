# Repository Agent Guide

This repository contains ESP32-S3-N16R8 handheld firmware (`firmware/BmoGameboy/`, `firmware/bringup/`), host tests (`tests/`), a WebAssembly simulator (`simulator/`), CAD/layout SSOT (`hardware/`), benchmarks (`benchmarks/`), and static analyzers (`tools/guardian/`).

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
1. **Stop** and do not modify `config.h`/`pin_map.h`, `pins_check.h`, `partitions.csv`, or `sketch.yaml`.
2. Write or update an ADR in [`docs/decisions/`](docs/decisions/README.md) with rationale, math, and rollback plan.
3. Add a plain-English entry to [`docs/owner-decisions.md`](docs/owner-decisions.md).
4. Wait for explicit written owner approval before mutating the hard stop and updating `tests/test_pin_snapshot.py`.

---

## AI Anti-Regression & Performance Budgets

Never introduce or reintroduce any of these defect classes:
1. **No blocking delays in frame/RTOS loops:** Never call `delay()` or `ets_delay_us()` inside frame loops, UI ticks, or emulators (use non-blocking deadline timers or `vTaskDelayUntil`).
2. **No internal-DRAM heap churn in cores/UI:** Never allocate with plain `new`, `malloc`, `std::string`, or `std::vector` inside emulator wrappers or UI rendering; route buffers to Octal PSRAM via `hal/psram_alloc.h` (`<= 1,024 B` stack frames).
3. **Per-function SPI arbitration:** Every function performing `SD.*` or `SPI`/`TFT` I/O must directly hold `SpiArbiter::ScopedLock` across all return paths.
4. **Hardware SSOT dimensions:** Derive enclosure and perfboard dimensions dynamically from [`hardware/layout.json`](hardware/layout.json) (`8.80 × 12.40 × 3.20 cm`, `31 × 46` holes); never hardcode stale dimensions in simulator or CAD scripts.
5. **Zero fabricated metrics & zero fake tests:** Never return hardcoded fallback numbers when ELFs/toolchains are missing (`tools/guardian/core/elf_analyzer.py`), and never add source-regex tests in place of compiled C++/WASM/Python behavioral checks.
6. **Module size & complexity targets:** Target `<= 400` lines/file (`1,000` hard cap), `<= 60` lines/function (`100` hard cap), cyclomatic complexity `<= 15`, explicit unit suffixes (`_ms`, `_us`, `_hz`, `_bytes`, `_cm`, `_mm`), and before/after verification via `python tasks.py bench --check`.

---

## Verification Commands & Evidence Vocabulary

Always classify claims using these exact four levels and never upgrade one without running the underlying check:
1. `NOT_TESTED` — analytical, config inspection, or unexecuted path.
2. `HOST_TESTED` — verified on host CPU via Python/native C/C++/WASM tests (`tests/`, `benchmarks/`).
3. `TARGET_COMPILED` — compiled with `arduino-cli` for `esp32:esp32:esp32s3`.
4. `HARDWARE_TESTED` — observed physically on the soldered ESP32-S3 perfboard and logged in [`docs/hardware_measurements.md`](docs/hardware_measurements.md) (`0` recorded yet).

```bash
python tasks.py doctor
python tasks.py format --check
python tasks.py check --fast
python tasks.py check
python tasks.py test
python tasks.py bench --check
python tasks.py build --profile all --warnings strict --fail-on-warnings
python tasks.py size --check-budgets
python scripts/sync_ai_codemap.py --check
```

- Keep firmware buildable with `BMO_ENABLE_BAKED_ROMS=0` across `default`, `no-doom`, and `mit`; only `mit` is redistributable ([`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)).
- Keep UI tokens synchronized across `theme.h`, `simulator.css`, and `DESIGN.md` (`python scripts/verify_color_tokens.py`).
- Never commit staged blobs `>= 1,048,576` bytes or secret tokens (`.githooks/pre-commit`).
- See [`firmware/AGENTS.md`](firmware/AGENTS.md) for C/C++ RTOS/PSRAM rules and [`docs/decisions/README.md`](docs/decisions/README.md) for ADRs.
