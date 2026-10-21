# 39. Mandatory Performance & Benchmark Framework (Guardian Engine)

**Purpose:** Autonomous AI agents frequently reinvent one-off benchmark scripts or make unsubstantiated claims about firmware performance, frame rates, and memory headroom. This rule mandates that all performance profiling, static safety analysis, bus budget calculations, and memory section tracking MUST be conducted through the unified Guardian Ground-Truth Engine (`tools/guardian/`).

---

## 1. The Ground-Truth Authority: `tools.guardian`

The `tools.guardian` framework is the single authoritative tool for evaluating performance and memory safety in this repository:

```bash
# Run full ground-truth audit (AST linter, bus model, ELF breakdown, microbenchmarks)
python -m tools.guardian audit

# Calculate hardware bus limits and CPU compute budgets per console resolution
python -m tools.guardian bus-calc

# Introspect Xtensa binary symbols and section allocations (IRAM, DRAM, Flash, PSRAM)
python -m tools.guardian profile-elf

# Run quantitative host microbenchmarks (SDF math, aligned memory stores, palette transforms)
python -m tools.guardian bench-host

# Generate comprehensive Markdown or JSON audit reports
python -m tools.guardian report --output docs/performance_report.md
```

---

## 2. Mandatory Guardrails for All Future Agents

### Rule 2.1: No Throwaway Scripts
Agents must **never** create one-off benchmarking or AST checking scripts in `scripts/` or `tools/` that duplicate functionality.
- If a new performance check, memory contract, or microbenchmark is needed, it **must be implemented directly as a module inside `tools/guardian/core/`**.
- All additions must be accompanied by unit tests in `tools/guardian/tests/test_guardian.py`.

### Rule 2.2: Hard Bus Physics Invariants
The ST7789 display communicates over a shared 80 MHz FSPI bus with 100 ns byte-transfer latency.
- At 320×240 (153,600 bytes), raw SPI transfer consumes **15.36 ms** (92.1% of a 60 FPS frame).
- Sequential blocking transfers leave only **1.31 ms** for CPU execution at 60 FPS.
- For high-resolution systems (Genesis 320×224, NES 256×240, Menu 320×240), agents must plan for **DMA double-buffering** or scanline interleaving.

### Rule 2.3: Memory Section Allocation Ground Truth
Every memory allocation must respect the ESP32-S3-N16R8 physical partition constraints:
- **Internal SRAM (DRAM)**: Total 327,680 bytes. Global variables use ~244 KB. Free headroom is < 85 KB.
  - **Hard Invariant:** Never allocate framebuffers, cartridge working RAM, or WAD caches via plain `malloc()`.
  - **Mandatory Call:** Always use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` for any allocation > 512 bytes.
- **External PSRAM**: 8,388,608 bytes (8MB Octal SPI). Latency is ~10-20 cycles. Ideal for framebuffers, ROM images, and menu canvases.
- **Internal IRAM**: Used for zero-wait-state instruction cache. All callbacks invoked > 10,000× per frame (e.g. `gb_rom_read`, `lcd_draw_line`) must carry `IRAM_ATTR`.

### Rule 2.4: Compiler Optimization Pragmas
All compute-intensive emulator wrappers (`src/emulators/emu_*.cpp`) and hot display stream functions (`display_emu.cpp`) must begin with:
```cpp
#pragma GCC optimize("O3,unroll-loops")
```

---

## 3. Pre-Commit Performance Checklist

Before submitting or committing any change modifying emulator hot-paths, memory management, or display streaming:

```
[ ] Ran `python -m tools.guardian audit` -> 0 CRITICAL violations.
[ ] Verified that no plain malloc() was introduced for allocations > 512 bytes.
[ ] Verified that all emulator wrappers contain `#pragma GCC optimize("O3,unroll-loops")`.
[ ] Verified that no per-frame O(N) scans were added to loop() or state transitions.
[ ] Verified that SD card clock speed is set to >= 25 MHz (25000000).
[ ] Quoted Guardian audit metrics in the PR / session handoff report.
```
