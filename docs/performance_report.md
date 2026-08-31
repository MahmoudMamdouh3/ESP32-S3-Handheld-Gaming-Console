# BMO Guardian Performance & Ground-Truth Audit Report
**Generated:** 2026-08-31 14:44:37  
**Target MCU:** ESP32-S3-N16R8 (240MHz Xtensa LX7, 16MB OPI Flash, 8MB Octal PSRAM)  
**Display:** ST7789VW 2.4" TFT on Shared 80MHz FSPI  
**Ground-Truth Status:** PASS

---

## 1. Executive Summary & Health Dashboard

| Metric | Measured Value | Budget / Limit | Utilization | Status |
| :--- | :--- | :--- | :--- | :--- |
| **Internal SRAM (DRAM)** | 244,352 bytes | 327,680 bytes | 74.6% | ✅ OK |
| **Flash Storage (app0)** | 5,002,020 bytes | 16,777,216 bytes | 29.8% | ✅ OK (Headroom 70%) |
| **Static PSRAM** | 0 bytes | 8,388,608 bytes | 0.0% | ✅ OK |
| **AST Critical Findings** | 0 | 0 allowed | - | ✅ PASS |
| **AST Warnings** | 10 | < 10 allowed | - | ⚠️ WARN |
| **Cppcheck Violations** | 0 | 0 critical | - | ✅ CLEAN |

---

## 2. Hardware Bus & Frame Transmission Physics Model

Mathematical limits of the 80 MHz FSPI display bus and CPU computation budget per resolution:

| Platform / Viewport | Resolution | Frame Size | SPI Transfer | Target FPS | Seq. CPU Budget | Parallel DMA Budget | Bus Saturation |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Game Boy DMG (1.5x Scaled)** | 240×216 | 103,680 B | 10.37 ms | 59.7 | 6.37 ms (38%) | 16.74 ms (100%) | 61.9% |
| **Game Boy Color (1.5x Scaled)** | 240×216 | 103,680 B | 10.37 ms | 59.7 | 6.37 ms (38%) | 16.74 ms (100%) | 61.9% |
| **NES (Native 256x240)** | 256×240 | 122,880 B | 12.29 ms | 60.1 | 4.35 ms (26%) | 16.64 ms (100%) | 73.9% |
| **DOOM (Native 320x200)** | 320×200 | 128,000 B | 12.80 ms | 35.0 | 15.77 ms (55%) | 28.57 ms (100%) | 44.8% |
| **Sega Master System (Native 256x192)** | 256×192 | 98,304 B | 9.83 ms | 59.9 | 6.86 ms (41%) | 16.69 ms (100%) | 58.9% |
| **Game Gear (Native 160x144)** | 160×144 | 46,080 B | 4.61 ms | 59.9 | 12.08 ms (72%) | 16.69 ms (100%) | 27.6% |
| **PC Engine (256x240)** | 256×240 | 122,880 B | 12.29 ms | 59.8 | 4.43 ms (26%) | 16.72 ms (100%) | 73.5% |
| **Atari 2600 (160x192)** | 160×192 | 61,440 B | 6.15 ms | 60.0 | 10.52 ms (63%) | 16.67 ms (100%) | 36.9% |
| **PICO-8 (128x128)** | 128×128 | 32,768 B | 3.28 ms | 30.0 | 30.06 ms (90%) | 33.33 ms (100%) | 9.8% |
| **Sega Genesis (320x224)** | 320×224 | 143,360 B | 14.34 ms | 59.9 | 2.35 ms (14%) | 16.69 ms (100%) | 85.9% |
| **Super Nintendo (256x224)** | 256×224 | 114,688 B | 11.47 ms | 60.1 | 5.17 ms (31%) | 16.64 ms (100%) | 68.9% |
| **WonderSwan (224x144)** | 224×144 | 64,512 B | 6.45 ms | 75.5 | 6.80 ms (51%) | 13.25 ms (100%) | 48.7% |
| **Neo Geo Pocket (160x152)** | 160×152 | 48,640 B | 4.87 ms | 59.7 | 11.88 ms (71%) | 16.74 ms (100%) | 29.1% |
| **Atari Lynx (160x102)** | 160×102 | 32,640 B | 3.27 ms | 75.0 | 10.07 ms (76%) | 13.33 ms (100%) | 24.5% |
| **ColecoVision (256x192)** | 256×192 | 98,304 B | 9.83 ms | 60.0 | 6.84 ms (41%) | 16.67 ms (100%) | 59.0% |
| **Menu UI Fullscreen (320x240)** | 320×240 | 153,600 B | 15.36 ms | 60.0 | 1.31 ms (8%) | 16.67 ms (100%) | 92.2% |

> [!IMPORTANT]
> **Hardware Bus Insight:** At 80 MHz SPI, transmitting a full 320×240 frame consumes **15.36 ms** (92.1% of a 16.67 ms frame budget). In sequential blocking mode, only **1.31 ms** of CPU time remains. **DMA double-buffering is mathematically required** to achieve full 60 FPS on 320×240 without frame drops.

---

## 3. Static AST & Architecture Linter Findings (10 total)

| Severity | Rule ID | File : Line | Issue Description | Suggested Action |
| :--- | :--- | :--- | :--- | :--- |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `d_iwad.c:217` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `d_iwad.c:346` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `d_iwad.c:762` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `d_main.c:1124` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `i_system.c:77` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `i_system.c:291` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `i_system.c:343` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `m_config.c:2045` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `p_saveg.c:70` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |
| `🟡 WARNING` | `NAKED_MALLOC_WITHOUT_SPIRAM` | `v_video.c:757` | Plain malloc() allocates from internal DRAM (327KB budget). Large allocations must use PSRAM. | Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. |

---

## 4. Host-Side Quantitative Microbenchmarks

| Benchmark Kernel | Iterations | Total Time | Avg Latency | Throughput | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **BMO_SDF_Procedural_Face_Render** | 100 | 122.52 ms | 1225.167 µs | 3.34 MOps/s | Evaluates procedural 2D Signed Distance Field (SDF) mathematics across 128x128 grid. |
| **4Pixel_Coalesced_Aligned_Store_Kernel** | 50,000 | 122.26 ms | 2.445 µs | 12.27 MOps/s | Measures throughput of Xtensa LX7 32-bit coalesced memory stores for 3:2 scaling. |
| **O1_Palette_Scanline_Transformation** | 10,000 | 47.01 ms | 4.701 µs | 51.06 MOps/s | Measures 240-pixel scanline indexed-to-BGR565 palette transform throughput. |
| **CPU_Emulation_Opcode_Dispatch** | 500,000 | 37.62 ms | 0.075 µs | 13.29 MOps/s | Simulates CPU opcode fetch-decode-execute cycle dispatch throughput. |