# Handheld Button Mapping & Chord Reference

This document is the canonical reference for all physical button inputs, boot recovery keys, in-game hotkeys, and per-console mappings (`HOST_TESTED` via `tests/test_pin_snapshot.py` and `firmware/BmoGameboy/src/cores/*.cpp`).

---

## 1. Physical GPIO Assignments (`config.h`)

All buttons are wired active-low (`INPUT_PULLUP`: idle `HIGH = 1`, pressed `LOW = 0`) and polled via `GPIO_IN_REG` (`GPIO 0–31`) and `GPIO_IN1_REG` (`GPIO 32–48`) with a `25 ms` per-pin debounce in menu mode and `0 ms` direct-register reads during emulator gameplay.

| Button | Macro | GPIO | Physical State on Current Board |
| :--- | :--- | :--- | :--- |
| D-Pad Up | `BTN_UP` | `4` | Soldered (`INPUT_PULLUP`) |
| D-Pad Down | `BTN_DOWN` | `5` | Soldered (`INPUT_PULLUP`) |
| D-Pad Left | `BTN_LEFT` | `6` | Soldered (`INPUT_PULLUP`) |
| D-Pad Right | `BTN_RIGHT` | `7` | Soldered (`INPUT_PULLUP`) |
| Face A (Primary / Right) | `BTN_A` | `16` | Soldered (`INPUT_PULLUP`) |
| Face B (Secondary / Bottom) | `BTN_B` | `17` | Soldered (`INPUT_PULLUP`) |
| System Start | `BTN_START` | `18` | Soldered (`INPUT_PULLUP`) |
| System Select | `BTN_SELECT` | `21` | Soldered (`INPUT_PULLUP`) |
| Face X (Top Expansion) | `BTN_X` | `14` | Configured (`INPUT_PULLUP`), **not soldered on current 8-button board** |
| Face Y (Left Expansion) | `BTN_Y` | `42` | Configured (`GPIO_IN1_REG`), **not soldered on current 8-button board** |
| Left Shoulder | `BTN_L` | `47` | Configured (`GPIO_IN1_REG`), **not soldered on current 8-button board** |
| Right Shoulder | `BTN_R` | `48` | Configured (`GPIO_IN1_REG`), **not soldered on current 8-button board** |

> [!IMPORTANT]
> Because `X`, `Y`, `L`, and `R` are not soldered on the current 8-button board, **every critical system action** (Pause, Save State, Load State, Quit to Menu, Safe Mode 40 MHz SPI, Diagnostics, Favorites, and DMG Palette cycling) works using **only the 8 soldered buttons (`UP`, `DOWN`, `LEFT`, `RIGHT`, `A`, `B`, `START`, `SELECT`)**.

---

## 2. Boot-Time Recovery Keys

| Held Button(s) During Power-On / Reset | Action |
| :--- | :--- |
| **Hold `SELECT` or `B`** | **Safe Mode (`40 MHz` Display SPI):** Forces `TFT_SPI_HZ` to `40 MHz` and persists the setting in NVS (`bmo_disp/spi_hz`) so long perfboard jumpers can recover if `80 MHz` causes display corruption. |

---

## 3. System UI & Menu Controls

| Screen (`SystemState`) | `UP / DOWN / LEFT / RIGHT` | `A` | `B` | `START` | `SELECT` |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Boot Splash / Idle Mascot** | Wake to Console Menu | Open Console Menu | Open Console Menu | Open Console Menu | Open Console Menu |
| **Console Carousel (`STATE_CONSOLE_MENU`)** | `LEFT / RIGHT`: Previous / next console platform | Open Game List for selected console | Enter Idle BMO Mascot face | Open Diagnostics Screen | Open Console History Museum |
| **Console Museum (`STATE_CONSOLE_MUSEUM`)** | `LEFT / RIGHT`: Browse console history cards | Return to Console Carousel | Return to Console Carousel | — | Return to Console Carousel |
| **Game List (`STATE_GAME_MENU`)** | `UP / DOWN`: Prev / next game; `LEFT / RIGHT`: Page jump (`±5`) | Launch selected game | Back to Console Carousel | Toggle **Top Picks** filter | Toggle **Favorite** (`★`) on selected game |
| **Diagnostics (`STATE_DIAGNOSTICS`)** | `LEFT / RIGHT` (in SPI Stress view): Toggle `40 MHz` vs `80 MHz` Display SPI clock & save to NVS (`bmo_disp/spi_hz`) | Toggle **SPI Stress Pattern** | Exit Diagnostics back to Console Carousel | Retry SD mount (if unmounted) or rebuild SD library index | Toggle **Live Matrix Dashboard** vs **Frame Profiler** (`SELECT` outside SPI Stress view) |
| **In-Game Pause Menu (`STATE_PAUSE_MENU`)** | `UP / DOWN`: Select Resume / Save State / Load State / Screenshot / Quit | Confirm selected pause option (Quit requires second `A` press within `3 s`) | Resume gameplay immediately | Resume gameplay | — |

