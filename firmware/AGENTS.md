# Firmware Agent Guide (`firmware/BmoGameboy/` & `firmware/bringup/`)

Applies to all `.ino`, `.cpp`, `.c`, `.h`, and `.hpp` changes under `firmware/BmoGameboy/` (`src/app/`, `src/hal/`, `src/storage/`, `src/ui/`, `src/cores/`, `src/third_party/`) and `firmware/bringup/`. Follow [`AGENTS.md`](../AGENTS.md) first.

## Hard Stops

- Do NOT modify `FEATURE_BATTERY_MONITOR` (`0`), `FEATURE_AUDIO` (`0`), `partitions.csv`, or `sketch.yaml` QIO/OPI settings without the Hard-Stop Change Protocol in [`AGENTS.md`](../AGENTS.md).
- Do NOT assign GPIO `0, 3, 26–37, 45, 46` or modify the soldered GPIO mapping (`hal/config.h` / `hal/pin_map.h`, verified by `hal/pins_check.h` and `tests/test_pin_snapshot.py`).
- Never commit commercial ROM arrays (`BMO_ENABLE_BAKED_ROMS=0`). Keep `default`, `no-doom` (`-DBMO_ENABLE_DOOM=0`), and `mit` (`-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0`) compiling cleanly ([`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md)).

## Shared SPI Bus & Dual-Core Rules

- **CPU Core 0** runs the `BMO_Display` worker (`src/ui/`) driving ST7789 over FSPI (`TFT_SCK=12`, `TFT_MOSI=11`, `TFT_CS=10`).
- **CPU Core 1** runs `setup()`/`loop()` (`BmoGameboy.ino`, `src/app/`), button polling, emulator stepping, and MicroSD I/O (`SD_CS=13`, `SD_MISO=15`).
- **Every individual function** executing display SPI transfers or SD card operations MUST acquire `SpiArbiter::ScopedLock` across every return path. Never rely on implicit caller locks.
- Synchronize Core 0/Core 1 state using `std::atomic` / FreeRTOS task notifications (`waitForDisplay()`) before tearing down an active emulator or freeing PSRAM buffers.

## Memory, Stack & Hot-Path Discipline

- **Internal DRAM (`327,680 B`) is scarce:** Keep emulator frames, ROM data, catalog indices, telemetry rings, and staging buffers in Octal PSRAM (`src/hal/psram_alloc.h`) — never fallback silently from `bmo_psram_malloc` to internal DRAM, and never use `new`/`malloc`/`std::string`/`std::vector` inside emulator wrappers or UI rendering.
- **DMA Bounce Strips:** Stream SPI transfers from PSRAM through small internal-DRAM DMA bounce buffers (`MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL`), batching scanlines rather than issuing per-line SPI writes.
- **Stack & Function Budgets:** Keep local stack arrays `<= 1,024 B`, target `<= 400` lines/file (`1,000` max) and `<= 60` lines/function (`100` max), use `[[nodiscard]]` on fallible I/O & allocators, and enforce Rule-of-Five on RAII/PSRAM wrappers.
- **No Blocking Loops or Raw Logging:** Zero `delay()` / `ets_delay_us()` inside frame loops or emulators; zero `Serial.print*` in hot paths (use gated `LOG_*` macros).
- **Hardware Quirks:** Read GPIO `> 31` (`BTN_Y=42`, `BTN_L=47`, `BTN_R=48`) via `GPIO_IN1_REG` in `src/hal/buttons.cpp`; keep `emu_walnut.cpp` free of unaligned `uint16_t*`/`uint32_t*` dereferences; keep UI strings 7-bit ASCII.

## Verification Scope

- Verify all profiles (`python tasks.py build --profile all --warnings strict --fail-on-warnings`), budgets (`python tasks.py size --check-budgets`), bring-up sketch (`.\arduino-cli.exe compile --profile esp32s3 firmware/bringup`), `python tasks.py format --check`, and `python tasks.py check`.
- Never report `HARDWARE_TESTED` unless physically measured and logged in [`docs/hardware_measurements.md`](../docs/hardware_measurements.md).
