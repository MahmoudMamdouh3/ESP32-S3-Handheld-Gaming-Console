# Project Ground-Truth Rules

**As of August 2026, this project has:**
- **Power:** USB-C power only. NO battery, NO TP4056 module, NO voltage divider on GPIO1.
- **Storage:** Internal flash memory only. NO SD card module is wired. All ROM data must come from C headers baked into flash via `PROGMEM`/`heap_caps_malloc`.
- **Audio:** NO MAX98357A DAC. NO I2S wiring at all.
- **Display & Input:** ST7789 display and 8 buttons ONLY. Wired on the shared FSPI bus (`SCK=12, MOSI=11, CS=10, DC=8, RES=9, BLK=3V3`).

## CRITICAL INSTRUCTIONS FOR FUTURE AI AGENTS

1. **DO NOT TRUST OTHER DOCS FOR HARDWARE REALITY:** `docs/software-design-document.md`, `hardware-notes.md`, and `README.md` describe aspirational/planned architectures. This `project-rules.md` file is the SINGLE persistent source of truth.
2. **FEATURE GATING IS MANDATORY:** Any code interacting with SD, Audio, or Battery MUST be wrapped in compile-time flags (`FEATURE_SD_CARD`, `FEATURE_AUDIO`, `FEATURE_BATTERY_MONITOR`) in `config.h`.
3. **NO SILENT/FATAL DEPENDENCIES:** Any code path that runs when a feature flag is disabled (`0`) must not hang, sleep, or crash waiting for hardware that isn't there. `Battery::update()` must not trigger deep sleep. `SD.begin()` must not be called.
4. **PERFBOARD WARNING:** Everything is currently soldered onto a permanent perfboard. Wiring mistakes or fatal code logic (like deep-sleep loops) are extremely costly to fix. Always audit and explicitly test code in isolated `.ino` sketches before integration.