---

## 4. In-Game System Chords (All Emulator Cores)

Every emulator core supports the following chord combinations during active gameplay (`STATE_EMULATOR`):

| Button Chord | Action | Notes |
| :--- | :--- | :--- |
| **`SELECT + START`** | **Open Pause Menu** | Works on all 8-button boards (`Resume`, `Save State`, `Load State`, `Slot`, `Screenshot`, `Quit`) |
| **`SELECT + UP`** | **Quick Quit Prompt** | Opens Pause Menu pre-selected on `Quit` (press `A` twice within `3 s` to confirm; flushes `.sav` before teardown) |
| **`SELECT + A`** | **Quick Save State (`BSS2`)** | Saves current emulator state to active slot `1..5` (`250 ms` hotkey cooldown) |
| **`SELECT + B`** | **Quick Load State (`BSS2`)** | Restores saved emulator state from active slot `1..5` (`250 ms` hotkey cooldown) |
| **`SELECT + DOWN`** | **Cycle DMG 4-Shade Palette** | Cycles the 5 hardware palettes on Game Boy / Game Boy Color |
| **`SELECT + LEFT`** | **Toggle A/B Turbo** | Toggles rapid-fire on `A` and `B` buttons |
| **`SELECT + RIGHT` (Hold)** | **Fast-Forward** | Skips the outer `FramePacing::targetFrameUs()` delay while held |

---

## 5. Per-Emulator Button Mapping

| Platform (`Emulator Core`) | D-Pad (`UP/DOWN/LEFT/RIGHT`) | `A` (`GPIO 16`) | `B` (`GPIO 17`) | `START` (`GPIO 18`) | `SELECT` (`GPIO 21`) | Expansion `X / Y / L / R` (When Soldered) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Game Boy DMG (`Peanut-GB`)** | D-Pad | `A` | `B` | `START` | `SELECT` (`SELECT + LEFT/RIGHT` cycles 5 DMG palettes) | `L/R`: Cycle DMG palette |
| **Game Boy Color (`Walnut-CGB`)** | D-Pad | `A` | `B` | `START` | `SELECT` | `L/R`: Cycle DMG palette (for DMG games) |
| **NES (`agnes`)** | D-Pad | `A` | `B` | `START` | `SELECT` | `X`: Turbo A, `Y`: Turbo B |
| **DOOM (`doomgeneric`)** | Move Forward / Back, Turn Left / Right | Fire (`Ctrl`) | Use / Open Door (`Space`) | Escape / DOOM Menu (`Enter` in menu) | Next Weapon (`1–7` cycle) | `X`: Run (`Shift`), `Y`: Automap (`Tab`), `L/R`: Strafe Left / Right |
| **Sega Master System / Game Gear (`SMS Plus`)** | D-Pad | Button `1` (`A`) | Button `2` (`B`) | `START` (GG) / `PAUSE` (SMS) | `PAUSE` (SMS) | — |
| **PC Engine / TurboGrafx-16 (`HuExpress`)** | D-Pad | Button `I` | Button `II` | `RUN` | `SELECT` | — |
| **Atari 2600 (`Stella 2014`)** | Joystick Up / Down / Left / Right | Fire | Fire (Alt) | Game Reset | Game Select | `L/R`: Left / Right Difficulty |
| **ColecoVision (`ColEm`)** | Joystick Up / Down / Left / Right | Left Fire (`Button 1`) | Right Fire (`Button 2`) | Keypad `1` (`Start`) | Keypad `#` / `*` | `X/Y`: Keypad `2`/`3` |
| **SNES (`Snes9x 2005`)** | D-Pad | `A` | `B` | `START` | `SELECT` | `X`: `X`, `Y`: `Y`, `L`: `L`, `R`: `R` (6-button games require expansion switches) |
| **Sega Genesis (`Gwenesis`)** | D-Pad | `C` | `B` | `START` | `A` (3-button mapping on 8-button board) | `Y`: `A`, `X`: `X`, `L`: `Y`, `R`: `Z` |
| **WonderSwan / Color (`Oswan`)** | `X1–X4` D-Pad | `A` | `B` | `START` | `OPTION` | `X/Y/L/R`: `Y1–Y4` cursor cluster |
| **Neo Geo Pocket / Color (`RACE`)** | Joystick | `A` | `B` | `OPTION` | `OPTION` | — |
| **Atari Lynx (`Handy`)** | D-Pad | `A` (Outer) | `B` (Inner) | `PAUSE` | `OPTION 1` | `X/Y`: `OPTION 1 / 2` |
| **PICO-8 (`Tac08`)** | D-Pad (`⬅➡⬆⬇`) | `❎` (`X` / Button 1) | `🅾️` (`O` / Button 0) | Pause Menu | Pause Menu | `X`: `❎`, `Y`: `🅾️` |
