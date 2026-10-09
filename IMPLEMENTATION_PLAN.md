# Implementation Plan

> **Current Focus:** Phase 5 (Real Core Implementations) to replace architectural stubs with cycle-accurate retro engines in PSRAM.

## Foundational Phases [COMPLETED]

- [x] **Phase 1: Stabilize the build environment** (Root-relative paths, CLI scripts, asset validation).
- [x] **Phase 2 (Legacy): Harden the ROM pipeline** (ROM integrity checks, deterministic headers, PROGMEM guards).
- [x] **Phase 2 (Async Engine): Display DMA & Dual-Core Asynchronous Blit Engine** (Core 0 worker task, SpiArbiter, double-buffered PSRAM canvases, 60 FPS non-blocking frame streaming).
- [x] **Phase 3: Non-Volatile Multi-Slot Save States & Cartridge Battery RAM Persistence** (`SaveManager`, slots 1–5, CRC32 verification, SpiArbiter concurrency, in-game pause menu UI & hotkeys).
- [x] **Phase 4: High-Speed Binary ROM Index Cache (`.bmo_index`) & 64×64 Box Art Cover Art Engine** (36-byte packed header, 72-byte entry records, CRC32 verification, fast alphabetical sort, 64x64 raw/BMP cover loader).

---

## Completed Phase: High-Speed Binary ROM Index Cache (`.bmo_index`) & 64×64 Box Art Cover Art Engine [COMPLETE]

### Milestone 4.1: Binary ROM Index Cache (`.bmo_index`) [COMPLETE]
- [x] 1. Designed compact binary index format (`rom_index.h`) with 36-byte packed header (`BmoIndexHeader`, magic `BMOIDX01`, version 1, CRC32, timestamp) and 72-byte entries (`BmoIndexEntry`).
- [x] 2. Integrated index cache in `SDCard` (`loadIndex()`, `saveIndex()`, `rebuildIndex()`) enabling O(1) boot enumeration (< 15 ms vs 3500 ms FAT directory crawl).
- [x] 3. Added case-insensitive alphabetical sorting (`sortRoms()`) to organize SD ROM catalogs cleanly for instant A–Z jumping.
- [x] 4. Wrapped all index operations with `SpiArbiter` mutex lock for full dual-core thread safety.

### Milestone 4.2: 64×64 Box Art Cover Art Engine [COMPLETE]
- [x] 1. Implemented `BoxArt` engine (`box_art.h`, `box_art.cpp`) with 8,192-byte PSRAM buffer (`MALLOC_CAP_SPIRAM`, 0 internal SRAM consumed).
- [x] 2. Supported both 64×64 uncompressed raw 16-bit (`.raw`) and Windows 16/24-bit (`.bmp`) images from `/boxart/` and `/covers/` on MicroSD.
- [x] 3. Integrated box art rendering into `DisplayEmu::drawGameSelectMenu` with centered retro rounded border, and smooth fallback to console badge when art is absent.
- [x] 4. Wired single-load caching into `BmoGameboy.ino` so cover art is loaded only when `selectedGameIndex` changes (< 2 ms load time, 12 µs frame blit).

### Milestone 4.3: Host Unit Testing & Verification [COMPLETE]
- [x] 1. Created test suites `tests/test_rom_index.py` (8 test cases) and `tests/test_box_art.py` (6 test cases). Total suite: 51/51 tests passing.
- [x] 2. Full AI Guardian CI validator clean pass (`python scripts/validate_repo.py` - Flash 33.1%, SRAM 75.2%).
- [x] 3. Zero critical findings in Guardian performance audit (`python -m tools.guardian audit`).

---

## Phase 5: Tier 1 Real Core Implementations [IN PROGRESS]

### Milestone 5.1: Memory & Execution Layout Assessment [COMPLETE]
- [x] 1. Profiled 8MB Octal PSRAM budget and DMA bandwidth for target cores (PCE ~73KB, Genesis ~350KB, WonderSwan ~100KB, SNES ~500KB; base footprint < 7% of 8MB, leaving > 7.4MB headroom).
- [x] 2. Defined unified `EmulatorCoreContract` (`begin`, `runFrame`, `updateJoypad`, `destroy`, `saveState`, `loadState`, `saveBatteryRam`, `loadBatteryRam`) per Rule 12.
- [x] 3. Expanded `SaveManager::CoreId` with all 14 console types (`CORE_PCE = 6`, `CORE_ATARI = 7`, `CORE_PICO8 = 8`, `CORE_GENESIS = 9`, etc.).
- [x] 4. Wired PCE core persistence into `BmoGameboy.ino` (`autoSaveCurrentBatteryRam`, `quickSaveState`, `quickLoadState`, transparent boot auto-load).
- [x] 5. Added test coverage in `tests/test_save_manager.py` (52/52 tests passing).

