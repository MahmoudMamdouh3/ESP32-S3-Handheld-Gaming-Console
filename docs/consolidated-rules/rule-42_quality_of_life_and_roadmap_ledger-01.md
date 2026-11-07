# Section 42 — 42. Quality of Life (QoL), UI/UX & System Roadmap Ledger (1/2)

Source: [`42_quality_of_life_and_roadmap_ledger.md`](../../.agents/rules/42_quality_of_life_and_roadmap_ledger.md).

# 42. Quality of Life (QoL), UI/UX & System Roadmap Ledger

**Purpose:** Track BMO handheld quality-of-life features, UI work, emulator controls, and developer tools.

**Project direction (user-stated, 2026-10-21):** Develop the handheld and its grounded tools together. Keep verification and hardware safety requirements in place.

**Status note:** `[IMPLEMENTED]` means present in code or tooling; it does not establish hardware verification. See Rules 06 and 04 for evidence and issue status.

---

## 1. Feature Lifecycle Status Definitions

Every feature or tool in this ledger is categorized into one of four states:
- `[IMPLEMENTED]`: Present and in use in firmware or tooling; this label alone is not a verification tier (see the status note above).
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
| **In-Game Screenshot Dump** | `[IMPLEMENTED]` | Captures BGR565 framebuffer to `/screenshots/<game>_<timestamp>.bmp` on MicroSD via `SaveManager::saveScreenshotBmp`. | `src/core/save_manager` |
| **Multi-Slot Save States (1–5)** | `[IMPLEMENTED]` | Quick save / load state snapshots to SD `/saves/` with CRC32 verification and pause menu across all 13 cartridge cores. | `src/core/save_manager`, `src/emulators/` |
| **Per-Core Button Remap & Turbo** | `[IMPLEMENTED]` | Custom A/B button orientation swap (`Buttons::setSwapAB`) and auto-fire turbo (`SELECT + LEFT` / `Buttons::toggleTurbo`). | `src/core/buttons`, `BmoGameboy.ino` |
| **Custom Console Bezels (1:1 Mode)** | `[IMPLEMENTED]` | Custom console bezel artwork rendered around sub-320×240 viewports (`DisplayEmu::drawConsoleBezel`, toggled via Pause Menu `RIGHT`). | `src/core/display_emu`, `BmoGameboy.ino` |

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
| **Battery Status UI** | `[IMPLEMENTED]` | Shows a gauge only when monitoring is enabled; current configuration shows USB/N/A and a not-fitted diagnostic. | `display_emu`, `tools/bmo_simulator` |
| **Binary ROM Fast-Cache (`.bmo_index`)**| `[IMPLEMENTED]` | Binary cache reducing 2,000-ROM SD boot enumeration from ~3.5s to < 15ms. | `src/core/sd_card`, `src/core/rom_index` |
| **Recently Played History Playlist** | `[IMPLEMENTED]` | Dynamic playlist tracking last 10 games launched (`SDCard::recordRecentLaunch`, persisted to `/history.txt`). | `src/core/sd_card`, `BmoGameboy.ino` |
| **Box Art / Screenshot Preview** | `[IMPLEMENTED]` | Renders 64×64 cover thumbnail next to selected ROM from `/boxart/` or `/covers/`. | `src/core/box_art`, `src/core/display_emu` |
| **Folder Hierarchy Navigation** | `[IMPLEMENTED]` | Recursive directory traversal up to depth 3 (`SDCard::scanDirectory`) for categorized ROM sets (e.g. `/roms/gb/rpg/`). | `src/core/sd_card`, `src/core/display_emu` |

---

### 2.3 BMO Mascot Personality & Ambient UX

| Feature / Tool | Status | Description | Target Component |
| :--- | :--- | :--- | :--- |
| **Procedural 2D SDF Expressions** | `[IMPLEMENTED]` | Anti-aliased mathematical face rendering (`IDLE`, `HAPPY`, `SURPRISED`, `SLEEPY`, `ERROR`). | `src/core/bmo_face` |
| **Launch Celebration Expression** | `[IMPLEMENTED]` | Brief `HAPPY` expression beat on game launch before emulator handoff. | `BmoGameboy.ino` |
| **Ambient Living Screensaver (`STATE_IDLE_MASCOT`)** | `[IMPLEMENTED]` | 30s inactivity auto-sleep transitioning to full-screen dreaming BMO mascot (`DisplayEmu::drawIdleMascotScreen`) with any-button wake up. | `BmoGameboy.ino`, `display_emu` |
| **BMO 2D SDF Mascot Renderer** | `[IMPLEMENTED]` | 60 FPS physics-driven mascot (native 320×240 PSRAM vector canvas, harmonic spring dynamics, gaze saccades, sinusoidal breathing, blush pulsing, particle engine, and gamepad interactivity). | `src/core/bmo_face`, `display_emu` |
| **Easter Egg / Konami Code Mini-Game** | `[IMPLEMENTED]` | Entering `↑ ↑ ↓ ↓ ← → ← → B A` in `STATE_IDLE_MASCOT` triggers BMO's celebratory dance party, wink, and "+30 LIVES" Easter egg toast. | `BmoGameboy.ino` |

---

### 2.4 Desktop & Web Simulator Tools

| Feature / Tool | Status | Description | Target Component |
| :--- | :--- | :--- | :--- |
| **1:1 Handheld Web Simulator** | `[IMPLEMENTED]` | 320×240 canvas with ST7789 display simulation, CRT/LCD shaders, and console museum. | `tools/bmo_simulator/` |
| **On-Screen Mobile Touch Controls** | `[IMPLEMENTED]` | Virtual touch D-Pad and action buttons for tablet and mobile browser simulation. | `tools/bmo_simulator/` |
| **Live Theme & Palette Previewer** | `[IMPLEMENTED]` | Real-time switching of shell themes and DMG display palettes in the simulator. | `tools/bmo_simulator/` |
| **WebUSB / WebSerial SD Manager** | `[IMPLEMENTED]` | Drag-and-drop ROM dropzone and WebSerial 115200-baud connection (`connectWebSerial`, `ingestUploadedRoms`) in web simulator. | `tools/bmo_simulator/` |
| **Automated Cover Scraper & Converter**| `[IMPLEMENTED]` | Generates 64×64 16-bit BGR565 `.raw` and 24-bit `.bmp` files and builds Libretro Thumbnail CDN URLs. | `scripts/fetch_covers.py` |
| **WebAssembly Playtest Cores** | `[IMPLEMENTED]` | Compiles `bmo_playtest_cores.wasm` (`scripts/build_wasm_cores.py`) and runs live 60 Hz RGB565 frame stepping, `.p8` cart parsing, and save states in `wasm_playtest_engine.js`. | `tools/bmo_simulator/`, `scripts/build_wasm_cores.py` |

---