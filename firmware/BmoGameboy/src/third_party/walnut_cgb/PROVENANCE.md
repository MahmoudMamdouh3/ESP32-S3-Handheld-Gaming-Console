# Provenance: Walnut-CGB

- **Component:** `walnut_cgb` Game Boy / Game Boy Color header emulator (`walnut_cgb.h`)
- **Copyright:** Walnut-CGB additions Copyright (c) 2025-2026 Mr. Paul (`https://github.com/Mr-PauI`), forked from Peanut-GB (Copyright (c) 2018-2023 Mahyar Koshkouei) and portions of SameBoy (Copyright (c) 2015-2019 Lior Halphon)
- **License (SPDX):** `MIT`
- **Integration wrapper:** `firmware/BmoGameboy/src/cores/emu_walnut.{h,cpp}`
- **Local modifications:** Hosted under `src/third_party/walnut_cgb/` with ESP32-S3 PSRAM memory routing and scanline push handled in `emu_walnut.cpp`; 16/32-bit bulk read/write and DMA paths hardened with `memcpy`-based helpers (`walnut_load_u16`/`walnut_load_u32`/`walnut_store_u16`/`walnut_store_u32`) to eliminate strict-aliasing/alignment UB and separate WRAM Bank 0 (`0xC000-0xCFFF`) from banked WRAM (`0xD000-0xDFFF`).
