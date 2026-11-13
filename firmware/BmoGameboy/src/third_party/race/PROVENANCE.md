# RACE (Neo Geo Pocket / Neo Geo Pocket Color Emulator)

- Upstream repository: `libretro/RACE` (`c7810dd7f172827bfa2004813bc000b13786636b`) / `Cardputer-Emulators` (`74b47763c3f2ccbaed6505b83ef0ef37c2deac82`)
- License: GNU GPL v2 (`LICENSE` / `license.txt`)
- Local BMO adaptations:
  - Excluded `koyote.bin` binary blob; uses compiled C table (`koyote_bin.h`).
  - Defined `USE_CZ80=1`, `CZ80=1`, `NGP_ONLY_RENDER_VISIBLE_LINES=1`, `NGP_OPTIMIZATION_16BIT_READ=1`, `DISABLE_WATCHDOGS=1` in `types.h`.
  - Fixed `flashShutdown()` cleanup bug, separated `cpuram` (`0x08a0`) from `mainram` (`0x18000`) to avoid aliasing, and fixed OOB `&mainram[0x20000]` copy in `state.c`.
  - Routed TLCS-900H tables, CZ80 tables, graphics buffers, sound state, mainram/cpurom/flash save buffer to PSRAM (`src/hal/psram_alloc.h`) with full deallocation.
  - Renamed global `link()` opcode handler (`tlcs900h.c`) to `tlcs_link()` to avoid POSIX `link()` symbol collision under host/WASI libc.
