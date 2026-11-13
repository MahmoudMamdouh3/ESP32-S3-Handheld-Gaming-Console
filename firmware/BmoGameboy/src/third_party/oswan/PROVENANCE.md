# Oswan (Bandai WonderSwan / WonderSwan Color Emulator)

- Upstream repository: Oswan (`3e4d7bb8271ad390c1d5623179fb19e2cca1acb32`) via `Cardputer-Emulators` (`74b47763c3f2ccbaed6505b83ef0ef37c2deac82`)
- License: GNU GPL v2 (`LICENSE`)
- Local BMO adaptations:
  - `WS_SAVE_ENABLED=1` and `WS_NO_SPLASH_SCREEN=1` set in `WS.h`; removed `startup.h` binary blob and `exit(1)` calls.
  - Fixed `WsRelease()` XIP double-free (`g_ws_rom_xip`), enabled multi-bank SRAM mapping (`WriteIO(0xC1)`), and reset static timers/flash state on `WsReset()`.
  - Routed RAM, IRAM, IO, Palette, FrameBuffer, ModRM, and APU allocations to PSRAM (`src/hal/psram_alloc.h`) with full deallocation.
  - Added `WsCreateFromMemory()` XIP loader and in-memory savestate serializer (`WsSaveStateToMem` / `WsLoadStateFromMem`).
  - Fixed `WsRelease()` double-free on shared `MemDummy` / aliased `RAMMap` slots, called `AllocateBuffers()` inside `WsInit()`, aligned framebuffer copy stride (`SCREEN_WIDTH`), and guarded `<stdint.h>` / `WSAPU_H_` for host/WASI builds.
