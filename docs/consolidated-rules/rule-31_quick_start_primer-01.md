# Section 31 — 31. Quick-Start Primer

Source: [`31_quick_start_primer.md`](../../.agents/rules/31_quick_start_primer.md).

# 31. Quick-Start Primer

## Hardware stops

- Target: ESP32-S3-N16R8, Quad-SPI flash and Octal-SPI PSRAM. Keep FlashMode=qio, PSRAM=opi, and the custom partition table.
- Preserve the existing wiring: GPIO 0/3/45/46 are strapping pins; GPIO33–37 are reserved for OPI PSRAM.
- Keep FEATURE_BATTERY_MONITOR=0 and do not read GPIO1 while no divider is installed.
- Keep FEATURE_AUDIO=0 while no I2S DAC is installed.
- Eight buttons are physically recorded as wired; X, Y, L, and R are configured but unsoldered. The display is configured for 80 MHz, which exceeds the ST7789VW's published 62.5 MHz serial-write timing. No clock comparison has been measured on the board.

Read 00_hard_stops.md, 01_hardware.md, and 27_codebase_map.md before firmware changes. Generate task context with python -m tools.guardian prompt-pack "<task>"; use python -m tools.guardian lookup "<symbol>" to check declarations.

## Runtime map

BmoGameboy.ino defines eight states: boot splash, console menu, console museum, game menu, emulator, pause menu, diagnostics, and idle mascot. The carousel has Favorites plus fifteen platform selections implemented by fourteen wrappers (SMS and Game Gear share one).

Core 0 runs the BMO_Display display task. The application loop owns launcher and emulator work. GB/GBC stream scanlines synchronously while holding the SPI arbiter; other wrappers generally submit complete frames to the display worker. This does not establish a measured frame rate.

SELECT + START opens pause. SELECT + UP opens pause with Quit selected; confirmation requires two A presses within three seconds. B/Start resumes, and other directional/Select input cancels. Confirmed exit saves supported battery RAM, drains display work, destroys wrapper state, frees the loaded ROM, and returns to the menu.

## Data and documentation

config.h owns pins/feature flags; sd_card.* owns SD catalog/index; save_manager.* owns persistent file formats; wrapper headers own per-core declarations. Use the Software Design Document, generated symbol pages, and active issue ledger for current details. Dated archive pages are historical snapshots.

## Required checks

    python -m unittest discover -s tests -v
    python -m tools.guardian verify
    python -m tools.guardian ui-audit
    python -m tools.guardian diff-audit
    python scripts/validate_repo.py

The validator has phases 0–6. Phase 0 skips with a warning if Arduino CLI is absent. A host test pass is not a target compile. No automated check proves physical hardware behavior. Update 04_known_issues.md, the dated handoff, and CHANGELOG.md; leave commits to the explicit task workflow.