# Provenance: agnes

- **Component:** `agnes` NES emulator (`agnes.h`, `agnes.c`)
- **Copyright:** Copyright (c) 2022 Krzysztof Gabis
- **Upstream:** `https://github.com/kgabis/agnes`
- **License (SPDX):** `MIT`
- **Integration wrapper:** `firmware/BmoGameboy/src/cores/emu_nes.{h,cpp}`
- **Local modifications:** Kept `agnes.h` / `agnes.c` API intact; ESP32-S3 PSRAM buffer routing, joypad mapping, and display scanline callbacks live in `emu_nes.cpp` (plus `IRAM_ATTR` hot-loop annotation where present in `agnes.c`).
