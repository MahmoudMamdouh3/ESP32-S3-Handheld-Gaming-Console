# Section 12 — Extensibility Contract (Emulator Cores & Peripherals)

Source: [`12_extensibility_contract.md`](../../.agents/rules/12_extensibility_contract.md).

# Extensibility Contract (Emulator Cores & Peripherals)
Purpose: Every console emulator core in the 14-core architecture follows a uniform C++ wrapper lifecycle so cores can be initialized, stepped, saved/restored, and torn down deterministically without PSRAM leaks.

## Adding or modifying an emulator core
- Wrapper files live under `src/emulators/emu_<name>.h/.cpp` and underlying C/C++ engines live under `src/vendor/<name>/` (or `src/engine/<name>/`), per `03_conventions.md`.
- Every wrapper must implement the unified lifecycle contract:
  - `begin(...)` — Allocates all required state, VRAM, and work RAM buffers in Octal PSRAM (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`).
  - `updateJoypad()` — Reads `Buttons::get(...)` or `Buttons::gb_joypad_state` (never calls `digitalRead()` directly).
  - `runFrame()` — Executes one frame and streams to `DisplayEmu::stream*Frame(...)`.
  - `destroy()` — Frees every PSRAM buffer allocated by the core and resets pointers to `nullptr`.
  - `saveBatteryRam(const char* romFilename)` / `loadBatteryRam(const char* romFilename)` — Persists cartridge battery backup RAM (`.sav`).
  - `saveState(const char* romFilename, int slot)` / `loadState(const char* romFilename, int slot)` — Persists 5-slot snapshots (`.s01`–`.s05`) via `SaveManager` using PSRAM staging buffers (never allocating large context structs on the 8 KB `loopTask` stack).

## Deterministic emulator teardown (`destroyActiveEmulator()`)
When exiting a game or switching ROMs in `BmoGameboy.ino`, `destroyActiveEmulator()` invokes `::destroy()` on the active emulator core and calls `SDCard::freeRom(currentRom)` to release the PSRAM ROM buffer. `scripts/validate_repo.py` Phase 2 enforces that all 14 `::destroy()` calls are wired in `BmoGameboy.ino`.

## No per-frame heap allocation
No emulator core's per-frame render or CPU-execution loop may call `malloc`, `heap_caps_malloc`, `new`, or any other heap allocation. Everything needed inside the hot loop must be allocated once at core initialization (or lazily allocated once in PSRAM and freed in `destroy()`) and reused across frames.

## General Peripheral Extensions
The init-update-teardown and `FEATURE_*`-flag contract above applies to any hardware peripheral module (`battery.h`, `audio_i2s.h`, `fram_save.h`). Keep `FEATURE_BATTERY_MONITOR = 0` and `FEATURE_AUDIO = 0` until physical hardware is soldered and verified per `00_hard_stops.md`.