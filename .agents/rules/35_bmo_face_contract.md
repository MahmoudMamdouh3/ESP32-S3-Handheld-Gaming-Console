# BmoFace Mascot Subsystem Contract
Purpose: consolidate every invariant governing the procedural SDF mascot
renderer into one place, the way `28_display_and_spi_contract.md` does for
the display. Previously scattered across the SDD, M-7, M-19, and the UI
style guide — this file is now the canonical source; those other files
reference it rather than restate it.

## Call-timing invariants
- `BmoFace::draw()` runs during `STATE_CONSOLE_MENU` and `STATE_GAME_MENU`.
  NEVER during active frame rendering in `STATE_EMULATOR` (mistake M-7) — calling
  `draw()` mid-frame corrupts the active SPI transaction.
- `BmoFace::update()` runs every `loop()` tick regardless of state
  (so blink and exponential-decay interpolation timers advance correctly
  even when not drawing).
- Every `BmoFace::draw()` call occurs strictly outside any open
  `DisplayEmu::startFrame()` / `endFrame()` pair (mistake M-19).

## Dirty-flag & caching contract
- `BmoFace::isDirty()` returns true if and only if an animated parameter
  (expression target, blink progress) changed since the last SDF render.
- `blitFace()` in `src/core/bmo_face.cpp` caches `faceBuf` (128×128): it only
  recomputes the 2D SDF (`renderFace()`, ~3ms) when `s_dirty` is true. On static
  frames, it reuses `faceBuf` to execute a fast ~0.4ms row-blit.
- In `STATE_CONSOLE_MENU` and `STATE_GAME_MENU`, `BmoFace::draw()` is called
  unconditionally after `writeMenuCanvas()`. This ensures the menu canvas's
  full-screen blit does not erase the mascot face on clean frames.

## Expression state-transition contract
- The 9 states are: `IDLE`, `HAPPY`, `SURPRISED`, `SLEEPY`, `LOW_BATTERY`,
  `CHARGING`, `ERROR`, `SHUTDOWN`, `HIDDEN` (SDD §6).
- `LOW_BATTERY` and `CHARGING` must never be reachable while
  `FEATURE_BATTERY_MONITOR=0` — all call sites passing these values are gated
  by `#if FEATURE_BATTERY_MONITOR`.
- All transitions interpolate via exponential decay (`k = EASE_RATE * dt`)
  EXCEPT `ERROR`, which snaps instantaneously without easing.

### Call Site Registry
| File & Line | Trigger Context | Target Expression |
|---|---|---|
| `BmoGameboy.ino:128` | Setup / boot splash initialization | `IDLE` |
| `BmoGameboy.ino:178` | Console Menu -> Game Menu navigation (`BTN_A`) | `IDLE` |
| `BmoGameboy.ino:220` | Game Menu -> Console Menu navigation (`BTN_B`) | `IDLE` |
| `BmoGameboy.ino:226` | Invalid ROM selection fallback | `IDLE` |
| `BmoGameboy.ino:273` | Emulator initialization failure fallback | `IDLE` |
| `BmoGameboy.ino:285` | Game launch celebration beat | `HAPPY` |
| `BmoGameboy.ino:332` | Emulator exit (`SELECT + UP`) return to menu | `IDLE` |
| `src/emulators/emu_walnut.cpp:114` | Walnut-CGB `gb_error` panic | `ERROR` |
| `src/emulators/emu_peanut.cpp:83` | Peanut-GB `gb_error` panic | `ERROR` |
| `src/core/battery.cpp:50` | Low battery shutdown (`FEATURE_BATTERY_MONITOR=1`) | `SHUTDOWN` |
| `src/core/battery.cpp:63` | Low battery warning (`FEATURE_BATTERY_MONITOR=1`) | `LOW_BATTERY` |

## Memory & rendering contract
- **Framebuffer Architecture:** Full 320×240 native canvas allocated in Octal PSRAM (`MALLOC_CAP_SPIRAM`, 153.6 KB), consuming 0 bytes of precious internal DRAM. Zero dynamic heap allocations inside `update()` or `draw()`.
- **Analytic Subpixel AA:** Evaluated with ~1.2-pixel `smoothstep()` AA filter band over all geometric contours.
- **Harmonic Spring-Damper Dynamics:** Facial features (squash-and-stretch, mouth opening, gaze tracking) utilize second-order mass-spring-damper differential equations with damping ratio $\zeta \approx 0.72$ and natural frequency $\omega_n \approx 20\text{ rad/s}$.
- **Wire Format:** Color output is packed as BGR565 byte-swapped via `packBGR565()`, matching `DisplayEmu::uiColor` and `CLASSIC_PALETTE`.
- **Non-blocking & 60 FPS Pacing:** Zero `delay()` calls in `BmoFace::update()` or `draw()`. All timing is driven by `millis()` and `micros()` deltas, streamed asynchronously via Core 0 DMA worker.

## Host-testability note
The 2D SDF math (ellipses, parabolic mouth, smoothstep AA, easing) is pure
arithmetic. The only hardware dependency is `DisplayEmu::pushPixelsAt()`.
Host test harnesses can compile and verify `renderFace()` output independently.

## Known failure signatures
| Symptom | Likely cause | Where to look |
|---|---|---|
| Face missing / flashes once and disappears | Menu canvas full-screen overwrite covering face when clean | `BmoGameboy.ino` menu loops |
| Face frozen, never animates | `BmoFace::update()` not called every tick | `loop()` top-level calls |
| Screen tears / corrupts near face | M-7 or M-19 violation | grep `BmoFace::` across ALL files |
| Colors wrong only on face | Missing BGR565 byte-swap | `bmo_face.cpp` `packBGR565` helper |
| Erratic / rapid blink | Blink timer not using `millis()` delta | Blink state variables in `bmo_face.cpp` |
