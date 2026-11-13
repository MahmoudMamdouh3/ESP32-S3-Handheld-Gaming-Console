# Provenance: Peanut-GB

- **Component:** `peanut_gb` single-header Game Boy emulator (`peanut_gb.h`, `peanut_gb_config.h`)
- **Copyright:** Copyright (c) 2018-2023 Mahyar Koshkouei (with portions from SameBoy, Copyright (c) 2015-2019 Lior Halphon, `https://github.com/LIJI32/SameBoy/`)
- **License (SPDX):** `MIT`
- **Integration wrapper:** `firmware/BmoGameboy/src/cores/emu_peanut.{h,cpp}`
- **Local modifications:** Project feature header `peanut_gb_config.h` (`ENABLE_SOUND 1`, `ENABLE_LCD 1`) ensures matching `struct gb_s` layout across translation units.
