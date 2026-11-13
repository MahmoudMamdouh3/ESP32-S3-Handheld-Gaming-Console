# Fake-08 + z8lua + LodePNG Provenance

- **Upstream Repository:** https://github.com/jtothebell/fake-08
- **Pinned Fake-08 Commit:** `814991a2571ad3970e3b6cef48f3b148aa1c27b9` (MIT License)
- **Pinned z8lua Commit:** `e6928578d46b61fd5ea30cfcf547e855a30a0553` (MIT License; Eris persistence MIT; Zepto-8 charmap WTFPL2; LodePNG zlib License — see `LICENSE.MD`)
- **Exclusions:** `main.cpp`, `test/`, `platform/`, `hostCommonFunctions.cpp` (desktop INI/pack-in helper), `libs/miniz/`, `libs/simpleini/`, `libs/z8lua/ltests.{c,h}` (unused Lua debug test harness).
- **ESP32-S3 Modifications:**
  1. Consolidated `source/`, `libs/z8lua/` (renamed `.c` -> `.cpp`, matching upstream `CC=$(CXX)` C++ fixed-point build), and `libs/lodepng/` into a flat `src/` folder with local `"..."` includes for Arduino-CLI.
  2. Enabled `#define LUA_USE_LONGJMP 1` in `luaconf.h` so `z8lua` uses `setjmp`/`longjmp` instead of C++ exceptions (`-fno-exceptions`).
  3. Enabled `#define LODEPNG_NO_COMPILE_DISK 1` in `lodepng.h` and disabled `freopen`/desktop logger output in `logger.cpp` and `system()` in `loslib.cpp`.
  4. Replaced `<regex>` and `<codecvt>` in `emojiconversion.*` and `cart.*` with deterministic UTF-8 longest-match / line-prefix parsers, and `<fstream>` in `filehelpers.cpp` with safe empty stubs (`Vm::LoadCart(const unsigned char*, size_t)` loads directly from memory).
  5. Routed `lua_State` heap allocations in `vm.cpp` (`lua_newstate(bmo_pico_lua_alloc, nullptr)`) to Octal PSRAM via `bmo_psram_realloc` / `bmo_psram_free`.
  6. Replaced `exit()` in `loslib.cpp` `os_exit()` with `luaL_error()` and omitted unused `ltests.h`/`ltests.cpp` so no host-abort path can tear down firmware RTOS tasks.
  7. Added `__wasi__` fallback for `lua_tmpnam` (`loslib.cpp`) and explicit `<stdlib.h>` includes (`lpico8lib.cpp`, `printHelper.cpp`, `stringToDataHelpers.cpp`, `cart.cpp`) for host/WASI `libc++` builds.
