# 42. Quality of Life (QoL), UI/UX & System Roadmap Ledger

**Purpose:** This document is the single ground-truth ledger for all Quality of Life (QoL) tools, UI/UX enhancements, emulator ergonomics, and developer utilities across the entire BMO Gameboy platform. It tracks active implementations, prioritizes backlog initiatives, and defines standards for handheld user experience.

---

## 1. Feature Lifecycle Status Definitions

Every feature or tool in this ledger is categorized into one of four states:
- `[IMPLEMENTED]`: Tested, verified on hardware / CI, and active in master firmware or tooling.
- `[IN PROGRESS]`: Actively under development or integration in the current cycle.
- `[BACKLOG / ROADMAP]`: Prioritized for future development; feasible with current hardware.
- `[BLOCKED / HARDWARE-DORMANT]`: Architected in code but dormant pending physical hardware rework (e.g., I2S audio DAC, battery divider resistors).

---

## 2. System-Wide Quality of Life & UI/UX Ledger

### 2.1 In-Game Gameplay & Emulator Ergonomics

| Feature / Tool | Status | Description | Target Component |
| :--- | :--- | :--- | :--- |
| **In-Game Quick Pause Overlay** | `[IMPLEMENTED]` | Non-blocking pause modal (`SELECT + START`) providing in-game options without full reset. | `src/core/display_emu`, `BmoGameboy.ino` |
| **DMG Runtime Palette Switcher** | `[IMPLEMENTED]` | On-the-fly switching between Classic Green, BMO Teal, Pocket Gray, Light Cyan, and Amber palettes. | `src/core/theme.h`, `src/emulators/` |
| **Fast-Forward Turbo Mode** | `[IMPLEMENTED]` | Bypasses 16.7ms frame pacing while holding `SELECT + RIGHT` to accelerate RPG dialog/grinding. | `BmoGameboy.ino` |
| **In-Game Screenshot Dump** | `[BACKLOG / ROADMAP]` | Captures 320×240 BGR565 framebuffer to `/screenshots/<game>_<timestamp>.bmp` on MicroSD. | `src/core/sd_card`, `display_emu` |
| **Multi-Slot Save States (1–5)** | `[IMPLEMENTED]` | Quick save / load state snapshots to SD `/saves/` with CRC32 verification and pause menu. | `src/core/save_manager`, `display_emu` |
| **Per-Core Button Remap & Turbo** | `[BACKLOG / ROADMAP]` | Custom A/B button orientation and auto-fire turbo rate per console. | `src/core/buttons` |
| **Custom Console Bezels (1:1 Mode)** | `[BACKLOG / ROADMAP]` | Pixel-perfect 160×144 rendering surrounded by authentic styled console artwork. | `src/core/display_emu` |

---

### 2.2 Launcher & System Navigation UI/UX

