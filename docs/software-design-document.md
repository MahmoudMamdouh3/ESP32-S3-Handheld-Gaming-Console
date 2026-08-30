# Software Design Document (SDD)
**Project:** BMO-Handheld-Gaming-Console  
**Target Platform:** ESP32-S3-N16R8 (16MB OPI Flash, 8MB Octal PSRAM)  
**Target Display:** ST7789VW 2.4" SPI TFT (240×320 Physical, 320×240 Landscape Viewport)  
**Document Version:** 3.0 (Living Architectural Specification)  
**Status:** Approved Engineering Ground Truth  
**Maintained By:** BMO Firmware Engineering Team & Autonomous AI Agents  

---

## 1. Executive Summary & Hardware Ground Truth

The **BMO-Handheld-Gaming-Console** is an ultra-optimized, multi-platform retro gaming handheld engineered around the Espressif ESP32-S3 microcontroller. It merges a real-time procedural mascot animation system ("BMO") with four high-performance retro gaming cores:
1. **Peanut-GB** (Nintendo Game Boy DMG, C99 engine)
2. **Walnut-CGB** (Nintendo Game Boy Color CGB, dual-fetch engine)
3. **Agnes** (Nintendo Entertainment System NES, discrete cycle-stepped engine)
4. **doomgeneric** (Classic DOOM WAD engine ported to ESP32 VFS)

The hardware is permanently soldered onto a custom perfboard. To eliminate guesswork for reviewers and autonomous AI development agents, all subsystems are strictly categorized as **Physically Wired & Active** vs. **Dormant / Future Hardware**:

### Hardware Capability Matrix

| Subsystem | Physical State | Software Module | Gating Macro (`config.h`) | Operational & Electrical Parameters |
| :--- | :--- | :--- | :--- | :--- |
| **MCU Core** | Soldered & Active | ESP32-S3-N16R8 | Mandatory | Dual-core Xtensa LX7 @ 240MHz, 16MB OPI Flash @ 80MHz, 8MB Octal PSRAM @ 80MHz |
| **Display** | Soldered & Active | `src/core/display_emu.cpp/h` | Mandatory | ST7789VW 240×320 TFT on shared FSPI @ 80MHz (`SCK=12, MOSI=11, CS=10, DC=8, RST=9`) |
| **Input** | Soldered & Active | `src/core/buttons.cpp/h` | Mandatory | 8 Game Boy tactile buttons (GPIO 4-7, 16-18, 21), active-low internal pull-ups (`INPUT_PULLUP`) |
| **MicroSD Storage** | Soldered & Active | `src/core/sd_card.cpp/h` | `FEATURE_SD_CARD = 1` | MicroSD on shared FSPI (`MISO=15, CS=13`); dynamic ROM/WAD streaming |
| **Baked ROMs** | Active (Flash) | `src/assets/rom_data.h` | Mandatory Fallback | 1MB Mario Deluxe + 1MB Zelda Ages in Flash `.rodata` partition |
| **BMO Mascot Face** | Active | `src/core/bmo_face.cpp/h` | Built-in | Procedural 2D Signed Distance Field (SDF) math renderer in internal DRAM (32KB) |
| **Battery Monitor** | **Dormant** | `src/core/battery.cpp/h` | `FEATURE_BATTERY_MONITOR = 0` | Complete driver compiled to no-op; no physical divider on GPIO1 (prevents floating ADC bootloop) |
| **I2S Audio DAC** | **Dormant** | `src/core/audio_i2s.cpp/h` | `FEATURE_AUDIO = 0` | Complete driver compiled to no-op; no MAX98357A DAC wired to GPIO 38-40 |
| **FRAM Save Memory**| **Planned** | `src/core/fram_save.cpp/h` | Dormant | I2C FM24C save memory reserved on GPIO 43/44 (`I2C_SDA=43, I2C_SCL=44`) |

> [!IMPORTANT]
> **Zero-Overhead Dormant Invariant:** Gated features (`FEATURE_BATTERY_MONITOR=0`, `FEATURE_AUDIO=0`) compile to zero-overhead no-ops and must **never** be enabled until physical hardware connections are verified soldered. Reading floating GPIO1 triggers bootloops.

