# Software Design Document (SDD)
**Project:** GB-Emu-ESP32S3
**Platform:** ESP32-S3 (N16R8)

## 1. Introduction
This document outlines the software architecture, design decisions, and implementation details of the GB-Emu-ESP32S3 project. The project is a highly optimized, portable, multi-platform handheld gaming system utilizing the ESP32-S3 microcontroller. It supports Game Boy, Game Boy Color, NES, and DOOM.

## 2. System Architecture
The firmware is built using the Arduino IDE/CLI core for ESP32. The system employs a state machine to manage the lifecycle between the Main Menu, the Game Selection UI, and active emulator instances.
- **Hardware Context:** ESP32-S3 with 16MB Flash and 8MB Octal PSRAM.
- **Partition Scheme:** A custom `partitions.csv` is required (allocating an 8MB `app0` partition) to support the compiled footprint of the 4 emulators and baked C-header ROMs.

## 3. Memory Management
The ESP32-S3's internal DRAM (~400KB) is highly constrained. The architecture relies on careful memory mapping:
- **PSRAM Allocation:** All dynamically loaded ROM buffers (up to 4MB) and the DOOM rendering heaps are dynamically allocated into external Octal PSRAM using `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`.
- **IRAM_ATTR Execution:** Performance-critical loops (e.g., pixel rendering loops, emulator cartridge read/write callbacks) are forced into internal zero-wait-state instruction RAM (`IRAM_ATTR`). This avoids instruction cache misses during the ~280,000 function calls per frame.
- **Cache Alignment:** Emulator state structs (like the CPU registers in `gb_s`) are strictly aligned to the ESP32-S3's 32-byte D-cache boundaries using `__attribute__((aligned(32)))` to prevent cache thrashing.

## 4. Multi-Emulator Integration
The architecture dynamically boots one of four emulator cores based on the ROM extension:
- **Peanut-GB (`.gb`):** A lightweight C99 Game Boy emulator. Optimized with pre-computed classic green 256-color palettes.
- **Walnut-CGB (`.gbc`):** Game Boy Color support. Utilizes dual-fetch modes and a 256-entry DMG-on-GBC palette lookup table to avoid bitwise arithmetic during the render loop.
- **Agnes (`.nes`):** An embedded NES emulator wrapped via `agnes.c`.
- **doomgeneric (`.wad`):** The classic DOOM engine, utilizing its own file stream logic to read from the SD card without pre-buffering into PSRAM (unlike the 8-bit consoles).

## 5. Display and Graphics Subsystem
- **ST7789 TFT (240x320):** Operated via a shared hardware SPI bus (`FSPI`) running at 80 MHz.
- **Hardware Bug Fix:** The ST7789 MADCTL register is manually overwritten at boot (`0xA0 | 0x08`) to toggle the BGR bit, fixing an issue with generic panels rendering swapped Red/Blue channels natively.
- **SPI Streaming:** The rendering loop abandons traditional `Adafruit_GFX` pixel-by-pixel pushes. A `startFrame()` function opens a single SPI `setAddrWindow` for the entire screen, and subsequent scanlines are streamed raw over the bus using `SPI.writeBytes()`.
- **Nearest-Neighbor Scaling:** The native 160x144 Game Boy resolution is scaled exactly 1.5x to 240x216 using phase-matched row duplication (A A B C C D) and centered on the 320x240 screen.

## 6. Input Handling
- **Physical Interface:** 8 GPIO buttons using internal pull-ups (`INPUT_PULLUP`). 
- **Debouncing:** Menu navigation uses a non-blocking `millis()` timestamp tracker to prevent double inputs.
- **Zero-Wait Polling:** During active gameplay, the traditional function-call overhead for button reading is bypassed. A hardware timer or FreeRTOS task updates a single 8-bit `gb_joypad_state` mask in `buttons.cpp`. The emulators perform a branchless read of this mask on every CPU tick.

## 7. Storage and File System (VFS)
- **Shared SPI:** The SD Card breakout shares the `SCK` and `MOSI` pins with the TFT display. To prevent initialization collisions, the firmware explicitly claims the SPI bus before calling `SD.begin()`.
- **Dual ROM Loading:** The system scans the SD card for compatible ROMs. If no SD card is present, it elegantly falls back to initializing any ROMs "baked" directly into the flash memory via C headers (e.g. `mario_deluxe.h`).

## 8. Development Tooling Pipeline
An extensive suite of Python scripts (`scripts/`) ensures reproducible firmware assets:
- **`process_games.py`:** Parses `.zip` ROM archives, extracts binaries, and generates correctly formatted C headers with `PROGMEM` guards. It also generates static 100x100 RGB565 UI cover assets using Pillow.
- **`validate_repo.py`:** Validates python syntax across all tooling scripts and verifies the Nintendo logo header checksum (0x014D) within the ROMs to ensure data integrity before compilation.
