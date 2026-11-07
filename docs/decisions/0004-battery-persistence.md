# ADR 0004: Battery-Backed RAM Persistence

- Status: Accepted
- Date: 2026-11-07
- Source: `battery_save.h`, emulator wrappers, `save_manager.cpp`, `BmoGameboy.ino`

## Context

Some ROMs contain battery-backed RAM while many do not. Persisting a generic volatile region as if it were a battery save can overwrite valid saves or create false `.sav` files. The battery monitor hardware is not installed and GPIO1 is not connected to a divider.

## Decision

Only GB/GBC, NES, SNES, and the currently modeled PCE backup-RAM paths may write battery RAM, and only after ROM metadata identifies that storage. Keep battery voltage monitoring disabled and show USB/N/A or NOT FITTED where applicable. Ordinary emulator state uses the separate save-state format.

## Consequences

Header gates and explicit supported-core routing are source-verifiable. Actual save/load round trips after power loss require the physical SD card and title-specific tests. Adding support for another core requires a documented memory map and a non-destructive test ROM.
