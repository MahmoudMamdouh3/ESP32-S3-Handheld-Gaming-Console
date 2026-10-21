# Embedded UI Frameworks & OSes — Not Yet Evaluated (Backlog Note)

> **Status:** UNEVALUATED. Logged 2026-10-21.
> This project has **never** evaluated, prototyped, or borrowed ideas from the options below.
> Searching the repo (firmware, rules, docs) for `lvgl`, `tactility`, `micropython` found
> no prior mention. The current custom UI is therefore **not** the result of a documented
> comparison. Whether that is the right call is an **open question**, not a settled decision.

## 1. Why this note exists
The UI (console menu, game menu, pause menu, Dynamic Island HUD, BMO face) was built
from scratch. No alternative was weighed. This note records the gap so a future agent or
the owner can test or learn from these projects instead of assuming the custom path is optimal.

## 2. Candidates

| Candidate | What it is | Relevance to this console |
| :--- | :--- | :--- |
| **LVGL** | C graphics/UI library (widgets, themes, animations, list scrolling, fonts). | Highest. It is a library, not an OS, so it could be adopted in part. |
| **Tactility** | Embedded OS for ESP32 touch-screen devices, with an app launcher. | Low as a platform. Possibly useful as a reference for app/launcher structure. |
| **MicroPythonOS** | MicroPython-based OS with an Android-style launcher and app store. | Low as a platform. Possibly useful as a UX reference. |

> [!NOTE]
> The descriptions above come from general knowledge and a user-supplied summary, not from
> a source reviewed in this repo. **Verify against each project's current docs before
> relying on any detail** (supported boards, memory footprint, licence, input model).

## 3. Preliminary concerns (hypotheses to test, not conclusions)
- **Platform swap risk:** Tactility and MicroPythonOS would replace the Arduino framework,
  the custom `partitions.csv`, and the verified `arduino-cli` build. Hardware rules in this repo
  show that wrong flash or PSRAM modes can brick the board.
- **Input model:** both target touch. This console has 8 GPIO buttons and no touch.
- **Native emulator cores:** the cores are C/C++ and need native speed, which does not suit MicroPython.
- **LVGL vs the display path:** frames stream via the Core 0 `vDisplayTask` with the
  `SpiArbiter` mutex shared with the SD card. LVGL has its own draw/flush buffers, so SPI
  contention and RAM use would need measuring.

## 4. What could be learned or borrowed (to test later)
- LVGL list/scroll/animation behaviour compared with the current carousel (spring-damper glide).
- LVGL's input-device abstraction compared with `Buttons` handling, for focus and navigation.
- LVGL theming/font handling for a future settings screen or file browser.
- Launcher structure from Tactility / MicroPythonOS, for any future "apps" concept.
- Possible **partial adoption**: LVGL only for non-gameplay screens (settings, browser),
  leaving the BMO face and emulator frame streaming untouched.

## 5. Suggested evaluation plan (no firmware changes until approved)
1. **Desk research:** confirm each project's current features, licence, and ESP32-S3 / SPI ST7789 support.
2. **Budget check:** estimate LVGL's DRAM/PSRAM and flash cost against `15_performance_budgets.md`
   and the flash budget in `37_rom_governance_and_flash_budget.md`.
3. **Isolated prototype:** one LVGL screen (e.g. settings list) on a branch, driven by `Buttons`,
   flushed through the existing `SpiArbiter` path. Measure frame time and heap.
4. **Compare** against the custom menus on frame time, memory, visual quality, and maintenance cost.
5. **Record the decision** here and in `04_known_issues.md`: adopt, adopt partially, or reject with data.

## 6. Hard constraints for any future prototype
- Do not touch GPIO1 / `FEATURE_BATTERY_MONITOR`, and keep `FEATURE_AUDIO` at `0`.
- Keep `Flash Mode: QIO 80MHz` + `PSRAM: OPI PSRAM`.
- Do not break the emulator exit contract or `SpiArbiter` ownership rules.
