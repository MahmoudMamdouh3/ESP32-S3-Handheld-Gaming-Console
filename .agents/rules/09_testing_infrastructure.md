# Testing Infrastructure
Formalizes the host-side native C/C++ test harnesses and Python verification suites so CPU-execution correctness, framebuffer rendering, and hardware invariants can be verified without a physical board.

## Layout
- [`tools/host_cores_runner.c`](file:///e:/BMO%20Gameboy/tools/host_cores_runner.c) — Native x64 C test harness that compiles and links all 10 custom C emulator cores (`atari.c`, `colem.c`, `lynx.c`, `ngp.c`, `snes.c`, `wswan.c`, `genesis.c`, `sms.c`, `pce.c`, `pico.c`), loads real machine-code test ROMs, executes 50 frames per core (500 frames total), verifies CPU register/RAM transitions, and checks deterministic FNV-1a BGR565 framebuffer hashes. Executed automatically by [`tests/test_native_cores_execution.py`](file:///e:/BMO%20Gameboy/tests/test_native_cores_execution.py).
- [`tools/host_test.cpp`](file:///e:/BMO%20Gameboy/tools/host_test.cpp) — Host-side C++ test harness for `walnut_cgb.h` running Blargg's Game Boy CPU instruction test (`firmware/BmoGameboy/src/assets/roms/tests/cpu_instrs.gb`).
- [`scripts/test_runner.py`](file:///e:/BMO%20Gameboy/scripts/test_runner.py) — Verifies the Game Boy / GBC cartridge header checksum (`0x0134..0x014D`) across all 6 baked ROM C headers in `firmware/BmoGameboy/src/assets/`.
- **Host Compiler:** Microsoft Visual C++ (`cl.exe` via `vcvars64.bat`, Visual Studio 2022 / v18 Community) or system `gcc`/`clang`.

## Test ROM Ledger
| ROM | License | Source | Covers |
|---|---|---|---|
| `cpu_instrs.gb` (Blargg) | Public Domain | https://github.com/retrio/gb-test-roms | 11 sub-tests covering Game Boy CPU instruction correctness and timing |
| Synthetic 10-Core Machine Code Suite | MIT (In-tree in `tools/host_cores_runner.c`) | `tools/host_cores_runner.c` | 6507/TIA, Z80/TMS9928A, 65SC02/Suzy, TLCS-900H/K2GE, 65C816/PPU, V30MZ/VDP, M68000/VDP, Z80/SMS-VDP, HuC6280/VDC, PICO-8 VM |

## What "passing" means, per suite
- **`tests/test_native_cores_execution.py` (`tools/host_cores_runner.c`)**: Compiles all 10 C cores cleanly with 0 errors, executes 50 frames per core, and outputs `SUMMARY|10/10 PASS` with non-blank BGR565 framebuffers and verified CPU state transitions.
- **`tools/host_test.cpp` (`cpu_instrs.gb`)**: Outputs `Passed all tests` over the emulated Game Boy serial link within the 8,000-frame budget. Partial sub-test completion is `IN_PROGRESS`, not `VERIFIED_HOST`.
- **`python -m tools.guardian verify`**: Runs the embedded AST linter (0 critical violations) and all Python + native C unit tests (`120+` tests).
- **`python scripts/validate_repo.py`**: Runs all 7 CI phases including full `arduino-cli` ESP32-S3 binary compilation (`FlashMode=qio, PSRAM=opi`) and `DocTruthVerifier` doc-to-code parity checks.
