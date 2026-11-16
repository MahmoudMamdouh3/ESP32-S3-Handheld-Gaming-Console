# GitHub Copilot Repository Instructions

Follow [`AGENTS.md`](../AGENTS.md) and [`firmware/AGENTS.md`](../firmware/AGENTS.md) as the canonical engineering standards for this repository.

## Mandatory Hard Stops
- Preserve `FEATURE_BATTERY_MONITOR=0`, `FEATURE_AUDIO=0`, `FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi`, and the soldered GPIO mapping (`pins_check.h` / `test_pin_snapshot.py`).
- Never flash hardware (`arduino-cli upload`, `esptool`, `tasks.py flash`), write to `games/`, `roms/`, `sdcard/`, `.rom_cache/`, or `Book/`, or commit blobs `>= 1 MiB`.
- Keep all 3 firmware profiles (`default`, `no-doom`, `mit`) building cleanly with `BMO_ENABLE_BAKED_ROMS=0` and strict warnings.

## AI Anti-Regression Rules
- **No blocking loops:** Never use `delay()` or `ets_delay_us()` inside frame loops or emulators.
- **PSRAM discipline:** Never allocate with `new`/`malloc`/`std::string`/`std::vector` in emulators or UI; use `hal/psram_alloc.h` and keep stack buffers `<= 1,024 B`.
- **Per-function SPI locks:** Every SD/TFT/SPI helper must hold `SpiArbiter::ScopedLock` across all return paths.
- **Single sources of truth:** Read enclosure/perfboard geometry (`8.80 × 12.40 × 3.20 cm`, `31 × 46`) from `hardware/layout.json`; read UI colors from `design/tokens.json` / `ui/theme.h`.
- **Honest evidence & real tests:** Never invent fallback telemetry/ELF numbers; write compiled C++ (`tests/host/`) or real JS/Python behavioral tests rather than source-regex checks.
- **Module limits:** Target `<= 400` SLOC/file (`1,000` cap) and `<= 60` SLOC/function (`100` cap) with strict typing (`-Wall -Wextra -Wshadow -Werror`, `ruff`, `mypy`, `tsc`).

## Verification Gates
Run `python tasks.py format --check`, `python tasks.py check`, `python tasks.py bench --check`, `python tasks.py build --profile all --warnings strict --fail-on-warnings`, `python tasks.py size --check-budgets`, and `python scripts/sync_ai_codemap.py --check`.
