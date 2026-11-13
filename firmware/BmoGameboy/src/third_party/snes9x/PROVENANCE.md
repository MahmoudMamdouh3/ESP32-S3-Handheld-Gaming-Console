# Snes9x (Snes9x2005 / Cardputer-Emulators port)

- Upstream project: Snes9x2005 / ESP32-S3 port from `Cardputer-Emulators`
- Upstream repository: `https://github.com/Automated-Test/Cardputer-Emulators` (local clone in `.tools/upstream/Cardputer-Emulators`)
- Pinned commit SHA: `74b47763c3f2ccbaed6505b83ef0ef37c2deac82`
- Upstream path: `src/snes/snes9x/`
- License: Snes9x non-commercial license (`LICENSE`)
- Local BMO adaptations:
  - `SNES_NO_SOUND=1`, `RIGHTSHIFT_IS_SAR=1`, `FAST_LSB_WORD_ACCESS=1`, `NO_ZERO_LUT=1` set in `src/port.h`.
  - Routed WRAM, VRAM, FillRAM, SRAM, tile/GFX buffers, and DSP allocations to `bmo_psram_*` (`src/hal/psram_alloc.h`), plus full deallocation in `S9xDeinitMemory()`, `S9xDeinitGFX()`, and `S9xDeinitDSP()`.
  - Removed file-path `fopen` loading from `LoadROM()` (uses `Memory.ROM` XIP buffer directly).
  - Added in-memory state snapshots (`src/snapshot_bmo.c`) integrated with `SaveManager`.
  - Enabled host/WASI compilation of `gfx.c` by exposing `psram_alloc.h` outside `#ifdef ESP_PLATFORM` and removing an unused `MALLOC_CAP_*` local.
