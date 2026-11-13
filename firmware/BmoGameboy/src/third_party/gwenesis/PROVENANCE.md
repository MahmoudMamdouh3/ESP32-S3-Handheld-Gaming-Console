# Gwenesis (Mega Drive / Genesis Emulator)

- Upstream repository: `https://github.com/bzhxx/gwenesis` (`168e46661a086a0b793a9ee127a206604ebe7df1`) via `Cardputer-Emulators` (`74b47763c3f2ccbaed6505b83ef0ef37c2deac82`)
- License: GNU GPL v3 (`LICENSE`)
- Local BMO adaptations:
  - Excluded unused `Z80.o`, debuggers (`ConDebug.c`, `Debug.c`), and duplicate `*_full.h` tables.
  - Split `m68ki_instruction_jump_table.h` into `_part1.h` and `_part2.h` (< 650 KB each).
  - Converted cross-directory header `#include` directives to explicit relative paths for Arduino IDE/CLI builds.
  - Fixed `sizeof(CRAM/CRAM565/SAT_CACHE/VSRAM)` pointer-size bugs in `gwenesis_vdp_mem.c`.
  - Routed M68K/Z80/VDP/bus/YM2612 allocations to PSRAM (`src/hal/psram_alloc.h`) and added full `*_free` cleanup functions.
  - Replaced filesystem savestate helpers with PSRAM section-tagged archive (`savestate/gwenesis_savestate.c`).
  - Exposed `psram_alloc.h` outside `#ifdef ESP_PLATFORM` (`m68kcpu.c`) and guarded address-error `setjmp` trapping under `!defined(__wasi__)` for clean WASI host tests.