| Feature / Tool | Status | Description | Target Component |
| :--- | :--- | :--- | :--- |
| **Centralized Theme Engine** | `[IMPLEMENTED]` | Semantic colors, typography metrics, and 8px grid constants defined in `theme.h` per Rule 08. | `src/core/theme.h` |
| **Alphabetical Quick Jump (A–Z)** | `[IMPLEMENTED]` | D-Pad `LEFT`/`RIGHT` skips to next initial letter in game lists, bypassing 10-item paging. | `BmoGameboy.ino`, `display_emu` |
| **Clean Title Sanitizer** | `[IMPLEMENTED]` | Strips ROM clutter (`[!]`, `(USA)`, `(Rev 1)`) for clean on-screen UI presentation. | `src/core/display_emu` |
| **Hardware Diagnostics & Gamepad Test** | `[IMPLEMENTED]` | Real-time graphical button tester, free memory telemetry (DRAM/PSRAM/IRAM), and system uptime. | `STATE_DIAGNOSTICS`, `display_emu` |
| **Interactive Console Museum** | `[IMPLEMENTED]` | Rich historical specs, landmark games, and architecture notes for all 15 systems. | `STATE_CONSOLE_MUSEUM` |
| **Universal Multi-Console Favorites (★)** | `[IMPLEMENTED]` | Star games across all 15 platforms (`SELECT` toggle), auto-dispatch to matching core, SD persistence (`/favorites.txt`). | `src/core/sd_card`, `display_emu`, `BmoGameboy.ino` |
| **Virtual BMO Official Game** | `[IMPLEMENTED]` | Pre-loaded featured title with BMO Desktop, Guardians of Sunshine action platformer, and BMO Talk. | `src/assets/roms/virtual_bmo.h`, `sd_card` |
| **1:1 Carousel Card UI & Hardware Silhouettes** | `[IMPLEMENTED]` | 1:1 replica of web simulator carousel card with dedicated retro hardware silhouette pixel-art per system. | `src/core/display_emu`, `theme.h` |
| **Retro Pixel-Art Dynamic Battery Indicator** | `[IMPLEMENTED]` | Pixel-art battery shell with color-changing fill bars (Green > 50%, Gold 20-50%, Pulsing Red < 20%, Cyan Charge Pulse). | `display_emu`, `tools/bmo_simulator` |
| **Binary ROM Fast-Cache (`.bmo_index`)**| `[IMPLEMENTED]` | Binary cache reducing 2,000-ROM SD boot enumeration from ~3.5s to < 15ms. | `src/core/sd_card`, `src/core/rom_index` |
| **Recently Played History Playlist** | `[BACKLOG / ROADMAP]` | Dynamic playlist tracking last 10 games launched. | `src/core/sd_card` |
| **Box Art / Screenshot Preview** | `[IMPLEMENTED]` | Renders 64×64 cover thumbnail next to selected ROM from `/boxart/` or `/covers/`. | `src/core/box_art`, `src/core/display_emu` |
| **Folder Hierarchy Navigation** | `[BACKLOG / ROADMAP]` | Directory traversal for categorized ROM sets (e.g. `/roms/gb/rpg/`). | `src/core/sd_card` |

---

### 2.3 BMO Mascot Personality & Ambient UX

| Feature / Tool | Status | Description | Target Component |
| :--- | :--- | :--- | :--- |
| **Procedural 2D SDF Expressions** | `[IMPLEMENTED]` | Anti-aliased mathematical face rendering (`IDLE`, `HAPPY`, `SURPRISED`, `SLEEPY`, `ERROR`). | `src/core/bmo_face` |
| **Launch Celebration Expression** | `[IMPLEMENTED]` | Brief `HAPPY` expression beat on game launch before emulator handoff. | `BmoGameboy.ino` |
| **Ambient Living Screensaver (`STATE_IDLE_MASCOT`)** | `[IMPLEMENTED]` | 30s inactivity auto-sleep transitioning to full-screen dreaming BMO mascot (`DisplayEmu::drawIdleMascotScreen`) with any-button wake up. | `BmoGameboy.ino`, `display_emu` |
| **Ultra-High-End Living BMO Mascot Engine** | `[IN DESIGN / PHASE 6]` | 60 FPS physics-driven companion (native 320×240 PSRAM vector canvas, harmonic spring dynamics, organic gaze saccades, sinusoidal breathing, blush pulsing, particle engine, and gamepad interactivity). | `src/core/bmo_face`, `display_emu` |
| **Easter Egg / Konami Code Mini-Game** | `[BACKLOG / ROADMAP]` | Entering `↑ ↑ ↓ ↓ ← → ← → B A` in launcher unlocks built-in retro mini-game. | `BmoGameboy.ino` |

---

### 2.4 Desktop & Web Simulator Tools

