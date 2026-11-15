# ADR 0015: Display Driver (`Adafruit_ST7789` vs `esp_lcd` / `LovyanGFX`) and UI Framework Closure (`LVGL` / `Tactility`)

- **Status:** Accepted
- **Evidence Level:** `HOST_TESTED` / `TARGET_COMPILED` (`0 HARDWARE_TESTED` records yet)
- **Source:** `firmware/BmoGameboy/src/ui/display_worker.cpp`, `firmware/BmoGameboy/src/ui/display_emu.cpp`, `firmware/BmoGameboy/src/ui/bmo_face.cpp`

## Context

Two architectural questions recurred across earlier design discussions:
1. **Display Driver:** Should the firmware replace `Adafruit_ST7789` + `GFXcanvas16` + the CPU Core 0 `display_worker` FreeRTOS task with ESP-IDF's `esp_lcd_panel_io_spi` GDMA driver or `LovyanGFX`?
2. **UI Framework:** Should the custom `GFXcanvas16` launcher, carousel, museum, and procedural BMO mascot face (`bmo_face.cpp`) be replaced with a widget toolkit such as **LVGL** or **Tactility**?

## Analysis & Quantitative Comparison

### 1. Display Driver Options

| Option | Architecture | CPU Overhead During SPI Push | Bus Contention (`SpiArbiter`) | Migration Risk |
| :--- | :--- | :--- | :--- | :--- |
| **Option A (Current):** `Adafruit_ST7789` + CPU Core 0 `display_worker` | CPU Core 1 emulates/draws into PSRAM; CPU Core 0 converts palettes and calls `SPI.writeBytes()` under `SpiArbiter::ScopedLock` | **0% on CPU Core 1** for async cores (`GB`, `GBC`, `NES`, `DOOM`, menus); blocking on Core 0 during wire transfer | Deterministically serialized with `SD` (`Arduino SD.h`) via `SpiArbiter` mutex | **None** (implemented and `TARGET_COMPILED`) |
| **Option B:** `esp_lcd_panel_io_spi` (GDMA) | Hardware GDMA descriptors stream bounce buffers from internal SRAM to FSPI asynchronously, firing an ISR callback on completion | Frees CPU Core 0 during SPI wire time (`~15–25 ms/frame`), requires `16–32 KiB` internal DMA-capable SRAM bounce buffers | Requires holding `SpiArbiter` across asynchronous ISR completion or migrating `SD.h` to `esp_vfs_fat_sdspi_mount` | **Medium-High** (must not release `SpiArbiter` before GDMA finishes) |
| **Option C:** `LovyanGFX` | Custom ESP32-S3 SPI/DMA driver with built-in sprite canvas and bus sharing | Low CPU overhead, fast DMA sprites | Replaces `Adafruit_GFX` API across 1,900+ lines of UI and emulator stream functions | **High** (large third-party dependency + full UI rewrite before hardware bring-up) |

### 2. UI Framework Options (`GFXcanvas16` vs `LVGL` / `Tactility`)

| Criterion | Current Custom UI (`GFXcanvas16` + `HarmonicSpring`) | `LVGL 8/9` | `Tactility` (LVGL + ESP-IDF Shell) |
| :--- | :--- | :--- | :--- |
| **Flash Footprint** | `~45 KiB` compiled code (`display_emu.cpp` + `bmo_face.cpp`) | `+350 KiB to +650 KiB` Flash | `+800 KiB+` Flash (requires pure ESP-IDF build) |
| **Internal SRAM / DRAM Impact** | **0 B** framebuffer in internal DRAM (`320×240×2 = 153.6 KiB` canvas lives in Octal PSRAM) | Requires internal SRAM draw buffers (`30–64 KiB`) + object heap overhead; reduces headroom needed by DOOM/SNES | Incompatible with `192 KiB` static DRAM footprint of 14 statically linked cores |
| **BMO Mascot SDF & Harmonic Springs** | Native per-pixel SDF eye/mouth renderer + 60 Hz spring-damper physics (`DESIGN.md` token-checked) | Still requires custom canvas drawing; LVGL widgets add no value to full-screen emulator viewports | Same; adds OS chrome unsuitable for a dedicated 8-button character handheld |

## Decision

1. **Close the `LVGL` / `Tactility` question permanently (`Rejected`):** The handheld has only four UI screens (Boot Splash/Mascot, Console Carousel, Game List, Pause/Diagnostics) plus full-screen emulator viewports. Adopting `LVGL` or `Tactility` would consume internal SRAM needed by emulator cores and break the verified `DESIGN.md` color/spring specification without helping emulator frame rates.
2. **Retain `Adafruit_ST7789` + CPU Core 0 `display_worker` for Initial Hardware Bring-Up (`Accepted`):** For `GB`, `GBC`, `NES`, `DOOM`, and all menus, CPU Core 0 already offloads SPI transfers from CPU Core 1 while preserving strict `SpiArbiter` mutex serialization with `Arduino SD.h`.
3. **Post-Bring-Up Optimization Path:** Once physical bench measurements (`docs/hardware_measurements.md`) are recorded, if `40 MHz` SPI is required on the perfboard, first extend CPU Core 0 async streaming (`CMD_STREAM_RAW16`) to the remaining synchronous experimental wrappers (`SMS`, `PCE`, `Genesis`, `SNES`) before considering an `esp_lcd` GDMA rewrite.

## Consequences

- Zero new UI framework dependencies or internal DRAM regressions before physical board bring-up.
- All UI rendering remains covered by `python scripts/verify_color_tokens.py` and host unit tests (`tests/test_bmo_face_spring.py`, `tests/test_theme_color.py`).
