# Firmware Agent Guide (`firmware/BmoGameboy/` & `firmware/bringup/`)

Applies to all `.ino`, `.cpp`, `.c`, `.h`, and `.hpp` changes under `firmware/BmoGameboy/` (`src/hal/`, `src/storage/`, `src/ui/`, `src/cores/`, `src/app/`, `src/third_party/`) and `firmware/bringup/`.

## Hard Stops

- Do NOT modify `FEATURE_BATTERY_MONITOR` (`0`), `FEATURE_AUDIO` (`0`), `partitions.csv`, or `sketch.yaml` QIO/OPI settings without following the Hard-Stop Change Protocol in [`AGENTS.md`](../AGENTS.md).
- Do NOT assign GPIO `0, 3, 26–37, 45, 46` or modify the soldered GPIO mapping in `src/hal/config.h` (verified at compile time by `src/hal/pins_check.h` and on host by `tests/test_pin_snapshot.py`).
- Never commit commercial ROM arrays (`BMO_ENABLE_BAKED_ROMS=0` by default). By default `BMO_EXPERIMENTAL_CORES=1` and `BMO_ENABLE_DOOM=1` compile all 13 third-party engines under `src/third_party/` for personal local use (`default` profile); pass `-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0` (`mit` profile) to compile only the redistributable permissive MIT GB/GBC/NES subset ([`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md)).

## Shared SPI Bus & Dual-Core Rules

- **CPU Core 0** runs the `BMO_Display` worker task (`src/ui/display_worker.cpp`) driving ST7789 over FSPI (`TFT_SCK=12`, `TFT_MOSI=11`, `TFT_CS=10`).
- **CPU Core 1** runs Arduino `setup()`/`loop()` (`BmoGameboy.ino`, `src/app/bmo_app.cpp`), button polling, emulator stepping, and MicroSD I/O (`SD_CS=13`, `SD_MISO=15`).
- **Every** display transfer and SD operation MUST hold `SpiArbiter::ScopedLock` (`src/hal/spi_arbiter.h`) across every return path. Never release or leak the mutex across early returns.
- Before tearing down an active emulator or freeing framebuffers in PSRAM, wait for `DisplayEmu::waitForDisplay()` to finish any queued asynchronous frame.

## Memory, Stack, and Hot-Path Discipline

- ESP32-S3 has `327,680 B` usable internal DRAM and `8 MiB` Octal PSRAM (`MALLOC_CAP_SPIRAM`). Large framebuffers, ROM buffers, emulator RAM/states, telemetry windows, and staging buffers belong in Octal PSRAM (`src/hal/psram_alloc.h`), not static `.bss` or stack.
- Avoid large stack allocations (`> 1024 B` arrays) in first-party functions; `python -m tools.guardian audit` flags stack-heavy local arrays.
- Never call `Serial.print*` inside per-frame emulator loops; use the gated `LOG_*` macros in `src/hal/config.h`.
- Pins `BTN_Y=42`, `BTN_L=47`, `BTN_R=48` are above GPIO 31 and are read from `GPIO_IN1_REG` inside `src/hal/buttons.cpp`; never scatter raw `digitalRead()` calls across wrappers.
- Keep `src/cores/emu_walnut.cpp` free of unaligned `uint16_t*` / `uint32_t*` dereferences on Xtensa.
- UI strings drawn with Adafruit GFX CP437 / `FreeSans*` fonts must remain 7-bit ASCII (no multibyte UTF-8 characters).

## Verification Scope

- Verify all three profiles (`default`, `no-doom`, `mit`) with `python tasks.py build --profile all --warnings strict --fail-on-warnings` (`TARGET_COMPILED`) and check memory budgets with `python tasks.py size --check-budgets`.
- Verify the standalone hardware bring-up sketch with `.\arduino-cli.exe compile --profile esp32s3 firmware/bringup`.
- Run `python tasks.py check` and `python tasks.py format --check`.
- Never report `HARDWARE_TESTED` unless observed directly on physical hardware and logged in [`docs/hardware_measurements.md`](../docs/hardware_measurements.md).