---

## 2. Hardware Topology & Complete Pin Assignment

The ESP32-S3-N16R8 utilizes specific pin groupings. Strapping pins (GPIO 0, 3, 45, 46) and Octal PSRAM/Flash dedicated pins (GPIO 33-37) are strictly isolated from user I/O:

```
                      +----------------------------------+
                      |       ESP32-S3-N16R8 (240MHz)    |
                      |   16MB OPI Flash | 8MB OPI PSRAM |
                      +----------------------------------+
                                       |
     +-----------------+---------------+-----------------+-----------------+
     |                 |                                 |                 |
 [FSPI Bus]       [Button GPIOs]                   [Dormant I/O]    [Dormant I2C]
 (Shared 80MHz)   (Active-Low)                     (Gated = 0)      (Reserved)
  SCK  : GPIO 12   UP    : GPIO 4   A      : GPIO 16  BATTERY: GPIO 1   SDA: GPIO 43
  MOSI : GPIO 11   DOWN  : GPIO 5   B      : GPIO 17  I2S_BCLK:GPIO 38  SCL: GPIO 44
  TFT_CS:GPIO 10   LEFT  : GPIO 6   START  : GPIO 18  I2S_LRC :GPIO 39
  TFT_DC:GPIO 8    RIGHT : GPIO 7   SELECT : GPIO 21  I2S_DIN :GPIO 40
  TFT_RST:GPIO 9
  SD_CS: GPIO 13
  SD_MISO:GPIO 15
```

### Complete Pin Assignment Matrix

| Pin / Net | GPIO # | Peripheral | Electrical Config | Invariant / Protection Rule |
| :--- | :--- | :--- | :--- | :--- |
| `TFT_SCK` | 12 | FSPI Clock | Push-Pull, 80MHz | Shared with SD Card clock |
| `TFT_MOSI` | 11 | FSPI Data Out | Push-Pull, 80MHz | Shared with SD Card data in |
| `TFT_CS` | 10 | ST7789 Chip Select | Active-Low Output | Driven LOW only during display transactions |
| `TFT_DC` | 8 | Data / Command Select| Push-Pull Output | LOW = Command, HIGH = Data parameter |
| `TFT_RST` | 9 | ST7789 Hardware Reset| Active-Low Output | Strobed at `DisplayEmu::begin()` |
| `SD_CS` | 13 | SD Chip Select | Active-Low Output | High during TFT blits; asserted during SD transfers |
| `SD_MISO` | 15 | SD Data In | Input with Pull-up | Dedicated MISO line for SD read |
| `BTN_UP` | 4 | D-Pad Up | `INPUT_PULLUP` | Reads LOW on press; single atomic register poll |
| `BTN_DOWN` | 5 | D-Pad Down | `INPUT_PULLUP` | Reads LOW on press; single atomic register poll |
| `BTN_LEFT` | 6 | D-Pad Left | `INPUT_PULLUP` | Reads LOW on press; single atomic register poll |
| `BTN_RIGHT` | 7 | D-Pad Right | `INPUT_PULLUP` | Reads LOW on press; single atomic register poll |
| `BTN_A` | 16 | Action A | `INPUT_PULLUP` | Primary confirm / action key |
| `BTN_B` | 17 | Action B | `INPUT_PULLUP` | Back / secondary action key |
| `BTN_START` | 18 | Start Button | `INPUT_PULLUP` | Console pause / start key |
| `BTN_SELECT`| 21 | Select Button | `INPUT_PULLUP` | Menu combo trigger (`SELECT + UP` exits emulator) |
| `BATTERY_ADC_PIN` | 1 | ADC1 Channel 0 | Analog In | **DORMANT**; do not read when `FEATURE_BATTERY_MONITOR=0` |
| `I2S_BCLK` | 38 | I2S Bit Clock | Output | **DORMANT**; reserved for MAX98357A DAC |
| `I2S_LRC` | 39 | I2S Word Select | Output | **DORMANT**; reserved for MAX98357A DAC |
| `I2S_DIN` | 40 | I2S Serial Data | Output | **DORMANT**; reserved for MAX98357A DAC |
| `I2C_SDA` | 43 | I2C Serial Data | Open-Drain Pull-up | **DORMANT**; reserved for FM24C FRAM |
| `I2C_SCL` | 44 | I2C Serial Clock | Open-Drain Pull-up | **DORMANT**; reserved for FM24C FRAM |

