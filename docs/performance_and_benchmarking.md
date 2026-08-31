# Performance, Benchmarking & Ground-Truth Engineering Manual

**Project:** BMO-Handheld-Gaming-Console  
**Target Platform:** ESP32-S3-N16R8 (Xtensa LX7 @ 240MHz, 16MB OPI Flash, 8MB Octal PSRAM)  
**Display:** ST7789VW 2.4" SPI TFT (240×320 Physical, 320×240 Landscape Viewport) on Shared 80MHz FSPI Bus  
**Authoritative Framework:** `tools/guardian/` (Guardian Ground-Truth Engine)  

---

## 1. Architectural Philosophy & Ground Truth

To prevent performance degradation, memory fragmentation, and false claims across multiple agent development cycles, all performance metrics in this repository are governed by physical hardware limits and mathematical modeling.

The **Guardian Engine** (`tools/guardian/`) provides an immutable, reproducible evaluation suite combining:
1. **Mathematical Bus & Physics Modeling** (`bus_model.py`): Real-time calculations of SPI wire times, DMA saturation, and compute budgets per resolution.
2. **Static AST & Memory Architecture Linter** (`ast_linter.py`): Automated scanner for 25+ embedded firmware anti-patterns (naked mallocs, stack buffers in loops, missing pragmas).
3. **Xtensa ELF Binary Introspection** (`elf_analyzer.py`): Direct parser for `.iram0`, `.dram0`, `.flash`, and `.psram` memory sections using the ESP32-S3 GCC toolchain (`nm`, `size`, `objdump`, `readelf`).
4. **Host Microbenchmarks** (`host_bench.py`): Cycle-accurate execution timing for CPU opcode dispatch, 2D Signed Distance Field (SDF) mathematics, and coalesced 32-bit memory store kernels.
5. **Firmware Profiler Telemetry** (`profiler.h` / `profiler.cpp`): Compile-time gated (`FEATURE_PROFILER`) 240MHz hardware cycle counter (`RSR CCOUNT`) measuring exact on-device microsecond timing.

---

## 2. Hardware Bus Physics & Timing Budgets

The ST7789 display controller is connected via a shared 4-wire SPI bus running at **80 MHz**.

### Physical Wire Parameters
- **Clock Period ($T_{\text{clk}}$):** $1 / 80\text{ MHz} = 12.5\text{ ns}$
- **Byte Transmission Latency:** $8 \times 12.5\text{ ns} = 100\text{ ns}$ (10 MB/sec peak throughput)
- **Address Window Setup Latency:** $4 \times 100\text{ ns} + \text{CS/DC setup} \approx 1.2\ \mu\text{s}$

### Frame Transfer Latency & Compute Budget Table

| Console / Viewport | Native Res | Rendered Output | Frame Buffer Size | Raw SPI Wire Time | Target FPS | Frame Budget ($T_{\text{frame}}$) | Sequential CPU Remaining | Parallel DMA CPU Budget | Bus Saturation |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Game Boy (DMG)** | 160×144 | 240×216 (1.5×) | 103,680 B | **10.37 ms** | 59.73 Hz | 16.74 ms | **6.37 ms (38.1%)** | **16.74 ms (100%)** | 61.9% |
| **Game Boy Color** | 160×144 | 240×216 (1.5×) | 103,680 B | **10.37 ms** | 59.73 Hz | 16.74 ms | **6.37 ms (38.1%)** | **16.74 ms (100%)** | 61.9% |
| **NES (Famicom)** | 256×240 | 256×240 (1:1) | 122,880 B | **12.29 ms** | 60.10 Hz | 16.64 ms | **4.35 ms (26.1%)** | **16.64 ms (100%)** | 73.9% |
| **DOOM** | 320×200 | 320×200 (1:1) | 128,000 B | **12.80 ms** | 35.00 Hz | 28.57 ms | **15.77 ms (55.2%)** | **28.57 ms (100%)** | 44.8% |
| **Master System** | 256×192 | 256×192 (1:1) | 98,304 B | **9.83 ms** | 59.92 Hz | 16.69 ms | **6.86 ms (41.1%)** | **16.69 ms (100%)** | 58.9% |
| **Game Gear** | 160×144 | 160×144 (1:1) | 46,080 B | **4.61 ms** | 59.92 Hz | 16.69 ms | **12.08 ms (72.4%)** | **16.69 ms (100%)** | 27.6% |
| **PC Engine** | 256×240 | 256×240 (1:1) | 122,880 B | **12.29 ms** | 59.82 Hz | 16.72 ms | **4.43 ms (26.5%)** | **16.72 ms (100%)** | 73.5% |
| **Atari 2600** | 160×192 | 160×192 (1:1) | 61,440 B | **6.15 ms** | 60.00 Hz | 16.67 ms | **10.52 ms (63.1%)** | **16.67 ms (100%)** | 36.9% |
| **PICO-8** | 128×128 | 128×128 (1:1) | 32,768 B | **3.28 ms** | 30.00 Hz | 33.33 ms | **30.05 ms (90.2%)** | **33.33 ms (100%)** | 9.8% |
| **Sega Genesis** | 320×224 | 320×224 (1:1) | 143,360 B | **14.34 ms** | 59.92 Hz | 16.69 ms | **2.35 ms (14.1%)** | **16.69 ms (100%)** | 85.9% |
| **Super Nintendo**| 256×224 | 256×224 (1:1) | 114,688 B | **11.47 ms** | 60.10 Hz | 16.64 ms | **5.17 ms (31.1%)** | **16.64 ms (100%)** | 68.9% |
| **WonderSwan** | 224×144 | 224×144 (1:1) | 64,512 B | **6.45 ms** | 75.47 Hz | 13.25 ms | **6.80 ms (51.3%)** | **13.25 ms (100%)** | 48.7% |
| **Neo Geo Pocket**| 160×152 | 160×152 (1:1) | 48,640 B | **4.87 ms** | 59.73 Hz | 16.74 ms | **11.87 ms (70.9%)** | **16.74 ms (100%)** | 29.1% |
| **Atari Lynx** | 160×102 | 160×102 (1:1) | 32,640 B | **3.27 ms** | 75.00 Hz | 13.33 ms | **10.06 ms (75.5%)** | **13.33 ms (100%)** | 24.5% |
| **ColecoVision** | 256×192 | 256×192 (1:1) | 98,304 B | **9.83 ms** | 60.00 Hz | 16.67 ms | **6.84 ms (41.0%)** | **16.67 ms (100%)** | 59.0% |
| **Menu UI** | 320×240 | 320×240 (Full) | 153,600 B | **15.36 ms** | 60.00 Hz | 16.67 ms | **1.31 ms (7.8%)** | **16.67 ms (100%)** | 92.1% |

