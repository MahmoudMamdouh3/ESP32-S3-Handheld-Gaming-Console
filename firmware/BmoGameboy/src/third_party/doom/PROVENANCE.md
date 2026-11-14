# Provenance: doomgeneric / Chocolate DOOM

- **Component:** `doom` engine sources under `src/`
- **Copyright:** Copyright (C) 1993-1996 Id Software, Inc.; Copyright (C) 2005-2014 Simon Howard (`Chocolate DOOM`); `doomgeneric` contributors
- **License (SPDX):** `GPL-2.0-or-later`
- **Compile-time gate:** Enabled by default (`BMO_ENABLE_DOOM 1` in `firmware/BmoGameboy/src/hal/config.h`). Pass `-DBMO_ENABLE_DOOM=0` to exclude DOOM (`python tasks.py build --profile no-doom`; see `THIRD_PARTY_NOTICES.md`).
- **Integration wrapper:** `firmware/BmoGameboy/src/cores/emu_doom.{h,cpp}`
- **Local modifications:** Added ESP32-S3 teardown hooks (`doomgeneric_Destroy`, `W_Shutdown`, `I_ShutdownZone`, `Doom_FreePSRAM`), PSRAM zone allocator integration, and SPI-safe WAD streaming via `emu_doom.cpp`.
