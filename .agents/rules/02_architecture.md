# 4. Toolchain & Build Configuration
- **Board Package:** `esp32:esp32` version `3.3.11`.
- **Key Libraries:** `Adafruit ST7735 and ST7789 Library` v1.11.0, `Adafruit GFX Library`, `SD` v1.3.0.
- **Board Model:** ESP32-S3-N16R8 (16MB Quad SPI Flash, 8MB Octal SPI PSRAM).
- **Host Native C/C++ Test Compiler:** Microsoft Visual C++ (`cl.exe` via `vcvars64.bat`, Visual Studio 2022/18 Community) or system `gcc`/`clang`. Used by [`tools/host_cores_runner.c`](file:///e:/BMO%20Gameboy/tools/host_cores_runner.c) and [`tools/host_test.cpp`](file:///e:/BMO%20Gameboy/tools/host_test.cpp) via `tests/test_native_cores_execution.py`.
- **Flash & PSRAM Mode:** Quad SPI Flash (`FlashMode=qio`, 80MHz) & Octal SPI PSRAM (`PSRAM=opi`, 80MHz) per verified hardware testing in `01_hardware.md`.
- **Partition Scheme:** Custom `partitions.csv` prioritizing `app0` space (8MB) for firmware and baked ROMs.
- **Verified Build Command:**
  ```powershell
  .\arduino-cli.exe compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi" firmware/BmoGameboy
  ```

---

# 5. Architecture & Performance Patterns
- **Memory & Cache:** Place hot structures in internal DRAM aligned to the ESP32-S3 D-cache line size (`__attribute__((aligned(32)))`) where needed, and allocate all large emulator contexts, framebuffers, ROM payloads, save buffers, and box art in Octal PSRAM (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`).
- **Dual-Core FreeRTOS Pipeline:**
  - **Core 1 (Application & Emulation):** Runs Arduino `setup()` and `loop()`, emulator CPU/PPU step loops, button debouncing, and UI logic.
  - **Core 0 (Display Worker Task):** Hosts `vDisplayTask` dedicated FreeRTOS worker. Core 1 hands off completed double-buffered PSRAM frames via `DisplayEmu::submitFrameAsync()` allowing CPU emulation and SPI display DMA transfers to run in parallel at 60 FPS.
- **SPI Bus Arbitration (`SpiArbiter`):**
  - ST7789 display and MicroSD card share the FSPI bus (`TFT_SCK` GPIO 12, `TFT_MOSI` GPIO 11).
  - All display transactions and SD card filesystem operations (save states, battery RAM, ROM index cache, box art loading, screenshots, favorites, history) MUST acquire `SpiArbiter::lock()` before asserting CS to eliminate bus contention between Core 0 and Core 1.
- **IRAM Placement:** Critical inner-loop rendering functions (e.g., `lcd_draw_line`, `gb_rom_read`) MUST use `IRAM_ATTR` to prevent instruction cache misses.
- **Input Polling:** `Buttons::update()` manages a single global bitmask (`gb_joypad_state`) via atomic `REG_READ(GPIO_IN_REG)` and handles per-core A/B swap and auto-fire turbo. It MUST be called once per `loop()` iteration (or once per emulator frame). Do not call `digitalRead()` in hot paths.
- **Vendor Core Inclusion:** Header-only libraries (`peanut_gb.h` and `walnut_cgb.h`) follow the single-translation-unit pattern (`emu_peanut.cpp` and `emu_walnut.cpp`) using a namespace wrap (`PGB::`, `WGB::`) to avoid ODR violations.
