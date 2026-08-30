# Software Design Document (SDD)
**Project:** BMO-Handheld-Gaming-Console  
**Target Platform:** ESP32-S3-N16R8 (16MB Flash, 8MB Octal PSRAM)  
**Target Display:** ST7789 2.4" SPI TFT (240×320, Landscape)  
**Document Version:** 2.0 (Verified Living Specification)  
**Maintained By:** BMO Firmware Engineering Team & AI Agents  

---

## 1. Executive Summary & Hardware Ground Truth

The **BMO-Handheld-Gaming-Console** is an ultra-optimized, multi-platform retro gaming handheld powered by an ESP32-S3 microcontroller. It integrates a custom procedural animated mascot face ("BMO") with four high-performance retro gaming cores: **Peanut-GB** (Game Boy), **Walnut-CGB** (Game Boy Color), **Agnes** (NES), and **doomgeneric** (DOOM).

To ensure transparency for human reviewers and autonomous AI development agents, hardware capabilities are partitioned into **Physically Wired & Active** vs. **Dormant / Future Hardware**:

### Hardware Capability Matrix

| Subsystem | Physical State | Software Module | Gating Flag (`config.h`) | Operational Notes |
| :--- | :--- | :--- | :--- | :--- |
| **MCU & Core** | Soldered | ESP32-S3-N16R8 | Core v3.3.11 | Dual-core Xtensa LX7 @ 240MHz, 16MB OPI Flash, 8MB Octal PSRAM |
| **Display** | Soldered & Active | `display_emu.cpp/h` | Mandatory | ST7789 240×320 TFT on shared FSPI @ 80MHz |
| **Input** | Soldered & Active | `buttons.cpp/h` | Mandatory | 8 Game Boy tactile buttons (GPIO 4-7, 16-18, 21), active-low pull-ups |
| **SD Card Storage** | Soldered & Active | `sd_card.cpp/h` | `FEATURE_SD_CARD = 1` | MicroSD on shared FSPI; supports dynamic ROM/WAD streaming |
| **Baked ROMs** | Active (Flash) | `sd_card.cpp` | `FEATURE_SD_CARD` fallback | 1MB Mario Deluxe + 1MB Zelda Ages baked in flash `.rodata` |
| **BMO Mascot Face** | Active | `bmo_face.cpp/h` | Built-in | Procedural 2D Signed Distance Field (SDF) math renderer in DRAM |
| **Battery Monitor** | **Dormant** | `battery.cpp/h` | `FEATURE_BATTERY_MONITOR = 0` | Complete driver exists; dormant (no voltage divider soldered to GPIO1) |
| **I2S Audio DAC** | **Dormant** | `audio_i2s.cpp/h` | `FEATURE_AUDIO = 0` | Complete driver exists; dormant (no MAX98357A DAC wired to GPIO 38-40) |
| **FRAM Save State**| **Planned** | `fram_save.cpp/h` | Dormant | I2C FM24C save memory reserved on GPIO 43/44 |

> [!IMPORTANT]
> Gated features (`FEATURE_BATTERY_MONITOR=0`, `FEATURE_AUDIO=0`) compile to zero-overhead no-ops and must **never** be enabled until the physical hardware connections are verified soldered.

---

## 2. System Architecture & State Machine

