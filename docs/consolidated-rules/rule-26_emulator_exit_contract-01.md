# Section 26 — Emulator Exit Contract

Source: [`26_emulator_exit_contract.md`](../../.agents/rules/26_emulator_exit_contract.md).

# Emulator Exit Contract

The launcher owns the ROM buffer and session transition. The wrapper owns only its emulator allocations. Check the live dispatch in firmware/BmoGameboy/BmoGameboy.ino before changing the sequence.

## Current user interaction

- SELECT + START opens the pause menu.
- SELECT + UP opens the pause menu with Quit selected; it does not immediately exit.
- Quit requires two A presses within three seconds. B or Start resumes. Direction or Select input cancels the armed confirmation; timeout disarms it.
- The ordinary Quit menu row uses the same confirmation.

On confirmed exit, firmware attempts to save supported battery RAM, waits for display work to finish, calls the active wrapper destroy() path, releases the caller-owned SD ROM buffer, and returns to the console menu. Unsupported battery save formats return false and are not written as if supported.

## Wrapper ownership contract

Each wrapper destroy() should be safe when called more than once, release every allocation owned by that wrapper, clear freed pointers and running state, and not free the caller-owned ROM. SDCard::freeRom() releases SD-loaded ROM buffers and leaves the known baked flash ROM pointers alone.

All fourteen wrapper teardown calls are present in the dispatcher and checked by the repository validator. This source check does not prove target allocation recovery. Repeated launch/exit and allocation-failure tests remain open; no hardware status may be inferred from the presence of the calls.