| Feature / Tool | Status | Description | Target Component |
| :--- | :--- | :--- | :--- |
| **1:1 Handheld Web Simulator** | `[IMPLEMENTED]` | 320×240 canvas with ST7789 display simulation, CRT/LCD shaders, and console museum. | `tools/bmo_simulator/` |
| **On-Screen Mobile Touch Controls** | `[IMPLEMENTED]` | Virtual touch D-Pad and action buttons for tablet and mobile browser simulation. | `tools/bmo_simulator/` |
| **Live Theme & Palette Previewer** | `[IMPLEMENTED]` | Real-time switching of shell themes and DMG display palettes in the simulator. | `tools/bmo_simulator/` |
| **WebUSB / WebSerial SD Manager** | `[BACKLOG / ROADMAP]` | Drag-and-drop ROMs directly onto SD card over USB-C browser connection without card reader. | Web companion app |
| **Automated Cover Scraper & Converter**| `[BACKLOG / ROADMAP]` | Fetches box art from OpenVGDB and generates 16-bit BGR565 `.bmp` files. | `scripts/fetch_covers.py` |
| **WebAssembly Playtest Cores** | `[BACKLOG / ROADMAP]` | Compiles Peanut-GB / Agnes to Wasm for in-browser ROM verification before flashing. | `tools/bmo_simulator/` |

---

### 2.5 Developer Experience (DX) & AI Guardian Tools

| Feature / Tool | Status | Description | Target Component |
| :--- | :--- | :--- | :--- |
| **AI Guardian CI Validator** | `[IMPLEMENTED]` | Multi-phase compilation, static AST linting, bus physics, and ROM health checks. | `scripts/validate_repo.py` |
| **Guardian CLI Suite** | `[IMPLEMENTED]` | `audit`, `bus-calc`, `profile-elf`, `bench-host`, `index`, `route`, `lookup`. | `tools/guardian/cli.py` |
| **Flash Budget Visualizer (`budget`)** | `[IMPLEMENTED]` | CLI explorer showing `app0` partition breakdown, baked ROMs, and free headroom. | `tools/guardian/cli.py` |
| **Roadmap & QoL Explorer (`qol`)** | `[IMPLEMENTED]` | CLI command to query active, in-progress, and backlog features directly from terminal. | `tools/guardian/cli.py` |
| **Host CPU C++ Verification Harness** | `[IMPLEMENTED]` | Native Zig-compiled host test harness for desktop test execution without hardware. | `tools/host_test.cpp` |
| **Headless Visual Regression Testing** | `[BACKLOG / ROADMAP]` | Framebuffer hash verification across 50-frame golden runs in CI. | `tools/host_test.cpp` |
| **Real-Time Serial Telemetry GUI** | `[BACKLOG / ROADMAP]` | Desktop plotter graphing FPS, frame pacing jitter, and heap fragmentation in real time. | `tools/telemetry_plotter` |

---

### 2.6 Hardware-Dormant Features (Awaiting Physical Solder Rework)

| Feature / Tool | Status | Precondition | Safety Rule |
| :--- | :--- | :--- | :--- |
| **I2S Audio & Chiptune SFX Engine** | `[BLOCKED / HARDWARE-DORMANT]` | Physical MAX98357A / PCM5102A I2S DAC soldered to GPIO 4, 5, 6. | Rule 00: `FEATURE_AUDIO` must stay `0` |
| **Battery Fuel Gauge & Low Voltage Cutoff** | `[BLOCKED / HARDWARE-DORMANT]` | Physical 100k/100k voltage divider soldered to GPIO 1 with 100nF filter cap. | Rule 00: `FEATURE_BATTERY_MONITOR` must stay `0` |

---

## 3. UI/UX Design Standards for Future Additions

When designing new screens or overlays:
1. **Rule 08 Compliance:** All colors must be sourced from `src/core/theme.h`. No inline hex literals.
2. **Performance Budget:** Menu rendering must execute in `< 4ms` on ESP32-S3. Never allocate heap buffers per frame.
3. **Selection States:** Use outlines (`drawRoundRect`), never filled boxes, to maintain 60 FPS menu responsiveness.
4. **Non-Blocking Control:** All UI animations, carousels, and transitions must use non-blocking `millis()` deltas.
