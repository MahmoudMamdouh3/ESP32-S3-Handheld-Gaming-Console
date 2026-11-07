# Section 24 — Vendor Flag Safety

Source: [`24_vendor_flag_safety.md`](../../.agents/rules/24_vendor_flag_safety.md).

# Vendor Flag Safety
Purpose: INC-3 (2026-08-30) proved that enabling a vendor `#define` flag
without reading its own warning comment breaks real games silently.
This file makes the read-before-enable step mandatory and non-skippable.

## The incident this rule prevents
`WALNUT_GB_16_BIT_OPS_DUALFETCH` and `WALNUT_GB_16_BIT_OPS` were flipped
from `0` to `1` by a previous agent to activate a performance fast path.
The comment directly above those defines, written by the upstream author,
reads: "currently breaks compatibility with some games, 16-bit opcode
optimization needs revisions." The agent enabled them anyway and committed.
Result: Super Mario Bros. Deluxe (GBC) froze on new-game/load, which
exercises MBC5 bank switching paths the fast paths mishandle.

## Mandatory read-before-enable protocol
Before changing any `#define` flag from `0` to `1` (or adding one) inside
**any file under `src/vendor/` or `src/engine/`**:

1. **Read the comment block immediately above the `#define`.** "Immediately
   above" means contiguous -- no blank line gap between comment and define.
2. **Read the comment block immediately inside any `#if <FLAG>` block** that
   flag controls (e.g., the body of `#if WALNUT_GB_16_BIT_OPS`).
3. **Search the file** for any // TODO, // FIXME, // NOTE, // WARNING,
   // BROKEN, // EXPERIMENTAL, // may, // breaks, // limited near
   the flag name. Grep for the flag name if the file is large.
4. **Quote the warning verbatim** in your commit message body if any such
   language exists. Do not paraphrase. The exact words are what a future
   agent will grep for.
5. If the comment contains any compatibility or correctness warning, the flag
   is **BLOCKED from being enabled** until either:
   - A hardware-verified game compatibility test (VERIFIED_HARDWARE) covers
     the specific code path the flag activates, OR
   - The user explicitly approves the risk in writing (quote their exact
     message in the commit body).

## What "compatibility warning" means
Any of the following phrases in the comment above the flag counts as a
blocking warning: "breaks", "compatibility", "some games", "may not work",
"experimental", "needs revisions", "limited to", "TODO", "FIXME",
"not yet", "only on", "unsafe", "can break".
This list is not exhaustive -- use judgment. If the upstream author was
hedging, treat it as a warning.

## CPU-test pass != game compatibility
A passing `cpu_instrs.gb` run (or any Blargg CPU test suite) only verifies
that individual opcodes execute with correct register state and timing.
It does **not** verify:
- MBC bank switching sequences (MBC1/3/5 state machine transitions)
- DMA transfer correctness during active HDMA
- Timing-dependent interrupt edge cases
- GBC-specific WRAM banking
- Any game-specific initialization sequence

Enabling a vendor flag and then running `cpu_instrs.gb` is insufficient
evidence to promote a change above FIXED_UNVERIFIED. State which
**named commercial or homebrew game** you ran, on real hardware, to
game-specific known-good states (see `25_game_compatibility_ledger.md`).

## Turning a flag off (revert path)
If a game breaks and the suspected cause is a recently-enabled vendor flag:
1. Revert that flag first (one-line change), commit, re-flash, confirm.
2. Do not combine the revert with other changes -- it must be isolatable.
3. Add the game and the broken-flag combination to `25_game_compatibility_ledger.md`.
4. Log the incident in `23_incident_postmortem_log.md`.

## src/engine/walnut_cgb/ specific flags
Current known-dangerous flags and their safe defaults:

| Flag | Safe default | Warning quoted from source |
|---|---|---|
| `WALNUT_GB_16_BIT_OPS_DUALFETCH` | `0` | "currently breaks compatibility with some games, 16-bit opcode optimization needs revisions" |
| `WALNUT_GB_16_BIT_OPS` | `0` | "this can break compatibility with some games and is disabled by default" |
| `WALNUT_GB_SAFE_DUALFETCH_OPCODES` | `0` | "used for debugging invalidated opcodes or compatibility but slows execution" |
| `WALNUT_GB_SAFE_DUALFETCH_DMA` | `0` | same as above |
| `WALNUT_GB_SAFE_DUALFETCH_MBC` | `0` | same as above |

**Do not change these without an entry in `25_game_compatibility_ledger.md`
first showing the affected game under both flag states.**