# Firmware Agent Guide (`firmware/BmoGameboy/`)

Applies to all `.ino`, `.cpp`, `.c`, `.h`, and `.hpp` changes under `firmware/BmoGameboy/` (`src/hal/`, `src/storage/`, `src/ui/`, `src/cores/`, `src/third_party/`).

## Hard Stops

- Do NOT modify `FEATURE_BATTERY_MONITOR` (`0`), `FEATURE_AUDIO` (`0`), `partitions.csv`, or `sketch.yaml` QIO/OPI settings.
- Do NOT assign GPIO `0, 3, 33–37, 45, 46` or modify the soldered GPIO mapping in `src/hal/config.h`.
- Never commit commercial ROM arrays (`BMO_ENABLE_BAKED_ROMS=0` by default). By default `BMO_EXPERIMENTAL_CORES=1` and `BMO_ENABLE_DOOM=1` include all 13 third-party engines under `src/third_party/` (combined firmware binary licensed `GPL-2.0-or-later`; pass `-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0` to compile only the permissive MIT GB/GBC/NES subset).

## Shared SPI Bus & Dual-Core Rules

- Core 0 runs the `BMO_Display` worker task (`src/ui/display_emu.cpp`) driving ST7789 over FSPI (`TFT_SCK=12`, `TFT_MOSI=11`, `TFT_CS=10`).
- Core 1 runs Arduino `setup()`/`loop()` (`BmoGameboy.ino`), button polling, emulator stepping, and MicroSD I/O (`SD_CS=13`, `SD_MISO=15`).
- **Every** display transfer and SD operation MUST hold `SpiArbiter::ScopedLock` (`src/hal/spi_arbiter.h`) or matching `SpiArbiter::lock()` / `SpiArbiter::unlock()` (`src/hal/spi_arbiter.cpp`) across every early return path. Never release or leak the mutex across returns.
- Before tearing down an active emulator or freeing framebuffers in PSRAM, wait for `DisplayEmu` to finish any queued asynchronous frame.

## Memory, Stack, and Hot-Path Discipline

- ESP32-S3 has 327,680 B usable DRAM and 8 MiB Octal PSRAM (`MALLOC_CAP_SPIRAM`). Large framebuffers, ROM buffers, emulator RAM/states, and staging buffers belong in Octal PSRAM, not static `.bss` or stack.
- Avoid large stack allocations (`> 1024` B arrays) in first-party functions; `python -m tools.guardian audit` flags stack-heavy local arrays.
- Never call `Serial.print*` inside per-frame emulator loops; use the gated `LOG_*` macros in `src/hal/config.h`.
- Pins `BTN_Y=42`, `BTN_L=47`, `BTN_R=48` are above GPIO 31 and are read from `GPIO_IN1_REG` inside `src/hal/buttons.cpp`; never scatter raw `digitalRead()` calls across wrappers.
- Keep `src/cores/emu_walnut.cpp` free of unaligned `uint16_t*` / `uint32_t*` dereferences on Xtensa.
- UI strings drawn with Adafruit GFX CP437 / `FreeSans*` fonts must remain 7-bit ASCII (no multibyte UTF-8 characters).

## Verification Scope

- Default compile builds all 15 platforms (`BMO_EXPERIMENTAL_CORES=1`, `BMO_ENABLE_DOOM=1`): `.\arduino-cli.exe compile --profile esp32s3 firmware/BmoGameboy` (`TARGET_COMPILED`). Also verify opt-out builds `-DBMO_ENABLE_DOOM=0` and `-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0` when modifying gated core plumbing or `CoreRegistry`.
- Run `python -m tools.guardian audit` / `python -m tools.guardian verify` and host C++ tests (`python -m unittest discover -s tests -v`).
- Never report `HARDWARE_TESTED` unless observed directly on physical hardware and logged in [`docs/hardware_measurements.md`](../docs/hardware_measurements.md).
