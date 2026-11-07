# SDD Section 05.6: Gameplay, Pause, Timing, and Teardown

[Back to the SDD index](../software-design-document.md) · [Previous: state input](section-05-part-02.md)

## Gameplay chords

`STATE_EMULATOR` polls buttons, handles modifier chords, then calls the selected core's input/update/frame functions. The current `SELECT` chords are:

| Chord | Behavior |
| :--- | :--- |
| Select + Start | Open the pause menu |
| Select + A | Save to the active save-state slot |
| Select + B | Load the active save-state slot |
| Select + Down | Cycle the DMG palette for GB/GBC |
| Select + Left | Toggle button turbo |
| Select + Right | Fast-forward; bypass the loop pacing wait while held |
| Select + Up | Open the pause menu with Quit selected; user confirmation required |

## Pause menu

The pause menu has six rows, navigated with up/down and activated with A:

| Option | Action |
| :---: | :--- |
| 0 | Resume |
| 1 | Save current state to active slot |
| 2 | Load active slot |
| 3 | Cycle active save slot (1–5) |
| 4 | Save supported battery RAM |
| 5 | Quit to console carousel, with confirmation |

Left toggles A/B mapping; right toggles the console bezel; Select writes a screenshot; B or Start resumes. Save-state/battery existence is cached while the menu is idle and refreshed when menu metadata becomes dirty. The option and data behavior are implemented in `BmoGameboy.ino`, `Buttons`, `SaveManager`, and `DisplayEmu`.

### Quit confirmation contract

`SELECT+UP` opens the pause menu with option 5 selected and prompts the user to confirm or resume. The first A press arms a three-second confirmation and changes the prompt to require another A. The second A during that interval attempts to save supported battery RAM. A failed save leaves the emulator session open; A retries, Start explicitly quits anyway, and B resumes. When no battery save is supported, the confirmed quit proceeds without a battery write. Up/down/left/right cancel the armed quit. Select cancels the armed quit and writes a screenshot. Timeout disarms the confirmation. The pause-menu Quit row uses the same flow. `test_pause_quit_policy.py` runs the pure policy with simulated save failures and control inputs; physical button behavior is not verified.

## Per-core loop pacing

`frame_pacing.h` provides nominal budgets keyed by the wrapper index selected during ROM launch. The canonical table and source basis are in [Section 15.1](section-15.md#151-source-configured-frame-targets):

The loop records core execution time before waiting. If execution is below the selected target, it yields with `delay()` for most of the remaining interval and uses `ets_delay_us()` for the tail. If execution already exceeded the target, it increments the dropped/over-budget count. Select+Right bypasses the wait. These target budgets are software configuration; they are not measurements of achieved frame rate or panel refresh.

## Frame telemetry

- Each second, firmware prints actual loop FPS and execution-time average/minimum/maximum, plus over-budget frame counts when present. The log includes emulator index and configured target budget.
- Every 600 emulator frames, firmware sorts a fixed 600-sample static buffer and prints nearest-rank p50/p95/p99 execution-time percentiles. The samples measure unpaced core-loop execution, not time from panel scan-out or user input to visible pixel.
- `tools/telemetry_plotter.py` parses the per-second lines and the 600-frame percentile records. Its percentiles over per-second averages are not a replacement for the firmware's per-frame execution-window percentiles.
- No current physical-device capture was recorded in this audit. Use the procedure in [`hardware_measurements.md`](../hardware_measurements.md).

## Teardown

`destroyActiveEmulator()` waits for outstanding display work, then dispatches `destroy()` only to the wrapper selected by `selectedEmulatorIndex` before releasing the ROM buffer. Save and exit paths must complete SD writes before the active source buffer is released. See [Section 09](section-09-part-01.md) for wrapper lifecycle contracts.
