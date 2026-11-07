# Archived changelog (history archived 2026-11-07)

## [Milestone 17.1] - 2026-10-09 (UI/UX Ergonomics Phase: Harmonic Carousel Glide, Floating Toast HUD, and Fast A-Z Jump Indicator)
### Added
- **Kinetic Carousel Horizontal Spring Glide (`DisplayEmu::triggerCarouselGlide`)**:
  - Implemented 2nd-order harmonic spring damper physics ($\zeta = 0.72$, $\omega_n = 22.0\text{ rad/s}$) on `s_carouselOffset` and `s_carouselVel`.
  - Displaces card horizontally ($\pm 36\text{ px}$) on `LEFT`/`RIGHT` D-Pad input in `STATE_CONSOLE_MENU` with initial velocity $\mp 240\text{ px/s}$.
  - Settles smoothly within 15–18 frames (~250–300 ms) with elastic recoil and zero jitter.
  - Offsets adjacent carousel tabs (left and right) in synchrony for continuous depth perception.
- **Floating Toast HUD System (`DisplayEmu::showToast`, `DisplayEmu::hasActiveToast`)**:
  - Implemented 2nd-order harmonic spring descent physics ($\zeta = 0.75$, $\omega_n = 24.0\text{ rad/s}$) from $Y = -28$ down to $Y = 8$.
  - 230×22 rounded pill ($r = 11$, centered at $X = 45$) with dark interior, high-contrast colored border, status dot, and centered typography.
  - Automatically composited inside `DisplayEmu::writeMenuCanvas()` across all menus and overlays with 0 extra memory allocation.
  - Wired into `STATE_GAME_MENU` favorite star toggling (`★ ADDED TO FAVORITES` / `REMOVED FROM FAVORITES`) and `STATE_PAUSE_MENU` quick saving/slot cycling.
- **Alphabetical A–Z Jump Letter Badge (`DisplayEmu::showAlphaBadge`)**:
  - Implemented circular floating indicator badge (center: $X = 270, Y = 115$, radius $18\text{ px}$) rendered in `drawGameSelectMenu`.
  - Displays currently selected starting character with drop shadow and high-contrast typography during rapid alphabetical skipping (`LEFT`/`RIGHT` in `STATE_GAME_MENU`).
- **Footer Button Badges**:
  - Fixed conditional rendering in `DisplayEmu::drawConsoleSelectMenu` ensuring button pills (`[A] PLAY`, `[UP] BMO`, `[SEL] SPECS`, `[< >] SYSTEM`) are displayed whenever the SD card is mounted.
- **Unit Testing Suite Expansion (`tests/test_apple_nintendo_ui.py`)**:
  - 4 comprehensive tests verifying 2nd-order harmonic spring convergence, toast HUD spring descent and overshoot margins, button pill geometry and screen boundary safety, and alphabetical badge coordinates.
  - Expanded repository test suite to 71/71 passing unit tests.
### Verified
- `python scripts/validate_repo.py` → PASS (All 7 phases clean, Flash 33.3%, SRAM 65.4%).
- `python -m unittest discover tests` → 71/71 tests OK.

---

## [Milestone 17.0] - 2026-10-09 (Milestone 5.2.1: PC Engine HuC6280 CPU & VDC Scanline Core Integration)
### Added
- **PC Engine / TurboGrafx-16 Real Engine Bringup (`src/vendor/pce/pce.c`, `src/vendor/pce/pce.h`)**:
  - Replaced architectural blank-screen scaffold with functional Hudson Soft HuC6280 8-bit CPU interpreter (65C02 core + custom HuC instructions `CLA`, `CLX`, `CLY`, `SXY`, `SAX`, `SAY`, `ST0`, `ST1`, `ST2`, `TAM`, `TMA`, `BRA`, `CSL`, `CSH`).
  - Implemented 8KB MPR memory banking across 21-bit physical address space, supporting HuCard ROMs, 8KB Work RAM, and 2KB Battery-backed Backup RAM.
  - Implemented HuC6270 Video Display Controller (VDC) with background tile generator, 4-bitplane decoding from 64KB VRAM, and SATB hardware sprite rendering.
  - Implemented HuC6260 Video Color Encoder (VCE) with 512-color BGR565 pre-swapped palette output directly streaming to ST7789 display.
  - 262-scanline frame timing with ~455 CPU cycles/line, raster compare interrupts (RCR), and VBlank interrupts.
  - 100% Octal PSRAM allocation (`MALLOC_CAP_SPIRAM`), consuming 0 bytes of internal DRAM.
- **Master Platform Test Suite & Manifest Synchronization**:
  - Created `tests/test_pce_emu.py` (6 unit tests) verifying copier header detection, MPR banking, VDC auto-increment, VCE color conversion, resolution budgeting, and save state serialization.
  - Promoted `emu_pce` from `STUB_ENGINES` to `PRODUCTION_ENGINES` in `tests/test_all_tiers_validation.py`.
  - Updated `AGENT_MANIFEST.json` and `docs/software-design-document.md` to classify PC Engine as a production core.
### Verified
- `python scripts/validate_repo.py` → PASS (All 7 phases clean).
- `python -m unittest discover tests` → 67/67 tests OK.

---

