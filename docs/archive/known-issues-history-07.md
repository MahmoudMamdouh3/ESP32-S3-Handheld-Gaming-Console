# Archived known-issue history (source archived 2026-11-07)

## 3. Bus Latency & Bandwidth Limitations (BUS-01 to BUS-05)

The table below illustrates the hardware physics calculations modeled in `tools/guardian/core/bus_model.py` for an 80 MHz SPI bus with 16-bit BGR565 byte-swapped color:

| Platform / Mode | Native Resolution | Rendered Output | Frame Bytes | Wire Transfer Time | Target FPS | Sequential CPU Left | Parallel DMA Budget | Bus Saturation |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Game Boy DMG** | 160×144 | 240×216 (3:2) | 103,680 B | **10.37 ms** | 59.73 | **6.37 ms (38.1%)** | 16.74 ms (100%) | 61.9% |
| **Game Boy Color** | 160×144 | 240×216 (3:2) | 103,680 B | **10.37 ms** | 59.73 | **6.37 ms (38.1%)** | 16.74 ms (100%) | 61.9% |
| **NES** | 256×240 | 256×240 | 122,880 B | **12.29 ms** | 60.10 | **4.35 ms (26.1%)** | 16.64 ms (100%) | 73.9% |
| **Classic DOOM** | 320×200 | 320×200 | 128,000 B | **12.80 ms** | 35.00 | **15.77 ms (55.2%)** | 28.57 ms (100%) | 44.8% |
| **Sega Master System** | 256×192 | 256×192 | 98,304 B | **9.83 ms** | 59.92 | **6.86 ms (41.1%)** | 16.69 ms (100%) | 58.9% |
| **Sega Game Gear** | 160×144 | 160×144 | 46,080 B | **4.61 ms** | 59.92 | **12.08 ms (72.4%)** | 16.69 ms (100%) | 27.6% |
| **PC Engine (TG-16)** | 256×240 | 256×240 | 122,880 B | **12.29 ms** | 59.82 | **4.43 ms (26.5%)** | 16.72 ms (100%) | 73.5% |
| **Atari 2600** | 160×192 | 160×192 | 61,440 B | **6.15 ms** | 60.00 | **10.52 ms (63.1%)** | 16.67 ms (100%) | 36.9% |
| **PICO-8** | 128×128 | 128×128 | 32,768 B | **3.28 ms** | 30.00 | **30.06 ms (90.2%)** | 33.33 ms (100%) | 9.8% |
| **Sega Genesis** | 320×224 | 320×224 | 143,360 B | **14.34 ms** | 59.92 | **2.35 ms (14.1%)** | 16.69 ms (100%) | 85.9% |
| **Super Nintendo** | 256×224 | 256×224 | 114,688 B | **11.47 ms** | 60.10 | **5.17 ms (31.1%)** | 16.64 ms (100%) | 68.9% |
| **WonderSwan** | 224×144 | 224×144 | 64,512 B | **6.45 ms** | 75.47 | **6.80 ms (51.3%)** | 13.25 ms (100%) | 48.7% |
| **Neo Geo Pocket** | 160×152 | 160×152 | 48,640 B | **4.87 ms** | 59.73 | **11.88 ms (70.9%)** | 16.74 ms (100%) | 29.1% |
| **Atari Lynx** | 160×102 | 160×102 | 32,640 B | **3.27 ms** | 75.00 | **10.07 ms (75.5%)** | 13.33 ms (100%) | 24.5% |
| **ColecoVision** | 256×192 | 256×192 | 98,304 B | **9.83 ms** | 60.00 | **6.84 ms (41.0%)** | 16.67 ms (100%) | 59.0% |
| **Menu UI (Full Canvas)**| 320×240 | 320×240 | 153,600 B | **15.36 ms** | 60.00 | **1.31 ms (7.8%)** | 16.67 ms (100%) | 92.2% |

