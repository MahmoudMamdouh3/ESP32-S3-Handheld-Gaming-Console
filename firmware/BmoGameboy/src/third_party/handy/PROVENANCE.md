# Handy (K. Wilkins / Retro-Go / Cardputer-Emulators Atari Lynx port)

- **Origin**: Adapted from `Cardputer-Game-Station-Emulators` (`src/lynx/handy/`) based on `retro-go` `handy` (derived from Keith Wilkins' Handy).
- **Upstream URL**:
  - `https://github.com/geo-tp/Cardputer-Game-Station-Emulators.git` (commit `74b47763c3f2ccbaed6505b83ef0ef37c2deac82`)
  - `https://github.com/ducalex/retro-go.git` (commit `4ced120669750ca7228fd0414211430c1d923166`)
- **License**: zlib License (`license.txt`).
- **Local Modifications**:
  1. Switched `LSS_FILE` / `lss_read` / `lss_write` / `lss_printf` from `FILE*` to bounds-checked in-memory serialization for `SaveManager`.
  2. Moved `CRam::mRamData` (64 KiB Lynx RAM) and BS93 homebrew copies to PSRAM (`../../hal/psram_alloc.h`).
  3. Removed stdio `FILE*` loading/saving in `CEEPROM::Load/Save` and `CSystem(const char*, ...)`, exposing `CEEPROM::Data()` for `SaveManager`.