## [Milestone 16.2] - 2026-10-09 (BMO Mascot Interactive Mode, Speech Bubbles, Typewriter Dialogue & Dance Party)
### Added
- **Interactive Mascot Mode (`STATE_IDLE_MASCOT` & `STATE_CONSOLE_MENU`)**:
  - Direct 1-button access: Pressing `UP` on the main console carousel immediately launches the BMO Mascot mode with initial greeting quote (`"Who wants to play video games?!"`).
  - Added `[UP] BMO` button pill badge to the console select menu footer alongside `[A] PLAY`, `[SEL] SPECS`, and `[< >] SYSTEM`.
  - Comprehensive interactive companion loop:
    * **D-Pad Directional Gaze**: BMO's eyes track D-Pad input in real time.
    * **Pet / Tickle Interaction (`petCompanion()`)**: Rapid D-Pad wiggling triggers squirming laughing bounce, increases happiness, and triggers dialogue: `"Hehehe! Stop it, that tickles!"`.
    * **[A] Wink Celebration**: Triggers playful one-eyed wink, 3 celebration star sparkles, and dialogue: `"BMO is camera ready! *wink*"`.
    * **[B] Bashful Blush**: Triggers deep glowing coral cheeks, bashful downward gaze, and dialogue: `"Oh, you are making BMO blush!"`.
    * **[SEL] Adventure Time Quote Cycler**: Cycles 7 iconic Adventure Time lines (`"Who wants to play video games?!"`, `"Yay! BMO is so happy to see you!"`, `"Sometimes life is scary, but we have games!"`, `"Yes, Finn. It goes in my butt."`, `"BMO chop! If this were a real attack, you'd be dead!"`, `"When bad things happen, I know you want to believe they're a joke."`, `"I am a little living boy!"`).
    * **[UP] Dance Party Toggle**: Starts/stops 125 BPM musical dance groove.
    * **[START] / Hold [B]**: Smoothly returns to console menu with happy expression.
- **Vector Speech Bubble Engine (`BmoFace`)**:
  - Procedural anti-aliased rounded speech card ($hw = 0.406, hh = 0.0875, r = 0.033$) with downward directional pointer tail pointing toward BMO's mouth, composited via vector SDF in PSRAM.
  - High-contrast pure white interior with `#101E2B` deep charcoal border.
  - Integrated into spatial bounding box culling for zero CPU waste when inactive.
- **Dynamic Typewriter Dialogue System (`BmoFace` & `DisplayEmu::drawIdleMascotScreen`)**:
  - Organic typewriter character reveal running at 25ms per character reveal.
  - Non-blocking expiration timer (`durationMs`) auto-dismissing dialogue bubbles after display.
  - High-contrast typography rendered inside the vector speech card.
- **Musical Dance Party Groove Engine (`BmoFace`)**:
  - 125 BPM tempo oscillator coupled to 2nd-order harmonic spring dampers:
    * Bounce Y: $0.09 \sin(\phi)$
    * Gaze X: $0.45 \sin(0.5\phi)$
    * Squash X: $1.0 + 0.12 \cos(\phi)$
  - Peak-beat detection spawning rhythm star sparkles at bounce apices.
- **Companion Relationship & Progression System**:
  - Tracks total pets, happiness rating (0-100), and companion level progression (`getBuddyLevel()`, `getHappiness()`, `petCompanion()`).
- **Unit Testing Suite Expansion**:
  - Added tests 07, 08, 09 to `tests/test_bmo_mascot_engine.py` verifying speech bubble geometry, typewriter timing, musical tempo phase integration, and companion level formulas.
  - Repo test suite expanded to 61/61 passing unit tests.
### Verified
- `python scripts/validate_repo.py` → PASS (All 7 phases clean, Flash 33.3%, SRAM 65.3%).
- `python -m unittest discover tests` → 61/61 tests OK.

---

## [Milestone 16.1] - 2026-10-09 (UI/UX Ergonomic Polish & Button Pill Interface)
### Added
- **UI/UX Research & Architectural Synthesis**:
  - Evaluated spatial continuity, spring-damper momentum, layered depth hierarchy, and floating HUD overlays.
  - Identified 6 core weak points in the existing firmware and established an upgrade roadmap.
- **Button Glyph System (`DisplayEmu::drawButtonPill`)**:
  - Replaced plain text instructions with rounded button badges (`[A]` Coral, `[B]` Mint, `[SEL]` Teal, `[< >]` Yellow) with bold typography.
  - Implemented across console selection menu, game selection library, and museum screens.
- **Library Scroll Track Indicator**:
  - Implemented dynamic horizontal scroll track and active thumb pill in `drawGameSelectMenu`, providing spatial position indicators across libraries of 500+ ROMs.
- **Floating Pill HUD (`DisplayEmu::drawPauseMenu`)**:
  - Replaced modal text block with rounded pill at $(X: 40, Y: 10, W: 240, H: 22)$ with high-contrast border and centered status message (`★ SAVED TO SLOT 1!`, `💾 BATTERY RAM SAVED!`), preserving footer control hints.
- **Ambient Mascot Integration in Menus**:
  - Connected header mini BMO to active navigation: BMO gazes toward the focused console card in `drawConsoleSelectMenu`, looks down at active cover art in `drawGameSelectMenu`, glances left/right on carousel paging, and triggers a joyful wink with star sparkles when starring a favorite game.
### Verified
- `python scripts/validate_repo.py` → PASS (All 7 phases clean, Flash 33.2%, SRAM 65.3%).
- `python -m unittest discover tests` → 58/58 tests OK.

---
