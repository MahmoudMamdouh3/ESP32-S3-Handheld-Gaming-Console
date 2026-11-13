# Stella 2014 (Atari 2600 Core) Provenance

- **Upstream Repository:** https://github.com/libretro/stella2014-libretro
- **Pinned Upstream Commit:** `7d1361e407e63f29e52892655069e5fb4096e691`
- **License:** GNU General Public License v2 or later (`license.txt`)
- **Ported Components (`src/`):** `stella/src/common/*`, `stella/src/emucore/*`, `stella/stubs/Stubs.hxx` -> `Stubs.cpp`, `Thumbulator.c` (with `THUMB_SUPPORT=1` for ARM/DPC+/CDF/BUS cartridges).
- **Exclusions:** `libretro.cxx`, `libretro-common/`, `stella/src/cheat/`, and `stella.pro` (`648 KB` raw uncompiled database).
- **ESP32-S3 Modifications:**
  1. Renamed `.cxx`/`.hxx` to `.cpp`/`.h`, `M6502.ins` -> `M6502_ins.h`, and `Thumbulator_execute.ci` -> `Thumbulator_execute_ci.h` so Arduino-CLI copies and builds all translation units and headers in a single flat `src/` folder.
  2. Changed `DefProps` in `DefProps.h` to `static const char* const DefProps[DEF_PROPS_SIZE][21]` so ~273 KB of pointer tables reside in Flash `.rodata` instead of internal DRAM.
  3. Allocated `TIA` `myCurrentFrameBuffer` and `myPreviousFrameBuffer` (`102 KB`) in Octal PSRAM via `bmo_psram_calloc` / `bmo_psram_free`.
  4. Replaced `<fstream>` / `try`/`catch`/`throw` in `Serializer` and `Cart*`/`Console`/`KidVid`/`MT24LC256` with a non-throwing PSRAM-backed in-memory `Serializer` (`-fno-exceptions` compatible).
