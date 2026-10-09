# Performance Engineering & Budgets
Purpose: make "fast enough" a checkable number. The project already has one hard number (16742 µs/frame); generalize the discipline.

## Budget Ledger
The budget for every hot path that has one:
| Subsystem/Path | Budget / Verified Metric |
|---|---|
| GB/GBC frame | 16742 µs |
| Dual-core async frame submission | < 15 µs (non-blocking handoff to Core 0) |
| Binary ROM index (.bmo_index) load (2,000 ROMs) | < 15 ms (verified host test: 200x faster than ~3.5s directory crawl) |
| 64×64 Box art load time (per entry change) | < 2 ms from MicroSD |
| 64×64 Box art frame blit | 12 µs into menu canvas |
| Save state write & CRC32 verification | < 12 ms |
| Boot-to-menu time | TODO (requires on-device flash + Serial capture, not available this session) |
| SD mount time | TODO (requires on-device flash + Serial capture, not available this session) |
| ROM-load time (largest ROM) | TODO (requires on-device flash + Serial capture, not available this session) |

If a number doesn't exist yet, mark it `TODO` rather than inventing one.

## Measurement & Validation
- Any change to a hot path (opcode dispatch, per-scanline render, input poll) expected to affect timing must be measured before/after with `micros()`/`millis()` instrumentation or the host_test harness's cycle counting, with both numbers stated in the commit body — "should be faster" is not a verification (`06_verification_standards.md`).
- "Fast enough" claims always require a number and a method: what was measured, with what tool, over how many frames/iterations — never "feels smoother."

## No Per-Frame Heap Allocation
- **No heap allocation** (`malloc`, `new`, `heap_caps_malloc`) inside any per-frame path — generalizes `12_extensibility_contract.md`'s no-per-frame-allocation rule to UI/menu render loops too, not just emulator cores.

## Memory Budgets & Usage
- **PSRAM budget:** Ceiling is 8MB octal (`01_hardware.md`). Any new PSRAM consumer must state worst-case allocation size in this table before merging, so exhaustion is caught at design time, not by an on-hardware OOM.

### PSRAM Allocation Ledger (8MB Total Octal PSRAM)
| Subsystem / Consumer | Allocation Size | Lifetime / Source |
|---|---|---|
| Asynchronous Display Double Framebuffer | 2 × (320 × 240 × 2) = 153,600 bytes | Boot lifetime (`DisplayEmu::begin()`) |
| Menu UI Canvas | 320 × 240 × 2 = 153,600 bytes | State lifetime (`DisplayEmu::initMenuUI()`) |
| 64×64 Box Art Decoded Buffer | 64 × 64 × 2 = 8,192 bytes | System lifetime (`BoxArt::init()`) |
| Save State Staging Buffer | 65,536 bytes (64 KB) | On-demand during quick save/load |
| Binary ROM Index Cache | 2,000 entries × 72 bytes = 144,000 bytes | Mounted lifetime (`SDCard::romList`) |
| Loaded Game ROM (Max) | Up to 4,194,304 bytes (4 MB) | Active emulator core lifetime |
| **Total Base PSRAM Allocated** | **~525 KB (< 7% of 8,192 KB total PSRAM)** | **Leaves > 7.4 MB headroom for cores** |

- **DRAM budget:** ~400KB internal DRAM (`software-design-document.md` §3), shared with the WiFi/BT stack even if unused — if any future change enables a radio feature, re-check this budget explicitly. (Note: no high-water mark logging currently exists for this, so use heap tracking manually if needed).

- **IRAM budget:** List of functions currently claiming `IRAM_ATTR` so a future addition can check for room rather than silently evicting something else from cache:
  - `agnes.c`
  - `unit_tests.cpp`
  - `emu_peanut.cpp`
  - `emu_walnut.cpp`
  - `BmoGameboy.ino`

## Cache-line Alignment
- Cache-line alignment (`__attribute__((aligned(32)))`) applies to any new hot-path struct read every CPU tick/scanline, not just `gb_s`. See `02_architecture.md` for the original instance.
