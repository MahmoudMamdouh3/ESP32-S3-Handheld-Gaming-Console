# Emulator Exit Contract
Purpose: Formalizes what every emulator core's teardown path must do on exit so PSRAM allocations are completely recovered and no session leaks heap across repeated game launches.

## Current State (Verified Across All 14 Cores)
In [`BmoGameboy.ino`](file:///e:/BMO%20Gameboy/firmware/BmoGameboy/BmoGameboy.ino), exiting a game from `STATE_PAUSE_MENU` (`START + SELECT` -> `Exit to Menu`) calls:
1. `autoSaveCurrentBatteryRam()` — Persists cartridge battery SRAM (`.sav`) to `/saves/` via `SaveManager::saveBatteryRam`.
2. `teardownCurrentEmulator()` — Invokes `destroy()` on whichever of the **14** emulator cores is active:
   - `WalnutEmu::destroy()`, `PeanutEmu::destroy()`, `NesEmu::destroy()`, `DoomEmu::destroy()`, `SmsEmu::destroy()`, `PceEmu::destroy()`, `AtariEmu::destroy()`, `PicoEmu::destroy()`, `GenesisEmu::destroy()`, `SNESEmu::destroy()`, `WSwanEmu::destroy()`, `NGPEmu::destroy()`, `LynxEmu::destroy()`, `ColemEmu::destroy()`.
3. `SDCard::freeRom(currentRom, currentRomType)` — Frees the PSRAM ROM buffer (while leaving `.rodata` flash-baked pointers untouched).

## What every emulator `destroy()` must do
When called, a core's `destroy()` function must:
1. Free every PSRAM buffer it allocated at `begin()` time (`heap_caps_free`).
2. Reset all freed pointers back to `nullptr` and `initialized = false` so `begin()` can re-initialize cleanly on the next game launch.
3. NOT free the caller-owned ROM buffer (`SDCard::freeRom()` handles that after `teardownCurrentEmulator()`).
4. Be idempotent (safe if called multiple times with null pointers).

## Why this matters
The ESP32-S3-N16R8 has 8MB of Octal PSRAM. Each emulator core allocates between 64KB and 512KB of PSRAM for CPU state, VRAM, work RAM, and cartridge SRAM, plus up to 4MB for the ROM image. Without deterministic teardown across all 14 cores, repeated game switches would exhaust or fragment PSRAM. `scripts/validate_repo.py` Phase 2 enforces that all 14 `::destroy()` calls remain wired in `BmoGameboy.ino`.