The firmware runtime lifecycle is governed by a lightweight, deterministic finite state machine implemented in [`BmoGameboy.ino`](file:///e:/BMO%20Gameboy/firmware/BmoGameboy/BmoGameboy.ino).

```
 +---------------------------------------------------------+
 |                      setup() Boot                       |
 |  Serial -> SPI -> Display -> BmoFace(IDLE) -> SD Mount  |
 +---------------------------------------------------------+
                              |
                              v
                   +---------------------+
                   | STATE_CONSOLE_MENU  |<---------------------+
                   | Select GB/GBC/NES/WAD|                     |
                   +---------------------+                     |
                              | (A Button)                     |
                              v                                |
                   +---------------------+                     |
                   |   STATE_GAME_MENU   |                     |
                   | Select ROM file/WAD |                     |
                   +---------------------+                     |
                        | (A Button)      | (B Button)         |
                        v                 +-------------------->
                   +---------------------+                     |
                   |   STATE_EMULATOR    |                     |
                   | Active Emulation    |---------------------+
                   | 59.73 Hz Pacing     |   (SELECT + UP)
                   +---------------------+   destroy() called
```

### State Definitions
1. **`STATE_CONSOLE_MENU`**: Renders console carousel (GB, GBC, NES, DOOM) with live game counts, animated mascot corner face, and battery indicator.
2. **`STATE_GAME_MENU`**: Displays the scrollable ROM library for the chosen console, highlighting baked flash games and SD card files. Pressing **B** returns to console selection; pressing **A** triggers dynamic ROM load, mascot `HAPPY` splash, and boots the core.
3. **`STATE_EMULATOR`**: Executes the active console frame-by-frame. Pressing **SELECT + UP** triggers the universal teardown contract (`destroy()`), frees allocated PSRAM buffers, and safely transitions back to `STATE_CONSOLE_MENU`.

---

## 3. Memory Architecture & Performance Engineering

The ESP32-S3 features ~400KB internal SRAM and 8MB external Octal PSRAM. Because internal SRAM is scarce and octal PSRAM exhibits access latency, memory placement is strictly partitioned:

```
+-------------------------------------------------------------------------+
| ESP32-S3 Memory Map                                                     |
|                                                                         |
| [Internal SRAM (~400KB)]                                                |
|   ├── IRAM (.text / IRAM_ATTR): Zero-wait-state hot execution paths     |
|   │     ├── gb_rom_read / gb_rom_read16 / gb_rom_read32                 |
|   │     ├── gb_cart_ram_read / gb_cart_ram_write                        |
|   │     └── lcd_draw_line (Xtensa vector-optimized pixel blitter)       |
|   ├── DRAM (.bss / .data): Fast state structs & intermediate buffers    |
|   │     ├── gb_s emulator state (32-byte cache-line aligned)            |
|   │     ├── rowBuffer[480] (4-byte aligned scanline buffer)             |
|   │     └── BmoFace 128x128 SDF framebuffer (32KB)                      |
|                                                                         |
| [External Octal PSRAM (8MB @ 80MHz OPI)]                                |
|   ├── Cartridge Save RAM: 128KB (allocated per emulator instance)       |
|   ├── Dynamic ROM Buffer: Up to 4MB (loaded from SD via heap_caps)      |
|   ├── Display Menu Canvas: 320x240x2 = 150KB (freed when game starts)   |
|   └── DOOM Working Heap: Dynamic engine allocations                     |
+-------------------------------------------------------------------------+
```

### Performance Engineering Rules
- **32-Byte D-Cache Alignment:** All core CPU emulator structs (e.g. `struct gb_s` in Walnut and Peanut) are declared with `__attribute__((aligned(32)))` to prevent CPU register state from straddling 32-byte cache lines.
- **Zero-Wait IRAM Execution:** Scanline rasterizers (`lcd_draw_line`) and ROM read callbacks are marked `IRAM_ATTR`. This avoids instruction cache stalls across the ~280,000 instruction iterations executed per 60Hz frame.
- **Zero Heap Allocations in Hot Paths:** Emulators and render loops are strictly prohibited from calling `malloc` or `heap_caps_malloc` during runtime frames. All working memory is acquired at `begin()` and released at `destroy()`.
- **4-Byte Aligned Memory Stores:** Pixel rasterization routines process pixels in 4-input batches to produce 6 scaled output pixels, perfectly mapped to three 32-bit aligned stores (`uint32_t* out32`), eliminating unaligned memory store penalties on the Xtensa LX7 core.

---

## 4. Display Subsystem & Video Rendering Pipeline

### Physical Display Parameters
- **Controller:** ST7789VW
- **Physical Resolution:** 240 × 320 pixels (operated in Landscape orientation, `rotation = 3`, effective dimensions 320 × 240)
- **Bus Interface:** Shared Hardware SPI (`FSPI`) running at **80 MHz**
- **Hardware Register Fix:** ST7789 `MADCTL` register is overwritten at initialization (`0xA0 | 0x08`) to set the BGR bit, correcting panel-level red/blue channel inversions.
- **Wire Pixel Format:** **BGR565 Byte-Swapped (Big-Endian on Wire)**. Because `SPI.writeBytes()` transmits the low-order byte first, all emulator palette lookup tables and UI assets pre-swap color bytes (`((c & 0xFF) << 8) | ((c >> 8) & 0xFF)`) to achieve zero runtime byte-swapping overhead during frame blits.

### Viewport Mapping & Scaling

| Console | Native Resolution | Rendered Resolution | Scaling Method | Viewport Positioning |
| :--- | :--- | :--- | :--- | :--- |
| **Game Boy (GB)** | 160 × 144 | 240 × 216 | 1.5× Nearest-Neighbor (Phase A-A-B-C-C-D) | Centered (`OFFSET_X = 40`, `OFFSET_Y = 12`) |
| **Game Boy Color (GBC)** | 160 × 144 | 240 × 216 | 1.5× Nearest-Neighbor (Phase A-A-B-C-C-D) | Centered (`OFFSET_X = 40`, `OFFSET_Y = 12`) |
| **NES** | 256 × 240 | 256 × 240 | 1.0× Native 1:1 Pixel Mapping | Centered horizontally (`OFFSET_X = 32`, `OFFSET_Y = 0`) |
| **DOOM** | 320 × 200 | 320 × 200 | 1.0× Native 1:1 Pixel Mapping | Letterboxed vertically (`OFFSET_X = 0`, `OFFSET_Y = 20`) |
| **UI & Mascot Face**| 320 × 240 | 320 × 240 | Full-Screen Native Blit | Full display coverage (`0, 0` to `320, 240`) |

### High-Performance SPI Streaming Protocol (N3)
Traditional display libraries issue a command window transaction for every pixel or scanline (`setAddrWindow` 144 times/frame), introducing bus dead-time. The firmware eliminates this via an atomic streaming protocol:
```cpp
DisplayEmu::startFrame();       // Asserts CS, issues setAddrWindow(240x216) ONCE
emulator_run_frame();           // Scanline callbacks stream raw bytes via DisplayEmu::streamPixelRow()
DisplayEmu::endFrame();         // Deasserts CS
```
This reduces display SPI bus command overhead from **144 transactions/frame to 1 transaction/frame**.

---

## 5. Procedural Mascot Face Engine (`BmoFace`)

Unlike traditional consoles that rely on static bitmap textures, the BMO mascot face is rendered entirely via **Procedural 2D Signed Distance Fields (SDF)**.

```
       [ Mathematical Parameters ]
  eye_radius, eye_squash, mouth_curvature
                   │
                   ▼
  [ 2D Signed Distance Field Evaluation ]
        d_eye(x, y), d_mouth(x, y)
                   │
                   ▼
  [ Analytical Anti-Aliasing: smoothstep() ]
                   │
                   ▼
  [ 128x128 RGB565 Framebuffer in DRAM (32KB) ]
                   │
                   ▼
  [ DisplayEmu::pushPixelsAt() SPI Blit ]
```

### Key Technical Characteristics
- **Zero Asset Footprint:** Does not consume Flash `.rodata` for bitmap frames. All facial features (eyes, pupils, blush, mouth curves, expressions) are generated dynamically in code.
- **Dynamic Emotional States:** Supports 9 expressions (`IDLE`, `HAPPY`, `SURPRISED`, `SLEEPY`, `LOW_BATTERY`, `CHARGING`, `ERROR`, `SHUTDOWN`, `HIDDEN`).
- **Smooth Interpolation:** Facial geometry parameters transition smoothly across states via continuous exponential decay (`val += (target - val) * 0.2f`), except `ERROR` which snaps instantly.
- **Dirty Flag Caching:** `BmoFace::isDirty()` ensures SPI bus transactions occur only when facial expressions change or a blink animation occurs, preserving frame budget during static menu displays.

---

## 6. Multi-Emulator Integration & Core Contracts

The firmware integrates four distinct emulator engines under a unified C++ wrapper contract:

```cpp
class EmulatorCoreContract {
public:
  static bool begin(const uint8_t* romData, size_t romSize);
  static void updateJoypad();
  static void runFrame();
  static void destroy();
};
```

### 1. Peanut-GB (`emu_peanut.cpp`)
- **Target:** Game Boy DMG (`.gb`).
- **Engine:** `peanut_gb.h` (C99 single translation unit).
- **Colorization:** Pre-computed 256-entry classic green palette table `PAL_256` avoiding bitwise operations per pixel.

### 2. Walnut-CGB (`emu_walnut.cpp`)
- **Target:** Game Boy Color (`.gbc`).
- **Engine:** `walnut_cgb.h` (9,937 lines, dual-fetch execution core).
- **Colorization:** Dual-mode scanline renderer supporting native CGB 64-color palettes and optimized 256-entry DMG-on-GBC lookup table (`DMG_ON_GBC_PAL_256`).
- **Memory Safety:** Little-endian byte reconstruction in `gb_rom_read16` and `gb_rom_read32` to ensure unaligned-safe flash access on Xtensa LX7.

### 3. Agnes NES (`emu_nes.cpp`)
- **Target:** Nintendo Entertainment System (`.nes`).
- **Engine:** `agnes.c` / `agnes.h`.
- **Palette Mapping:** Direct mapping via pre-swapped `NES_PALETTE[64]`.

### 4. doomgeneric (`emu_doom.cpp`)
- **Target:** Classic DOOM (`.wad`).
- **Engine:** `doomgeneric` ported to ESP32 VFS.
- **Direct VFS Streaming:** Reads `.wad` file data directly from the SD FAT filesystem via POSIX `fopen()` / `fread()`, bypassing PSRAM ROM pre-buffering to reserve memory for DOOM's internal game heap.

---

## 7. Input Subsystem & Zero-Wait Polling

- **Pin Assignments:** 8 GPIO inputs configured with internal pull-up resistors (`INPUT_PULLUP`).
  - D-Pad: `UP` (GPIO 4), `DOWN` (GPIO 5), `LEFT` (GPIO 6), `RIGHT` (GPIO 7)
  - Action Buttons: `A` (GPIO 16), `B` (GPIO 17), `START` (GPIO 18), `SELECT` (GPIO 21)
- **Zero-Wait State Polling:** The button engine (`buttons.cpp`) polls all button GPIOs in a single atomic hardware register read `REG_READ(GPIO_IN_REG)`.
- **Active-Low Bitmask Synchronization:** Maintains `Buttons::gb_joypad_state` (0 = pressed, 1 = released). Emulators update their joypad registers via a single branchless register assignment:
  ```cpp
  gb.direct.joypad = Buttons::gb_joypad_state;
  ```
- **Menu Debouncing:** Menu navigation employs non-blocking `millis()` timestamp tracking (`DEBOUNCE_MS = 200`) to guarantee responsive navigation without blocking the FreeRTOS scheduler.

---

## 8. Storage, Partition Layout & Dual-ROM Subsystem

### Flash Partition Layout (`partitions.csv`)
Because the firmware bakes multiple complete commercial GBC ROMs into Flash `.rodata` for out-of-the-box play, a custom partition table is required:

| Partition Name | Type | Subtype | Flash Offset | Size | Purpose |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `nvs` | `data` | `nvs` | `0x00009000` | 20 KB | Non-Volatile Storage (settings) |
| `otadata` | `data` | `ota` | `0x0000E000` | 8 KB | OTA selection data |
| `app0` | `app` | `ota_0`| `0x00010000` | **8 MB** | Application binary + Baked ROM `.rodata` |
| `ffat` | `data` | `fat` | `0x00810000` | ~7.9 MB | On-flash FAT filesystem |

### Shared SPI Bus Contract
The MicroSD card and the ST7789 TFT display share the FSPI peripheral:
- **Shared Lines:** `SCK` (GPIO 12), `MOSI` (GPIO 11)
- **Dedicated Lines:** `TFT_CS` (GPIO 10), `TFT_DC` (GPIO 8), `TFT_RST` (GPIO 9), `SD_CS` (GPIO 13), `SD_MISO` (GPIO 15)
- **Arbitration Rule:** The SPI bus is claimed exclusively by `startFrame()` during display rendering. No SD card reads are permitted mid-frame. During DOOM gameplay, file stream reads occur strictly between frame blits.

### Dual-ROM Fallback Mechanism
At startup, `SDCard::begin()` registers baked flash ROMs (`mario_deluxe.h`, `zelda_ages.h`) at the top of the ROM list before scanning the SD card FAT table. If no SD card is inserted, the console runs baked ROMs seamlessly without crashing.

---

## 9. Future Hardware Subsystems (Dormant Drivers)

The codebase contains fully designed, production-ready drivers for future hardware expansions. These drivers are gated by compile-time flags and remain dormant:

### 1. Battery Power Management (`battery.cpp`, `FEATURE_BATTERY_MONITOR = 0`)
- **Hardware Design:** TP4056 LiPo charger paired with a 2:1 precision voltage divider connected to `BATTERY_ADC_PIN` (GPIO 1).
- **Driver Architecture:** Reads ADC raw values, applies calibrated polynomial curve, and calculates a moving average over 16 samples.
- **Deep-Sleep Protection:** If battery voltage falls below 3.3V, the driver displays `BmoFace::LOW_BATTERY`, waits 2 seconds, and calls `esp_deep_sleep_start()` to protect the LiPo cell from permanent damage.

### 2. Digital I2S Audio (`audio_i2s.cpp`, `FEATURE_AUDIO = 0`)
- **Hardware Design:** MAX98357A Mono Class-D I2S DAC wired to `I2S_BCLK` (GPIO 38), `I2S_LRC` (GPIO 39), `I2S_DIN` (GPIO 40).
- **Driver Architecture:** ESP-IDF I2S driver operating with DMA circular buffers, providing non-blocking audio synthesis and natural frame-pacing for 60Hz emulation.

### 3. I2C FRAM Save States (`fram_save.cpp`)
- **Hardware Design:** Fujitsu MB85RC / FM24C non-volatile Ferroelectric RAM on `I2C_SDA` (GPIO 43) / `I2C_SCL` (GPIO 44).
- **Driver Architecture:** Provides 10^14 write endurance for instant, wear-free save state writes without FAT filesystem corruption risks.

---

## 10. Development Tooling & Verification Pipeline

1. **Host-Side CPU Test Harness (`tools/host_test.cpp`):** Standalone desktop test harness compiled via Zig (`zig c++`). Executes Blargg's `cpu_instrs.gb` test suite in headless mode to verify opcode correctness and CPU timings in under 3 seconds without hardware flashing.
2. **ROM Tooling Pipeline (`scripts/`):**
   - `process_games.py`: Validates Nintendo header checksums (`0x014D`) and exports ROM binaries into PROGMEM C header arrays.
   - `validate_repo.py`: Enforces codebase Python syntax and header sanity checks.

---

## 11. Multi-Agent AI Development Environment & Governance

To enable autonomous coding agents and human contributors to collaborate without introducing regressions or hardware damage, this repository implements strict governance rules stored in [`.agents/rules/`](file:///e:/BMO%20Gameboy/.agents/rules/README.md):

- **Hard Stops (`00_hard_stops.md`):** Zero-tolerance rules preventing bootloops (e.g., reading floating GPIO1, changing OPI flash modes, hanging on missing peripherals).
- **Verification Hierarchy (`06_verification_standards.md`):** Strict status vocabulary (`OPEN`, `FIXED_UNVERIFIED`, `VERIFIED_HOST`, `VERIFIED_HARDWARE`). Claims must be backed by literal test output executed in the active session.
- **Symbol Reference (`10_symbol_reference.md`):** Ground-truth symbol table to eliminate invented/hallucinated APIs.
- **Extensibility & Teardown Contract (`12_extensibility_contract.md`, `26_emulator_exit_contract.md`):** Universal `destroy()` contract preventing PSRAM heap leaks when switching between cores.
- **Common Mistakes Catalogue (`30_common_agent_mistakes.md`):** Institutional anti-pattern registry (M-1 to M-16) cataloguing historical failures and immediate preventions.
- **Quick-Start Primer (`31_quick_start_primer.md`):** Rapid on-ramp enabling any LLM to safely navigate and modify the codebase with zero friction.