---

## 3. The 20-Point Performance Issue Registry (PERF-01..PERF-20)

| ID | Location | Severity | Description | Fix |
| :--- | :--- | :--- | :--- | :--- |
| **PERF-01** | `sd_card.cpp:75` | 🔴 HIGH | SD card mounted at 4 MHz instead of 25 MHz | Change `4000000` to `25000000` (5-6× speedup) |
| **PERF-02** | `BmoGameboy.ino:222` | 🔴 HIGH | `countGamesForConsole` O(N×15) = 245K iter/frame | Cache count array at scan time |
| **PERF-03** | `BmoGameboy.ino:268` | 🟡 MED | `rebuildVisibleGames()` O(N) on every menu frame | Add dirty flag gate |
| **PERF-04** | `display_emu.cpp:443` | 🟢 LOW | 153KB PSRAMCanvas allocated/freed repeatedly | Allocate once in `setup()` |
| **PERF-05** | `agnes.c` (NES) | 🔴 HIGH | `agnes_t` allocated in internal DRAM (327KB) | Patch with `MALLOC_CAP_SPIRAM` |
| **PERF-06** | `emu_nes.cpp:1` | 🟡 MED | Missing `#pragma GCC optimize("O3,unroll-loops")` | Add pragma to file top |
| **PERF-07** | `display_emu.cpp:276` | 🟡 MED | `streamNESFrame` stack buffer, unvectorized loop | Use static aligned buffer + 32-bit stores |
| **PERF-08** | `display_emu.cpp:336` | 🟡 MED | Sequential 196KB blits block CPU | Implement SPI DMA ping-pong buffers |
| **PERF-09** | `BmoGameboy.ino:501` | 📄 DOC | 2000µs spin-tail burns CPU in tight frames | Tune down to ~800µs |
| **PERF-10** | `doomgeneric.c:21` | 🔴 HIGH | `DG_ScreenBuffer = malloc(256KB)` in DRAM | Route to `MALLOC_CAP_SPIRAM` |
| **PERF-11** | `i_system.c:119` | 🔴 HIGH | `zonemem = malloc(4MB)` in DRAM | Route to `MALLOC_CAP_SPIRAM` |
| **PERF-12** | `bmo_face.cpp:65` | 🟡 MED | 3× `sqrtf()` per pixel across 16K pixels | Bounding box culling + fast distance approx |
| **PERF-13** | `bmo_face.cpp:372` | 🟡 MED | 160 separate `pushPixelsAt` calls per blit | Single contiguous window blit |
| **PERF-14** | `display_emu.cpp:310` | 🟡 MED | DOOM palette re-packed every frame | Cache packed palette, dirty-check |
| **PERF-15** | `emu_sms.cpp:1` | 🟡 MED | Missing `#pragma GCC optimize("O3,unroll-loops")` | Add pragma to file top |
| **PERF-16** | `emu_doom.cpp:1` | 🟡 MED | Missing `#pragma GCC optimize("O3")` | Add pragma to file top |
| **PERF-17** | `sd_card.cpp:168` | 🟡 MED | Single-byte/small chunk SD stream reads | Burst buffer read chunks (64KB) |
| **PERF-18** | `buttons.cpp` | 🟢 LOW | 8 individual `digitalRead` calls per frame | Direct 32-bit register read |
| **PERF-19** | `display_emu.cpp:180` | 🟡 MED | 153KB menu transfer blocks CPU for 15.36ms | Background DMA transfer |
| **PERF-20** | `config.h` | 📄 DOC | Shared SPI bus arbitration lacks mutex lock | Add bus lock guard for ISR safety |

---

## 4. How to Use the Guardian CLI

```bash
# Run comprehensive audit
python -m tools.guardian audit

# View bus latency and compute budgets
python -m tools.guardian bus-calc

# Run quantitative microbenchmarks
python -m tools.guardian bench-host

# Introspect compiled ELF binary
python -m tools.guardian profile-elf

# Export full report
python -m tools.guardian report --output report.md
```
