# 4. Toolchain & Build Configuration
- **Board Package:** esp32:esp32 version `3.3.11`.
- **Key Libraries:** `Adafruit ST7735 and ST7789 Library` v1.11.0, `SD` v1.3.0, `Agnes` v0.2.0.
- **Board Model:** ESP32-S3-N16R8 (16MB Flash, 8MB PSRAM).
- **Host Test Compiler:** Zig v0.13.0 for Windows x86_64 (`https://ziglang.org/download/0.13.0/zig-windows-x86_64-0.13.0.zip`). Downloaded and extracted via `curl` and `Expand-Archive`.
- **Flash & PSRAM Mode:** Quad SPI Flash (QIO 80MHz) & Octal SPI PSRAM (OPI 80MHz) per verified hardware testing in `01_hardware.md`.
- **Partition Scheme:** Custom `partitions.csv` prioritizing `app0` space (8MB) for baked ROMs.
- **Verified Build Command:**
  ```powershell
  .\arduino-cli.exe compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi" firmware/BmoGameboy
  ```

---

# 5. Architecture & Performance Patterns
- **Memory & Cache:** Place hot structures (like emulator `gb_s` contexts) in internal DRAM and align them to the ESP32-S3 D-cache line size (`__attribute__((aligned(32)))`) to prevent cache straddling penalties. Large idle buffers (like save RAM, ROM payloads, box art) MUST go to PSRAM (`MALLOC_CAP_SPIRAM`).
- **Dual-Core FreeRTOS Pipeline:**
  - **Core 1 (Application & Emulation):** Runs Arduino `setup()` and `loop()`, emulator CPU/PPU step loops, button debouncing, and UI logic.
  - **Core 0 (Display Worker Task):** Hosts `vDisplayTask` dedicated FreeRTOS worker. Core 1 hands off completed double-buffered PSRAM frames via `DisplayEmu::submitFrameAsync()` allowing CPU emulation and SPI display DMA transfers to run in parallel at 60 FPS.
- **SPI Bus Arbitration (`SpiArbiter`):**
  - ST7789 display and MicroSD card share the FSPI bus (SCK GPIO12, MOSI GPIO11).
  - All display transactions and SD card filesystem operations (save states, ROM index cache, box art loading, favorites) MUST acquire `SpiArbiter::lock()` before asserting CS to eliminate bus contention between Core 0 and Core 1.
- **IRAM Placement:** Critical inner-loop rendering functions (e.g., `lcd_draw_line`, `gb_rom_read`) MUST use `IRAM_ATTR` to prevent instruction cache misses.
- **Input Polling:** `Buttons::update()` manages a single global bitmask (`gb_joypad_state`) and enum indexes. It MUST be called exactly **once per `loop()` iteration** (or once per emulator frame, e.g., in `DoomEmu::runFrame()`). Do not poll hardware multiple times per frame.
- **Vendor Core Inclusion:** The `peanut_gb.h` and `walnut_cgb.h` header-only libraries follow the single-translation-unit pattern. Their implementations must be compiled into exactly one `.cpp` file (`emu_peanut.cpp` and `emu_walnut.cpp` respectively) using a namespace wrap to avoid ODR violations.