### BUS-01: Fullscreen 320×240 Menu Blit Sequential Bottleneck (PERF-19)
- **Status:** `RESOLVED_OPTIMIZED (DUAL_CORE_PIPELINE)`
- **Impact:** Transmitting 153,600 bytes synchronously stalled Core 1 for 15.36 ms out of a 16.67 ms frame budget (92.2% SPI bus saturation).
- **Remediation:** Implemented dedicated FreeRTOS Core 0 display worker task (`BMO_Display`), double-buffered Octal PSRAM canvases (`menuCanvasArr[2]`), and recursive FSPI bus arbiter (`SpiArbiter`) eliminating the Core 1 blocking stall (< 50 µs queue dispatch).

### BUS-02: Sega Genesis 320×224 Frame Time Pinch
- **Status:** `MITIGATED (ASYNC_CORE0_STREAM)`
- **Impact:** 14.34 ms wire time leaves only 2.35 ms for 68000 CPU and VDP emulation in sequential mode.
- **Remediation:** `DisplayEmu::streamGenesisFrame` dispatches asynchronously to Core 0 double-buffered `s_asyncEmuBuf[2]`.

### BUS-03: NES / PC Engine 256×240 High Bus Saturation (73.9%)
- **Status:** `MITIGATED_OPTIMIZED`
- **Impact:** 12.29 ms wire time.
- **Remediation:** Hoisted static aligned line buffers `s_nesRowBuf` with 32-bit coalesced stores (PERF-07) and O3 compiler optimization (PERF-06).

---

## 4. Memory Architecture & Heap Limitations (MEM-01 to MEM-06)

### MEM-01: Internal SRAM (DRAM) 327KB Boundary
- **Status:** `VERIFIED_OPTIMAL (65.7% Utilization)`
- **Data:** Static DRAM consumption is **215,200 bytes** (65.7% of 327,680 bytes budget).
- **Constraint:** Internal SRAM must be reserved strictly for DMA descriptors, FreeRTOS stacks, and timing-critical IRAM kernels.
- **Remediation:** All large runtime structures (16,384 ROM catalog slots = 1.18MB, DOOM 64KB screen buffer, DOOM 6MB zone heap, DOOM WAD `lumpinfo`, NES Agnes context = 40KB, Walnut cart RAM = 128KB) are explicitly routed to Octal PSRAM via `MALLOC_CAP_SPIRAM`.

### MEM-02: Flash Partition Table Overhead (FLASH_OVERFLOW_IDE)
- **Status:** `OPEN (DOCUMENTATION_ENFORCED)`
- **Description:** Building via Arduino IDE without manually choosing **Tools → Partition Scheme → Custom** defaults to the standard 3MB partition, triggering a compile error (`158% of program storage space`).
- **Remediation:** Canonical build command specified in `.agents/rules/31_quick_start_primer.md`:
  `.\arduino-cli.exe compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi" firmware/BmoGameboy`

### MEM-03: PSRAM Access Latency Overhead
- **Status:** `VERIFIED_HARDWARE`
- **Description:** Octal PSRAM accesses incur higher latency (80MHz OPI bus) compared to internal SRAM (240MHz CPU bus).
- **Remediation:** High-frequency per-pixel scaling loops utilize 32-bit aligned stack/DRAM line registers (`s_gbRowBuf`, `s_nesRowBuf`, `s_doomLineBuf`) before bursting to PSRAM or SPI.

### MEM-04: Baked ROM Flash `.rodata` Protection
- **Status:** `FIXED_VERIFIED`
- **Description:** Calling `free()` on baked ROM pointers (`mario_deluxe.h`, `zelda_ages.h`, `aladdin.h`, `lego_racers.h`) causes a fatal heap corruption panic because they reside in Flash `.rodata`.
- **Remediation:** `SDCard::freeRom()` checks pointer boundaries and ignores Flash pointers.

### MEM-05: Dynamic 16,384 ROM Catalog Allocation
- **Status:** `FIXED_VERIFIED`
- **Description:** Indexing 16,384 game titles requires ~1.05 MB of RAM (`sizeof(RomEntry) * 16384`).
- **Remediation:** Dynamically allocated in PSRAM at boot; consumes 0 bytes of internal DRAM.

---

## 5. Platform & Emulator Tier Limitations (EMU-01 to EMU-15)

