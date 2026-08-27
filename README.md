# GB-Emu-ESP32S3

A portable Game Boy handheld project built around an ESP32-S3 and a small set of supporting tooling scripts. The repository documents the wiring, the emulator integration, and the ROM asset pipeline needed to build and validate the firmware.

## Project overview

- Hardware bring-up for the ESP32-S3 and TFT display
- Button polling and menu UI implementation
- Multi-emulator architecture: Peanut-GB (Game Boy), Walnut-CGB (Game Boy Color), Agnes (NES), and DOOM (doomgeneric)
- Game selection menu dynamically loading ROMs from SD Card
- Custom ESP32-S3 memory mapping to run heavy engines completely in PSRAM

## Current Status / Milestones

*Note: Milestones distinguish between "Code Exists", "Physically Wired", and "Tested on Real Hardware".*

- [x] **Milestone 1:** ILI9341/ST7789 display bring-up, button test. (Code Exists, Physically Wired, Tested on Real Hardware)
- [ ] **Milestone 2:** SD Card module via shared SPI. (Code Exists. Hardware Pending / Not Tested)
- [x] **Milestone 3:** Peanut-GB / Walnut-CGB integration (Code Exists, Tested on Real Hardware without SD)
- [x] **Milestone 4:** Multi-platform architecture (Code Exists, Tested on Real Hardware without SD)
- [x] **Milestone 5:** Game Selection UI (Code Exists, Tested on Real Hardware without SD)
- [ ] **Milestone 6:** I2S Audio Subsystem MAX98357A I2S DAC. (Code Exists. Hardware Pending / Not Tested)
- [ ] **Milestone 7:** Portable Handheld Conversion LiPo + TP4056 + battery sensing on GPIO1. (Code Exists. Hardware Pending / Not Tested)
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
│   ├── 03_emulator/
│   │   ├── 03_emulator.ino
│   │   ├── agnes.c (NES Core)
│   │   ├── buttons.cpp
│   │   ├── config.h
│   │   ├── display_emu.cpp
│   │   ├── emu_peanut.cpp (Game Boy)
│   │   ├── emu_walnut.cpp (Game Boy Color)
│   │   ├── emu_nes.cpp (NES Wrapper)
│   │   ├── emu_doom.cpp (DOOM Wrapper)
│   │   ├── sd_card.cpp
│   │   ├── unit_tests.cpp
│   │   └── src/
│   │       └── doom/ (doomgeneric engine source)
│   └── 04_unit_tests/
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
3. Select **ESP32S3 Dev Module** with **16 MB flash**, **Custom** partition scheme,
   and **OPI PSRAM**. The supplied `firmware/03_emulator/partitions.csv` reserves
   an 8 MB application partition; the default 4 MB board profile is too small.
4. Open the sketch in `firmware/03_emulator/03_emulator.ino` and flash it.

Equivalent Arduino CLI build:

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,PSRAM=opi" firmware/03_emulator
```

## Important repository conventions

- Game ROMs are dynamically loaded from the SD Card and are intentionally excluded from git.
  (Note: Some tested ROMs like Mario Deluxe and Zelda Ages can be "baked" directly into flash via C-headers if desired).
- The DOOM engine uses dynamic `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` to load rendering tables into PSRAM, bypassing internal DRAM limits.
- All Python scripts should be run from the repo root or invoked via `python path/to/script.py`.

## Documentation

See `docs/hardware-notes.md` for board-level debugging notes and wiring constraints. The repo also includes the implementation plan in `IMPLEMENTATION_PLAN.md`.

MIT - see [LICENSE](LICENSE).
