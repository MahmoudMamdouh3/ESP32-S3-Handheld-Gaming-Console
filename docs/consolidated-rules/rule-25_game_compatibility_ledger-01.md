# Section 25 — Game Compatibility Ledger

Source: [`25_game_compatibility_ledger.md`](../../.agents/rules/25_game_compatibility_ledger.md).

# Game Compatibility Ledger
Purpose: agents should never have to re-discover whether a game works --
the answer must be in this file. Specifically, this ledger records what
hardware flags, emulator revisions, and engine settings were active when
a game was last tested, so future agents know if a change is a regression.

## Maintenance rule
- Add a row for EVERY game that is tested on real hardware, even partial runs.
- Update the row (do not add a duplicate) when a game is re-tested after a change.
- Never remove rows -- only update Status and Notes.
- If you break a previously-working game: change its Status to BROKEN,
  add a note describing what changed, and log an INC in 23_incident_postmortem_log.md.

## Status vocabulary (same as 06_verification_standards.md, adapted)
- `WORKS` -- ran to a stable known-good gameplay state, defined below.
- `PARTIAL` -- boots/title screen shown but freezes, glitches, or crashes before
  reaching a stable gameplay state.
- `BROKEN` -- does not boot or immediately crashes/reboots device.
- `UNTESTED` -- exists in repo or on SD card but never run on real hardware.

## Known-good states (PASS bar per game)
These definitions are the PASS bar for VERIFIED_HARDWARE status.
A game may not be listed as WORKS unless it reached its defined state.

| Game | Console | PASS definition |
|---|---|---|
| Super Mario Bros. Deluxe | GBC | Title screen loads; "New Game" selected; gameplay starts; Mario walks at least 10 steps without freeze |
| Legend of Zelda: Oracle of Ages | GBC | Title screen loads; "New Game" selected; intro cutscene plays |
| Aladdin | GBC | Title screen loads; gameplay starts; at least 5 seconds of movement |
| Lego Racers | GBC | Title screen loads; menu navigation responds |
| Tobu Tobu Girl | GB (baked) | Title screen loads; gameplay starts |

For any game not listed above, define the PASS bar before calling it WORKS.
Document the definition in this file before the hardware test, not after.

## Ledger

| Game | Console | Emulator | Firmware commit | Walnut flags | Status | Last tested | Notes |
|---|---|---|---|---|---|---|---|
| Super Mario Bros. Deluxe (Baked) | GBC | WalnutEmu | 3c53275 (logging refactor) | 16BIT_OPS=1, DUALFETCH=1 | BROKEN | 2026-08-30 | Froze on new-game/load. INC-3. Flags reverted to 0 in 5d78e8b. |
| Super Mario Bros. Deluxe (Baked) | GBC | WalnutEmu | 5d78e8b (flag revert) | 16BIT_OPS=0, DUALFETCH=0 | UNTESTED | -- | Flags reverted; needs hardware re-flash to confirm fix. |
| Legend of Zelda: Oracle of Ages (Baked) | GBC | WalnutEmu | -- | -- | UNTESTED | -- | Baked ROM present, never hardware-tested |
| Aladdin (Baked) | GBC | WalnutEmu | -- | -- | UNTESTED | -- | Baked ROM registered in sd_card.cpp; compiles cleanly |
| Lego Racers (Baked) | GBC | WalnutEmu | -- | -- | UNTESTED | -- | Baked ROM registered in sd_card.cpp; compiles cleanly |

## How to update this file
When you flash and test a game:
1. Record the exact `git describe --tags --always` output as "Firmware commit".
2. Record which walnut flags were non-default (0->1 or 1->0).
3. Record the date as YYYY-MM-DD.
4. Set Status to WORKS / PARTIAL / BROKEN.
5. Add a Notes column entry if anything was abnormal.