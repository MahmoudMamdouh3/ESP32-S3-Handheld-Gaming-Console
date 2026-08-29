# Project Ground-Truth Rules

**As of August 2026, this project has:**
- **Power:** USB-C power only. NO battery, NO TP4056 module, NO voltage divider on GPIO1.
- **Storage:** SD card module wired. Feature-gated behind `FEATURE_SD_CARD` in `config.h`.
- **Audio:** NO MAX98357A DAC. NO I2S wiring at all.
- **Display & Input:** ST7789 display and 8 buttons ONLY. Wired on the shared FSPI bus (`SCK=12, MOSI=11, CS=10, DC=8, RES=9, BLK=3V3`).

## CRITICAL INSTRUCTIONS FOR FUTURE AI AGENTS

1. **DO NOT TRUST OTHER DOCS FOR HARDWARE REALITY:** `docs/software-design-document.md`, `hardware-notes.md`, and `README.md` describe aspirational/planned architectures. This `docs/project-rules.md` file is the SINGLE persistent source of truth.
2. **FEATURE GATING IS MANDATORY:** Any code interacting with SD, Audio, or Battery MUST be wrapped in compile-time flags (`FEATURE_SD_CARD`, `FEATURE_AUDIO`, `FEATURE_BATTERY_MONITOR`) in `config.h`.
3. **NO SILENT/FATAL DEPENDENCIES:** Any code path that runs when a feature flag is disabled (`0`) must not hang, sleep, or crash waiting for hardware that isn't there. `Battery::update()` must not trigger deep sleep. `SD.begin()` must not be called.
4. **PERFBOARD WARNING:** Everything is currently soldered onto a permanent perfboard. Wiring mistakes or fatal code logic (like deep-sleep loops) are extremely costly to fix. Always audit and explicitly test code in isolated `.ino` sketches before integration.

---

## Repository Structure Conventions

This project uses a **single, clean repository structure** with no numbered milestone
or stage folders. The working tree always reflects the current project state.
Past states are retrievable via `git log`, branches, or tags.

### Layout

```
repo-root/
├── README.md
├── LICENSE
├── .gitignore
├── docs/                          ← project documentation and rules
│   ├── project-rules.md           ← THIS FILE — single source of truth
│   ├── hardware-notes.md
│   └── software-design-document.md
├── tools/                         ← build/asset tooling scripts (not firmware source)
│   └── convert.py
└── firmware/
    └── BmoGameboy/                ← Arduino sketch folder (MUST match .ino filename)
        ├── BmoGameboy.ino         ← only setup()/loop()/state dispatch lives here
        ├── partitions.csv
        └── src/
            ├── core/              ← hand-written, project-specific modules
            │   ├── config.h           (single source of hardware pin truth)
            │   ├── buttons.cpp / .h
            │   ├── battery.cpp / .h
            │   ├── sd_card.cpp / .h
            │   ├── fram_save.cpp / .h
            │   ├── audio_i2s.cpp / .h
            │   ├── display_emu.cpp / .h
            │   └── bmo_face.cpp / .h
            ├── emulators/         ← glue/integration code per emulated console
            │   ├── emu_peanut.cpp / .h   (Game Boy / DMG)
            │   ├── emu_walnut.cpp / .h   (Game Boy Color)
            │   ├── emu_nes.cpp / .h      (NES via Agnes)
            │   └── emu_doom.cpp / .h     (DOOM via doomgeneric)
            ├── vendor/            ← third-party libraries, kept near unmodified upstream
            │   ├── peanut_gb/     (peanut_gb.h, peanut_gb_config.h)
            │   ├── walnut_cgb/    (walnut_cgb.h)
            │   ├── agnes/         (agnes.c, agnes.h)
            │   └── doom/          (doomgeneric source tree under src/)
            ├── assets/            ← generated/baked binary-as-C-array data
            │   ├── bmo_assets.h   (SDF face constants, if any)
            │   └── roms/          (mario_deluxe.h, zelda_ages.h, aladdin.h, etc.)
            └── tests/
                ├── unit_tests.cpp
                └── unit_tests.h
```

### Rules

1. **No numbered milestone folders.** This project does NOT use
   `01_foo/`, `02_bar/` style folders. Past states are tracked via git
   history, branches, or tags — not living folders in the working tree.
   The working tree always reflects only the current state of the project.

2. **Third-party libraries go under `src/vendor/<library>/`.** Never place
   upstream/vendor code flat alongside project source files. If you add a
   new library (e.g. a new emulator core), create `src/vendor/<name>/` and
   put all upstream files there. Do not modify upstream files unless
   absolutely necessary; document any patch in a comment.

3. **Baked/generated binary-as-C-array data goes under `src/assets/`.** ROM
   headers, baked sprite data, and similar generated files belong in
   `src/assets/` (or `src/assets/roms/` for ROM headers), never flat
   alongside logic source files in `core/` or `emulators/`.

4. **Build and tooling scripts go under `tools/` at the repo root.** Scripts
   like `convert.py` that generate asset headers or perform build processing
   live in `tools/`, not inside the firmware source tree.

5. **`config.h` is the single source of hardware truth.** All pin numbers,
   feature flags, and board-specific constants must be defined there. No
   other file should hardcode a pin number or board constant.

6. **Feature flags are mandatory for optional hardware.** Code touching SD,
   Audio, or Battery must be wrapped in `#if FEATURE_*` guards from
   `config.h`. A disabled feature must compile to a no-op, not a runtime crash.