### Milestone 5.2: Core Integration & Engine Bringup [IN PROGRESS]
- [x] 1. Bring up cycle-accurate PC Engine / TurboGrafx-16 HuC6280 CPU + VDC scanline engine in PSRAM replacing `PCE-Stub`.
- [x] 2. Bring up cycle-accurate Genesis / Mega Drive 68000 CPU + VDP core in PSRAM replacing `Genesis-Stub`.
- [ ] 3. Bring up WonderSwan / Color core in PSRAM replacing `WonderSwan-Stub`.

---

## Phase 6: Ultra-High-End Living BMO Mascot Engine (State-of-the-Art 2026 Edition) [COMPLETE]

> **Objective:** Completely overhaul the BMO mascot from the retro 128×128 pixelated scaffold into a museum-grade, 60 FPS, physics-driven living digital companion with 1:1 *Adventure Time* animation authenticity, native 320×240 subpixel anti-aliasing, critically damped harmonic springs, natural eye saccades, organic breathing, and full gamepad interactivity.

### Milestone 6.1: Native 320×240 PSRAM Vector & SDF Pipeline Architecture [COMPLETE]
- [x] 1. Replace 128×128 DRAM buffer + nearest-neighbor stretching with a native 320×240 Octal PSRAM vector canvas (`MALLOC_CAP_SPIRAM`, 0 internal DRAM consumed).
- [x] 2. Implement subpixel analytic anti-aliasing (~1.2 pixel smoothstep filter) across all geometry, eliminating all jagged edges and pixelated artifacts.
- [x] 3. Design 1:1 authentic cartoon feature primitives:
  - Deep glossy black eyes with dynamic specular catchlights moving with gaze vector.
  - Expressive composite mouth with dark inner cavity (`#1B2E2B`), warm pink/coral tongue (`#FA7F8F`), and high-order polynomial lip curvature.
  - Soft glowing blushing cheeks (`#FF8BA7`) with smooth radial gradient falloff and emotional intensity pulsing.
  - Authentic Adventure Time screen backdrop (`#8AD5C3` mint seafoam) with subtle rounded CRT bezel framing.
- [x] 4. Profile and optimize spatial bounding-box culling to guarantee < 4.5 ms per-frame compute on the 240MHz Xtensa LX7 dual-core MCU.

### Milestone 6.2: 60 FPS Harmonic Oscillator Physics & Organic Animation [COMPLETE]
- [x] 1. Implement second-order critically damped harmonic spring dynamics ($\zeta \approx 0.72$, $\omega_n \approx 20\text{ rad/s}$) for all facial features, delivering authentic cartoon squash-and-stretch.
- [x] 2. Integrate organic life simulation:
  - **Natural Eye Saccades:** Biologically realistic gaze wandering (randomized fixation intervals, rapid darts, curiosity tracking).
  - **Sinusoidal Breathing Cycle:** Gentle vertical bobbing ($y \pm 1.5\text{ px}$) and synchronous scale expansion ($T \approx 3.2\text{ s}$).
  - **Asymmetric Non-Linear Blinks:** Rapid shut (55 ms), brief dwell, elastomeric spring open, with occasional double-blinks.
  - **Ambient Particle Engine:** Floating alpha-blended `z Z` sleep bubbles in dreaming mode, musical notes, and sparkly celebration stars.

### Milestone 6.3: Interactive Living Mascot System & Rich 16-Expression Matrix [COMPLETE]
- [x] 1. Expand emotional states to a rich 16-expression matrix:
  - `IDLE_CONTENT`, `HAPPY_WINK`, `ECSTATIC_LAUGH`, `SURPRISED`, `CURIOSITY_LOOK`, `SLEEPY`, `DEEP_DREAMING`, `YAWNING`, `SMUG_COOL`, `MUSIC_GROOVING`, `LOVE_ENAMORED`, `DERP_PLAYFUL`, `DETERMINED`, `TICKLED_GIGGLE`, `GLITCH_CYBER`, `ERROR_PANIC`.
- [x] 2. Full gamepad interactivity in `STATE_IDLE_MASCOT` and dedicated interactive mode:
  - D-pad directional gaze tracking; rapid wiggles tickle BMO into squirming giggles.
  - Button A triggers celebratory wink and sparkle burst.
  - Button B triggers shy, warm rosy blush.
  - Select button cycles showcase animations; Start button wakes BMO with an energetic cheer.
- [x] 3. Dynamic idle screensaver escalation: soft gaze wandering (0–30s) → yawning transition (30–45s) → deep dreaming with floating sleep bubbles (>45s).

### Milestone 6.4: Host Emulation, Visual Verification & Guardian Benchmarks [COMPLETE]
- [x] 1. Build comprehensive Python test suite (`tests/test_bmo_mascot_engine.py`) verifying spring dynamics, expression state machine, and math precision.
- [x] 2. Benchmark on-host execution with `python -m tools.guardian bench-host` to verify real-time performance headroom.
- [x] 3. Synchronize all rules (`35_bmo_face_contract.md`, `15_performance_budgets.md`, `10_symbol_reference.md`), SDD v3.7, and agent manifest.




