# Section 14 — Error Handling & Fault Isolation

Source: [`14_error_handling_and_fault_isolation.md`](../../.agents/rules/14_error_handling_and_fault_isolation.md).

# Error Handling & Fault Isolation
Purpose: ties `00_hard_stops.md`'s no-fatal-deadlock rule and the `gb_error` panic path into one place so failure handling isn't reinvented ad hoc per module.

## Core Philosophy
- Every module's failure path degrades to a **visible, non-fatal state** (e.g., adding an error/panic display state to the `BmoFace` state machine, a Serial log line, or a returned error enum) — never a silent `return;` leaving state inconsistent, never a blocking loop waiting on hardware that might not be there (already a `00_hard_stops.md` category).
- Never swallow an init function's return value (`gb_init`, `SD.begin()`, display init). Check it; on failure, enter that subsystem's explicit error state rather than proceeding as if it succeeded.

## Emulator Panics
Emulator panics (`gb_error` and equivalents in other cores) must:
1. Stop that core's execution.
2. Free that core's PSRAM allocations via its `teardown()` (`12_extensibility_contract.md`).
3. Return to the menu/BmoFace with a visible reason string — never a silent reboot, never a hang.

## Untrusted Input
- Anything read from the SD card (ROM headers, save files, generated cover art) is **untrusted input**.
- Validate size and the Nintendo-logo checksum (`software-design-document.md` §9) before treating a file as valid.
- A failed validation goes to the visible error state — never a "partial load."

## Subsystem Error Codes
- Define one small error-code enum per subsystem (SD, save/FRAM, each emulator core) instead of magic ints or a `bool` that collapses multiple failure reasons into one bit.
- Register these in `10_symbol_reference.md` like any other symbol.

## Assertions
- Use asserts liberally in the **host_test** desktop build (`09_testing_infrastructure.md`), where a failed assert just prints and exits — that's where violated invariants should be caught.
- On-device, a failed invariant degrades to the visible error state above, never `abort()`/halt — a handheld that looks bricked with no visible cause is worse for field debugging than a "core X crashed: reason Y" screen.