---

## 3. System Architecture & Finite State Machine

The firmware lifecycle is governed by a deterministic finite state machine implemented in [`BmoGameboy.ino`](file:///e:/BMO%20Gameboy/firmware/BmoGameboy/BmoGameboy.ino).

```mermaid
stateDiagram-v2
    [*] --> BOOT_SETUP: Power On / Reset
    BOOT_SETUP --> STATE_CONSOLE_MENU: Serial + SPI + Display + BmoFace(IDLE) + SD Mount
    
    state STATE_CONSOLE_MENU {
        [*] --> RenderConsoleCarousel
        RenderConsoleCarousel --> NavigateConsoles: D-Pad LEFT / RIGHT
        NavigateConsoles --> RenderConsoleCarousel
    }
    
    STATE_CONSOLE_MENU --> STATE_CONSOLE_MUSEUM: Press SELECT (View History & Specs)
    STATE_CONSOLE_MUSEUM --> STATE_CONSOLE_MENU: Press B or SELECT (Return)

    STATE_CONSOLE_MENU --> STATE_GAME_MENU: Press A (Console Selected)
    
    state STATE_GAME_MENU {
        [*] --> RenderGameList
        RenderGameList --> NavigateGames: D-Pad LEFT / RIGHT / UP / DOWN (±10 jumps)
        NavigateGames --> RenderGameList
    }
    
    STATE_GAME_MENU --> STATE_CONSOLE_MENU: Press B (Return to Console Carousel)
    STATE_GAME_MENU --> STATE_EMULATOR: Press A (ROM Selected & Loaded)
    
    state STATE_EMULATOR {
        [*] --> CoreExecution
        CoreExecution --> PollInputs: Read REG_READ(GPIO_IN_REG)
        PollInputs --> RunFrame: Core Step
        RunFrame --> StreamPixels: SPI DMA Stream
        StreamPixels --> FramePacing: 59.73 Hz Hardware Spin
        FramePacing --> CoreExecution
    }
    
    STATE_EMULATOR --> STATE_CONSOLE_MENU: Combo SELECT + UP (Calls Core::destroy())
```

### State Execution Details
1. **`STATE_CONSOLE_MENU`**:
   - Renders 15-system carousel (GB, GBC, NES, WAD, SMS, GG, PCE, ATARI, PICO-8, GENESIS, SNES, WSWAN, NGP, LYNX, COLEM).
   - Displays live game counts discovered from SD card and baked Flash `.rodata`.
   - Renders animated corner mascot face (`BmoFace`, 40×40) with dirty-flag caching (`BmoFace::isDirty()`).
   - Frame-paced at 60 FPS with non-blocking debounce (`DEBOUNCE_MS = 200`).
2. **`STATE_CONSOLE_MUSEUM`**:
   - Displays authentic historical hardware specifications (CPU, clock, RAM, VRAM, sound, resolution).
   - Highlights architectural breakthroughs and hallmark games for the selected console.
3. **`STATE_GAME_MENU`**:
   - Displays scrollable list of available ROMs for the active console (supports up to 2,048 games allocated dynamically in PSRAM).
   - Controls: **LEFT / RIGHT** for single-title browsing, **UP / DOWN** for rapid +/-10 title page jumping across large libraries.
   - Pressing **B** returns to `STATE_CONSOLE_MENU`.
   - Pressing **A** triggers memory allocation, loads ROM into PSRAM (or prepares VFS stream for DOOM), flashes full-screen `BmoFace::HAPPY` celebration, and starts the emulator core.
3. **`STATE_EMULATOR`**:
   - Executes active emulator core frame-by-frame.
   - Paces frames to exact 59.73 Hz Game Boy vsync (16,742 µs) via hybrid FreeRTOS sleep (`delay()`) and hardware timer spin (`ets_delay_us()`).
   - Pressing **SELECT + UP** triggers the universal teardown contract (`Core::destroy()`), frees PSRAM buffers, and safely transitions to `STATE_CONSOLE_MENU`.

---

## 4. Memory Architecture & Latency Hierarchy

The ESP32-S3-N16R8 has a multi-tiered memory hierarchy. Internal SRAM is ultra-fast but constrained (~400KB available for IRAM + DRAM), whereas 8MB external Octal PSRAM provides massive capacity with higher access latency:

```
+-----------------------------------------------------------------------------------+
| ESP32-S3-N16R8 Memory Subsystem                                                   |
|                                                                                   |
| [Internal SRAM: ~400 KB Total]                                                    |
|   ├── IRAM (.text / IRAM_ATTR): Zero-Wait-State Single-Cycle Instruction Execution   |
|   │     ├── gb_rom_read / gb_rom_read16 / gb_rom_read32                           |
|   │     ├── gb_cart_ram_read / gb_cart_ram_write                                  |
|   │     └── lcd_draw_line (1.5x scaling rasterizer with 32-bit aligned stores)    |
|   ├── DRAM (.bss / .data): High-Speed Data & Intermediate Buffers                 |
|   │     ├── struct gb_s emulator CPU context (32-byte cache-line aligned)         |
|   │     ├── rowBuffer[480] (4-byte aligned scanline pixel buffer)                 |
|   │     └── BmoFace procedural 128x128 framebuffer (32KB)                         |
|                                                                                   |
| [External Octal PSRAM: 8 MB @ 80MHz OPI]                                          |
|   ├── Cartridge Save RAM: 128 KB (allocated per active emulator instance)        |
|   ├── Dynamic ROM Buffer: Up to 4 MB (allocated from SDCard via heap_caps)        |
|   ├── ROM Catalog Index: ~140 KB (up to 2,048 ROMs dynamically allocated in PSRAM)|
|   ├── Menu Canvas Buffer: 320x240x2 = 150 KB (freed upon entering game)           |
|   └── DOOM Game Engine Heap: ~6 MB dynamic zone allocations                       |
|                                                                                   |
| [External Octal SPI Flash: 16 MB @ 80MHz OPI]                                     |
|   ├── app0 Partition (8 MB): Firmware executable + Baked ROMs (.rodata)           |
|   └── ffat Partition (~7.9 MB): Internal FAT filesystem storage                   |
+-----------------------------------------------------------------------------------+
```

### Performance Engineering Invariants
1. **32-Byte D-Cache Alignment:** All core CPU state structures (e.g., `struct gb_s`) are declared with `__attribute__((aligned(32)))` to prevent CPU register state from straddling 32-byte cache lines.
2. **Zero-Wait IRAM Execution:** Hot execution paths (ROM read callbacks, scanline rasterizers) are decorated with `IRAM_ATTR`. This avoids instruction cache stalls across the ~280,000 instruction iterations executed per 60Hz frame.
3. **Zero Heap Allocations in Hot Loops:** No emulator core or rendering loop may call `malloc()`, `new`, or `heap_caps_malloc()` during `runFrame()`. All working memory is pre-allocated at `begin()` and released at `destroy()`.
4. **4-Byte Aligned Pixel Stores:** Scaled pixel rasterization converts 4 input pixels to 6 output pixels and writes them directly as three 32-bit aligned stores (`uint32_t* out32`), eliminating unaligned memory penalties on Xtensa LX7.

---

## 5. Display Subsystem & Video Rendering Pipeline

### Physical Display Parameters
- **Controller:** ST7789VW
- **Physical Resolution:** 240 × 320 pixels
- **Orientation:** Landscape (`rotation = 3`, logical resolution 320 × 240)
- **Bus Interface:** Shared Hardware SPI (`FSPI`) running at **80 MHz**
- **Hardware Register Initialization:** ST7789 `MADCTL` register is overwritten at startup (`0xA0 | 0x08`) to set the BGR bit, fixing panel-level red/blue channel inversions.
- **Wire Pixel Format:** **BGR565 Byte-Swapped (Big-Endian on Wire)**. Because `SPI.writeBytes()` transmits the low-order byte first, all lookup tables and UI palettes pre-swap color bytes (`((c & 0xFF) << 8) | ((c >> 8) & 0xFF)`), resulting in zero runtime byte-swapping overhead during frame blits.

### Viewport Mapping & Scaling Matrix

| Platform | Native Resolution | Scaled Resolution | Scaling Algorithm | Viewport Offsets |
| :--- | :--- | :--- | :--- | :--- |
| **Game Boy (GB)** | 160 × 144 | 240 × 216 | 1.5× Nearest-Neighbor (Phase A-A-B-C-C-D) | Centered (`OFFSET_X = 40`, `OFFSET_Y = 12`) |
| **Game Boy Color (GBC)** | 160 × 144 | 240 × 216 | 1.5× Nearest-Neighbor (Phase A-A-B-C-C-D) | Centered (`OFFSET_X = 40`, `OFFSET_Y = 12`) |
| **NES** | 256 × 240 | 256 × 240 | 1.0× Native 1:1 Direct Pixel Blit | Centered (`OFFSET_X = 32`, `OFFSET_Y = 0`) |
| **DOOM** | 320 × 200 | 320 × 200 | 1.0× Native 1:1 Direct Pixel Blit | Letterboxed (`OFFSET_X = 0`, `OFFSET_Y = 20`) |
| **Sega Master System (SMS)** | 256 × 192 | 256 × 192 | 1.0× Native 1:1 Direct Pixel Blit | Centered (`OFFSET_X = 32`, `OFFSET_Y = 24`) |
| **Sega Game Gear (GG)** | 160 × 144 | 160 × 144 | 1.0× Native / 1.5× Scaled Blit | Centered (`OFFSET_X = 80`, `OFFSET_Y = 48`) |
| **PC Engine (PCE)** | 256 × 240 | 256 × 240 | 1.0× Native 1:1 Direct Pixel Blit | Centered (`OFFSET_X = 32`, `OFFSET_Y = 0`) |
| **Atari 2600** | 160 × 192 | 160 × 192 | 1.0× Native 1:1 Direct Pixel Blit | Centered (`OFFSET_X = 80`, `OFFSET_Y = 24`) |
| **PICO-8** | 128 × 128 | 128 × 128 | 1.0× Native 1:1 Direct Pixel Blit | Centered (`OFFSET_X = 96`, `OFFSET_Y = 56`) |
| **Sega Genesis / Mega Drive** | 320 × 224 | 320 × 224 | 1.0× Native 1:1 Direct Pixel Blit | Centered (`OFFSET_X = 0`, `OFFSET_Y = 8`) |
| **Super Nintendo (SNES)** | 256 × 224 | 256 × 224 | 1.0× Native 1:1 Direct Pixel Blit | Centered (`OFFSET_X = 32`, `OFFSET_Y = 8`) |
| **WonderSwan & Color** | 224 × 144 | 224 × 144 | 1.0× Native 1:1 Direct Pixel Blit | Centered (`OFFSET_X = 48`, `OFFSET_Y = 48`) |
| **Neo Geo Pocket & Color** | 160 × 152 | 160 × 152 | 1.0× Native 1:1 Direct Pixel Blit | Centered (`OFFSET_X = 80`, `OFFSET_Y = 44`) |
| **Atari Lynx** | 160 × 102 | 160 × 102 | 1.0× Native 1:1 Direct Pixel Blit | Centered (`OFFSET_X = 80`, `OFFSET_Y = 69`) |
| **ColecoVision / SG-1000** | 256 × 192 | 256 × 192 | 1.0× Native 1:1 Direct Pixel Blit | Centered (`OFFSET_X = 32`, `OFFSET_Y = 24`) |
| **Menu UI & Mascot** | 320 × 240 | 320 × 240 | Full-Screen Native Blit | Full display coverage (`0, 0` to `320, 240`) |

### High-Performance SPI Streaming Protocol (N3)
Traditional display libraries issue window address commands (`setAddrWindow`) for every scanline (144 times/frame), introducing significant bus latency. The firmware utilizes an atomic streaming protocol:

```cpp
// 1. Assert CS once and configure window for entire frame
DisplayEmu::startFrame();       

// 2. Emulator scanline callback streams raw scanlines directly over SPI DMA/FIFO
DisplayEmu::streamPixelRow(rowBuffer, 240);

// 3. Deassert CS once after full frame is rendered
DisplayEmu::endFrame();         
```
This reduces display SPI bus command transactions from **144 transactions/frame to 1 transaction/frame**.

---

## 6. Procedural Mascot Face Engine (`BmoFace`)

The BMO mascot face is rendered entirely via real-time **Procedural 2D Signed Distance Fields (SDF)**, eliminating static bitmap assets from flash.

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

### Technical Specification
- **Zero Static Assets:** Facial features (eyes, pupils, blush, mouth curvature, expressions) are generated dynamically through algebraic distance functions.
- **Dynamic Emotional States:** Supports 9 distinct expressions:
  `IDLE`, `HAPPY`, `SURPRISED`, `SLEEPY`, `LOW_BATTERY`, `CHARGING`, `ERROR`, `SHUTDOWN`, `HIDDEN`.
- **Continuous Parameter Interpolation:** Geometric parameters transition smoothly via continuous exponential decay (`val += (target - val) * 0.2f`), except `ERROR` which snaps instantaneously.
- **Dirty Flag Caching:** `BmoFace::isDirty()` ensures SPI bus blits occur only when facial parameters animate or blink occurs, preserving the frame budget for menu rendering.

---

## 7. Multi-Emulator Integration & Core Contracts

All emulator cores adhere to the unified C++ lifecycle contract:

```cpp
class EmulatorCoreContract {
public:
  static bool begin(const uint8_t* romData, size_t romSize);
  static void updateJoypad();
  static void runFrame();
  static void destroy();
};
```

### 1. Peanut-GB (`src/emulators/emu_peanut.cpp`)
- **Target:** Game Boy DMG (`.gb`).
- **Engine:** `peanut_gb.h` (C99 single translation unit).
- **Colorization:** Pre-computed 256-entry classic green palette table (`PAL_256`) eliminating bitwise operations per pixel.
- **Memory Management:** Allocates 128KB Cartridge RAM in PSRAM at `begin()`; frees Cartridge RAM at `destroy()`.

### 2. Walnut-CGB (`src/emulators/emu_walnut.cpp`)
- **Target:** Game Boy Color (`.gbc`).
- **Engine:** `walnut_cgb.h` (Dual-fetch CGB execution core).
- **Colorization:** Dual-mode scanline renderer supporting native CGB 64-color palettes and optimized 256-entry DMG-on-GBC lookup table (`DMG_ON_GBC_PAL_256`).
- **Memory Safety:** Little-endian byte reconstruction in `gb_rom_read16` and `gb_rom_read32` to ensure unaligned-safe flash access on Xtensa LX7.
- **Memory Management:** Allocates 128KB Cartridge RAM in PSRAM at `begin()`; frees Cartridge RAM at `destroy()`.

### 3. Agnes NES (`src/emulators/emu_nes.cpp`)
- **Target:** Nintendo Entertainment System (`.nes`).
- **Engine:** `agnes.c` / `agnes.h`.
- **Palette Mapping:** Direct mapping via pre-swapped `NES_PALETTE[64]`.
- **Memory Management:** Initializes Agnes internal structures at `begin()`; resets and frees at `destroy()`.

### 4. doomgeneric (`src/emulators/emu_doom.cpp`)
- **Target:** Classic DOOM (`.wad`).
- **Engine:** `doomgeneric` ported to ESP32 VFS.
- **Direct VFS Streaming:** Reads `.wad` data directly from the MicroSD FAT filesystem via POSIX `fopen()` / `fread()`, bypassing PSRAM ROM pre-buffering to maximize available heap for DOOM's game zone memory.
- **Memory Management:** Releases DOOM zone heap memory at `destroy()`.

---

## 8. Input Subsystem & Zero-Wait Hardware Polling

- **Pin Assignments:** 8 GPIO inputs configured with internal pull-up resistors (`INPUT_PULLUP`).
  - D-Pad: `UP` (GPIO 4), `DOWN` (GPIO 5), `LEFT` (GPIO 6), `RIGHT` (GPIO 7)
  - Action Keys: `A` (GPIO 16), `B` (GPIO 17), `START` (GPIO 18), `SELECT` (GPIO 21)
- **Zero-Wait Atomic Register Polling:** The button engine (`buttons.cpp`) polls all button GPIOs in a single atomic hardware register read:
  ```cpp
  uint32_t gpio_in = REG_READ(GPIO_IN_REG);
  ```
- **Active-Low Bitmask Synchronization:** Maintains `Buttons::gb_joypad_state` (0 = pressed, 1 = released). Emulators update joypad state via a single branchless register assignment:
  ```cpp
  gb.direct.joypad = Buttons::gb_joypad_state;
  ```
- **Menu Debouncing:** Menu navigation uses non-blocking `millis()` timestamp tracking (`DEBOUNCE_MS = 200`) to guarantee responsive navigation without blocking FreeRTOS execution.

---

## 9. Storage Subsystem, Partition Table & Dual-ROM Fallback

### Custom Flash Partition Layout (`partitions.csv`)
To accommodate full commercial Game Boy Color ROMs baked directly into Flash `.rodata`, a custom partition table is configured:

| Partition Name | Type | Subtype | Flash Offset | Size | Purpose |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `nvs` | `data` | `nvs` | `0x00009000` | 20 KB | Non-Volatile Storage (settings, calibration) |
| `otadata` | `data` | `ota` | `0x0000E000` | 8 KB | OTA boot selection metadata |
| `app0` | `app` | `ota_0`| `0x00010000` | **8 MB** | Application binary + Baked ROM `.rodata` |
| `ffat` | `data` | `fat` | `0x00810000` | ~7.9 MB | Internal Flash FAT filesystem |

### Shared FSPI Bus Arbitration Contract
The MicroSD card and ST7789 TFT display share the FSPI peripheral:
- **Shared Bus Lines:** `SCK` (GPIO 12), `MOSI` (GPIO 11)
- **Dedicated Lines:** `TFT_CS` (GPIO 10), `TFT_DC` (GPIO 8), `TFT_RST` (GPIO 9), `SD_CS` (GPIO 13), `SD_MISO` (GPIO 15)
- **Arbitration Rule:** The SPI bus is claimed exclusively by `DisplayEmu::startFrame()` during display blits. No SD card reads are permitted mid-frame. During DOOM gameplay, disk I/O occurs strictly between frame renders.

### Baked Flash ROM Fallback Mechanism & Large SD Catalog Scaling
At startup, `SDCard::begin()` registers four baked flash ROMs (`mario_deluxe.h`, `zelda_ages.h`, `aladdin.h`, `lego_racers.h` — totaling ~4MB in `.rodata`) at the top of the ROM list before scanning the MicroSD FAT directory. 
- **PSRAM Dynamic Catalog Scaling:** To support massive game libraries without exhausting internal SRAM, `SDCard` and `BmoGameboy.ino` dynamically allocate ROM indexing buffers (`RomFile* romList`, `visibleRomIndexes`, `visibleGames`) in Octal PSRAM (`MALLOC_CAP_SPIRAM`), supporting up to **2,048 games** simultaneously (~140 KB PSRAM footprint).
- **Fallback Guarantee:** If no MicroSD card is inserted, the console falls back gracefully to internal static buffers and built-in flash games.
- **Automated Catalog Staging Tooling:** Complete curated 1G1R (1 Game, 1 Region) libraries for Game Boy (602 games), GBC (538 games), NES (577 games), and Doom WADs (1,720 games total) are downloaded, sanitized ($<60$ chars), and staged onto the SD card root via `scripts/auto_install_romsets.py`.

---

## 10. Dormant Hardware Expansion Drivers

The codebase includes fully architected drivers for future hardware expansions. These drivers compile to zero-overhead no-ops while dormant:

### 1. Battery Power Management (`src/core/battery.cpp`, `FEATURE_BATTERY_MONITOR = 0`)
- **Hardware Topology:** TP4056 LiPo charger paired with a 2:1 precision voltage divider connected to `BATTERY_ADC_PIN` (GPIO 1).
- **Driver Architecture:** Reads raw ADC values, applies calibrated polynomial curve, and calculates a moving average over 16 samples.
- **Deep-Sleep Brownout Guard:** If battery voltage falls below 3.3V, the driver displays `BmoFace::LOW_BATTERY`, pauses 2 seconds, and calls `esp_deep_sleep_start()` to protect the LiPo cell from permanent damage.

### 2. Digital I2S Audio (`src/core/audio_i2s.cpp`, `FEATURE_AUDIO = 0`)
- **Hardware Topology:** MAX98357A Mono Class-D I2S DAC connected to `I2S_BCLK` (GPIO 38), `I2S_LRC` (GPIO 39), `I2S_DIN` (GPIO 40).
- **Driver Architecture:** ESP-IDF I2S driver operating with DMA circular buffers, providing non-blocking audio synthesis and natural frame-pacing for 60Hz emulation.

### 3. I2C FRAM Save States (`src/core/fram_save.cpp`)
- **Hardware Topology:** Fujitsu MB85RC / FM24C non-volatile Ferroelectric RAM on `I2C_SDA` (GPIO 43) / `I2C_SCL` (GPIO 44).
- **Driver Architecture:** Provides 10^14 write endurance for instant save state persistence without wear-out or FAT corruption risks.

---

## 11. Verification Pipeline & Development Tooling

```
+-------------------------------------------------------------------------+
| Automated Verification Pipeline                                         |
|                                                                         |
| [Host Unit & CPU Tests]  --> [AI Guardian Repo Validation]              |
|  - zig c++ host_test.cpp      - python scripts/validate_repo.py         |
|  - Blargg cpu_instrs.gb       - Python syntax & ROM checksums           |
|  - python -m unittest         - Hard stop & symbol freshness check      |
|                                                                         |
|                              |                                          |
|                              v                                          |
|                    [Arduino CLI Build]                                  |
|                     - 16MB OPI Flash + 8MB Octal PSRAM                  |
|                     - Zero compilation errors/warnings                  |
+-------------------------------------------------------------------------+
```

1. **Host-Side CPU Test Harness (`tools/host_test.cpp`):** Standalone desktop test harness compiled via Zig (`zig c++`). Executes Blargg's `cpu_instrs.gb` test suite in headless mode to verify opcode correctness and CPU timings in under 3 seconds without hardware flashing.
2. **AI Guardian Repository Validator (`scripts/validate_repo.py`):** Multi-phase CI validator that audits Python script syntax, Nintendo ROM checksums, firmware safety guardrails, and symbol reference consistency.
3. **Firmware Build Verification Command:**
   ```powershell
   .\arduino-cli.exe compile --fqbn "esp32:esp32:esp32s3:FlashMode=opi,FlashSize=16M,PartitionScheme=custom,PSRAM=opi" firmware/BmoGameboy
   ```

---

## 12. Autonomous AI Agent Governance & Ruleset Index

To allow autonomous AI agents and human engineers to develop and extend the firmware without introducing regressions or hardware damage, the repository operates under strict governance rules stored in [`.agents/rules/`](file:///e:/BMO%20Gameboy/.agents/rules/README.md):

- **Hard Stops (`00_hard_stops.md`):** Absolute constraints preventing hardware damage, bootloops, and unaligned memory crashes.
- **Symbol Ground Truth (`10_symbol_reference.md`):** Complete registry of verified public APIs, types, and macros.
- **Extensibility & Teardown Contract (`12_extensibility_contract.md`, `26_emulator_exit_contract.md`, `32_modular_core_template.md`):** Universal `destroy()` lifecycle and step-by-step modular core addition scaffolding.
- **Common Mistakes Catalogue (`30_common_agent_mistakes.md`):** Anti-pattern registry cataloguing historical failure modes (M-1 through M-20) with immediate preventions.
- **Agent Handoff & Optimization Cycle (`33_agent_handoff_and_optimization_cycle.md`):** Standardized protocol for continuous anonymous agent-to-agent progress and issue tracking.
- **Quick-Start Primer (`31_quick_start_primer.md`):** 90-second zero-context on-ramp enabling any LLM to safely contribute to the codebase.