| Platform | Core Engine | Tier Status | Code Status | Known Limitation / Remediation |
| :--- | :--- | :--- | :--- | :--- |
| **Nintendo Game Boy (DMG)** | Peanut-GB (Mahyar Koshkouei v0.8.0) | Tier 1 (Active) | `VERIFIED_HOST` | 1.5× 3:2 software scaling consumes CPU; optimized via 32-bit coalesced memory stores. |
| **Nintendo Game Boy Color (CGB)** | Walnut-CGB (Mahyar Koshkouei fork) | Tier 1 (Active) | `VERIFIED_HOST` | 16-bit fast ops (`WALNUT_GB_16_BIT_OPS`) break GBC compatibility; reverted to 0 (FIXED). |
| **Nintendo Entertainment System (NES)** | Agnes (Krzysztof Gabis) | Tier 1 (Active) | `VERIFIED_HOST` | Context hoisted to PSRAM; requires `#pragma GCC optimize("O3")`. |
| **Classic DOOM (1993)** | DoomGeneric (ozkl / id Software 1.10) | Tier 1 (Active) | `VERIFIED_HOST` | Zone heap and screen buffer routed to PSRAM via `Doom_MallocPSRAM`. |
| **Sega Master System (SMS)** | SMS Plus GX (Charles MacDonald) | Tier 1 (Active) | `VERIFIED_HOST` | Full 256×192 SPI transmission takes 9.83 ms. |
| **Sega Game Gear (GG)** | SMS Plus GX (Charles MacDonald) | Tier 1 (Active) | `VERIFIED_HOST` | 160×144 viewport centered on 320×240. |
| **PC Engine / TurboGrafx-16** | HuC6280 + HuC6270 VDC + HuC6260 VCE | Tier 1 (Active) | `VERIFIED_HOST` | 256×240 PSRAM framebuffer; verified via 50-frame host runner (`0x3FE0E605`). |
| **Atari 2600 (VCS)** | MOS 6507 + 6532 RIOT + TIA Scanline Core | Tier 1 (Active) | `VERIFIED_HOST` | 160×192 PSRAM framebuffer; verified via 50-frame host runner (`0x04001DC5`). |
| **PICO-8 Fantasy Console** | PICO-8 32KB RAM + Graphics/Cart Engine | Tier 1 (Active) | `VERIFIED_HOST` | 128×128 PSRAM framebuffer; verified via 50-frame host runner (`0x57FD3265`). |
| **Sega Genesis / Mega Drive** | Motorola 68000 + YM7101 VDP Core | Tier 2 (Active) | `VERIFIED_HOST` | 320×224 async Core 0 double-buffered stream; verified via 50-frame host runner (`0x0CFE5DC5`). |
| **Super Nintendo (SNES)** | WDC 65C816 + S-PPU + 8-Ch DMA Core | Tier 2 (Active) | `VERIFIED_HOST` | 256×224 PSRAM framebuffer; verified via 50-frame host runner (`0x4F301DC5`). |
| **Bandai WonderSwan / Color** | NEC V30MZ + WonderSwan VDP Core | Tier 2 (Active) | `VERIFIED_HOST` | 224×144 PSRAM framebuffer; verified via 50-frame host runner (`0xDD851515`). |
| **SNK Neo Geo Pocket / Color** | Toshiba TLCS-900/H + K1GE/K2GE Core | Tier 2 (Active) | `VERIFIED_HOST` | 160×152 PSRAM framebuffer; verified via 50-frame host runner (`0x427729C5`). |
| **Atari Lynx** | WDC 65SC02 + Mikey + Suzy SCB/Math Core | Tier 2 (Active) | `VERIFIED_HOST` | 160×102 PSRAM framebuffer; verified via 50-frame host runner (`0x874F2AB9`). |
| **ColecoVision / SG-1000** | Zilog Z80A + TI TMS9918A VDP Core | Tier 2 (Active) | `VERIFIED_HOST` | 256×192 PSRAM framebuffer; verified via 50-frame host runner (`0x47659DC5`). |

---
