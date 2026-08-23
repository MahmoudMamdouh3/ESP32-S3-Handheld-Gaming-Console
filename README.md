# GB-Emu-ESP32S3

A portable Game Boy handheld project built around an ESP32-S3 and a small set of supporting tooling scripts. The repository documents the wiring, the emulator integration, and the ROM asset pipeline needed to build and validate the firmware.

## Project overview

- Hardware bring-up for the ESP32-S3 and TFT display
- Button polling and menu UI implementation
- Multi-emulator architecture: Peanut-GB (Game Boy), Walnut-CGB (Game Boy Color), Agnes (NES), and DOOM (doomgeneric)
- Game selection menu dynamically loading ROMs from SD Card
- Custom ESP32-S3 memory mapping to run heavy engines completely in PSRAM

## Current status

- [x] Milestone 1: display + button testing
- [x] Milestone 2: SD card SPI interface bring-up
- [x] Milestone 3: emulator core bring-up (Peanut-GB, Walnut-CGB, Agnes NES, DOOM)
- [x] Milestone 4: SD Card game selection menu
- [x] Milestone 5: Deep performance optimizations (zero-wait input loops, cache-aligned state, dynamic PSRAM allocation for BSS tables)
- [x] Repo tooling is now portable, extensively tested, and versionable

## Repository structure

```text
.
├── README.md
├── IMPLEMENTATION_PLAN.md
├── scripts/
│   ├── benchmark_repo.py
│   ├── color_calc.py
│   ├── convert_roms.py
│   ├── fetch_covers.py
│   ├── process_games.py
│   ├── repo_tools.py
│   ├── test_runner.py
│   └── validate_repo.py
├── firmware/
│   ├── 01_display_button_test/
│   ├── 02_sd_card_test/
│   └── 03_emulator/
│       ├── 03_emulator.ino
│       ├── display_emu.cpp
│       ├── emu_peanut.cpp (Game Boy)
│       ├── emu_walnut.cpp (Game Boy Color)
│       ├── emu_nes.cpp (NES)
│       ├── emu_doom.cpp (DOOM)
│       ├── sd_card.cpp
│       └── src/
│           └── doom/ (doomgeneric engine source)
├── docs/
│   └── hardware-notes.md
└── tests/
    └── test_repo_tools.py
```

## Tooling workflow

Use the repo-root-based scripts instead of machine-specific hardcoded paths.

1. Validate the repo state:
   ```bash
   python scripts/validate_repo.py
   ```
2. Run the test suite:
   ```bash
   python -m unittest discover -s tests -v
   ```
3. Benchmark the validation pipeline:
   ```bash
   python scripts/benchmark_repo.py
   ```
4. Regenerate ROM and cover assets when the zip archives change:
   ```bash
   python scripts/process_games.py
   ```

## Firmware build notes

1. Install Arduino IDE with ESP32 board support.
2. Add the libraries: Adafruit ST7789 and Adafruit GFX.
3. Select the ESP32-S3 Dev Module board and set the flash/PSRAM mode appropriately.
4. Open the sketch in `firmware/03_emulator/03_emulator.ino` and flash it.

## Important repository conventions

- Game ROMs are dynamically loaded from the SD Card and are intentionally excluded from git.
- The DOOM engine uses dynamic `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` to load rendering tables into PSRAM, bypassing internal DRAM limits.
- All Python scripts should be run from the repo root or invoked via `python path/to/script.py`.

## Documentation

See `docs/hardware-notes.md` for board-level debugging notes and wiring constraints. The repo also includes the implementation plan in `IMPLEMENTATION_PLAN.md`.

MIT - see [LICENSE](LICENSE).
