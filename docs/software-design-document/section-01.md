# SDD Section 01: Scope and System Truth Matrix

[Back to the SDD index](../software-design-document.md)

## 1.1 Scope

This repository contains firmware for a soldered ESP32-S3-N16R8 handheld, 14 emulator wrappers and their engines, SD-card catalog/save support, a local browser simulator with selected WebAssembly cores, host verification tools, and the Guardian rule/index tooling. The firmware entry point is [`BmoGameboy.ino`](../../firmware/BmoGameboy/BmoGameboy.ino). The root [`README.md`](../../README.md) is the user-facing quick start; this SDD explains implementation and verification boundaries.

## 1.2 Runtime environments

| Environment | Executes | Can establish | Cannot establish |
| :--- | :--- | :--- | :--- |
| ESP32-S3 target | Arduino firmware, core wrappers, storage and display drivers | The **Target compile verified** label under the exact conditions in [Section 1.3](section-01.md#13-truth-labels) and [Section 14.2](section-14.md#142-target-compilation); after device testing, behavior on this board | Hardware behavior, timing, or anything not built or measured in that run |
| Wokwi | The configured sketch and simulated parts in `diagram.json` | Selected control-flow and wiring-model behavior | Electrical safety or exact behavior of the soldered modules |
| Browser simulator | JavaScript, WebGL, browser storage and included WASM modules | Browser interaction, simulator layout, selected engine behavior | Firmware timing, SD electrical levels, panel output, power, or full core coverage |
| Native host | Selected C/C++ test runners and Python tooling | Deterministic logic and supported engine execution on the host | Xtensa-specific timing, ESP32 memory placement, or SPI behavior |
| Source analysis | Parsed source, constants, formulas and generated maps | Structural and model-level claims when the analyzer is correct | Runtime outcomes and hardware measurements |

The browser simulator server is loopback-only. Its host-core test endpoint is POST-only, requires an in-memory session token and same-origin request, rejects request bodies, limits launches, and invokes a fixed test command. See [Section 13](section-13.md).

## 1.3 Truth labels

- **Configured:** a source macro, build profile, or code path requests the value.
- **Model:** a formula or source-derived budget predicts a value under stated assumptions.
- **Target compile verified:** a fresh Arduino CLI build with the committed `esp32s3` profile passes, the generated partition table matches the repository table, and the app image fits `app0`. This is compile evidence only.
- **Host verified:** a test/tool ran on a desktop. Name the exact command and result.
- **Simulator verified:** browser or Wokwi behavior was observed; name the simulator.
- **Hardware measured:** a human tested the soldered board and retained procedure, instrument, raw result, and pass criterion in [`hardware_measurements.md`](../hardware_measurements.md).
- **Unknown / proposed:** evidence is missing or the design is not implemented.

Do not upgrade a claim's evidence label based on a compile, code comment, old report, or simulator run.

## 1.4 Subsystem ownership and present evidence

| Subsystem | Main owners | Present source-defined behavior | Evidence gap |
| :--- | :--- | :--- | :--- |
| Pins and features | `src/core/config.h`, `buttons.*`, `01_hardware.md` | GPIO assignment table; SD on; audio and battery monitor off; optional FRAM pins proposed | No new electrical measurements |
| Launcher and state | `BmoGameboy.ino`, `display_emu.*` | Boot, console carousel, game list, emulator, pause, diagnostics and idle states | Button behavior not retested on unit |
| Display and SPI | `display_emu.*`, `spi_arbiter.*`, `theme.h` | ST7789 command/pixel operations; 80 MHz TFT default, 40 MHz diagnostic comparison, 25 MHz SD | 80 MHz timing and panel color remain unmeasured |
| ROM catalog and SD | `sd_card.*`, `rom_index.h`, `path_key.h` | Extension dispatch, bounded ROM pool, BMOIDX02 cache, favorites/history/art metadata | FAT timestamp behavior, recovery, and card throughput need device testing |
| Persistence | `save_manager.*`, `battery_save.h`, wrappers | Keyed state/battery files, validated writes, staged load, metadata gates | Power-loss and title-level round trips need SD/device testing |
| Emulator engines | `src/emulators/`, `src/vendor/`, `src/engine/` | Wrappers provide lifecycle, input, frame, state and save calls where implemented | Support and accuracy vary by core/title; use compatibility ledger |
| BMO face | `bmo_face.*`, `theme.h` | Procedural renderer and expression controls | Panel appearance is not calibrated by host tests |
| Telemetry | `BmoGameboy.ino`, `tools/telemetry_plotter.py` | Per-second FPS/execution summary and a non-overlapping 600-sample percentile window | No current board capture |
| Simulator | `tools/bmo_simulator/` | Browser UI, WebGL model, selected WASM engines, upload and local host-test endpoint | Not an electrical or full firmware emulator |
| Verification | `tests/`, `tools/guardian/`, `scripts/validate_repo.py` | Unit/static/source checks and a pinned CI target compile | Local and CI results are dated evidence; neither proves physical hardware behavior |

## 1.5 Feature flags and hardware presence

| Feature | Current configuration | Meaning |
| :--- | :---: | :--- |
| `FEATURE_SD_CARD` | `1` | Firmware includes and initializes the configured SD path. The card/module still needs physical testing. |
| `FEATURE_AUDIO` | `0` | Audio output is disabled; no DAC/I2S wiring is recorded. |
| `FEATURE_BATTERY_MONITOR` | `0` | GPIO1 is not sampled. Getters return zero while disabled; UI reports USB/N/A or not fitted. |
| Profiler | `FEATURE_PROFILER` defaults to `0` | Cycle profiler APIs are compiled out unless explicitly enabled; main loop does not claim zone reports. |
| FRAM | Not physically recorded | `I2C_SDA=43`, `I2C_SCL=44` are planned only. |

## 1.6 Data boundaries

Treat `games/`, `roms/`, and `sdcard/` as user data. Repository checks must not rewrite or delete those directories. The tracked baked ROM headers under `firmware/BmoGameboy/src/assets/roms/` are repository source assets; their policy/provenance is separate and awaits the user's licensing direction. See [ADR 0008](../decisions/0008-rom-policy-and-licensing.md).

## 1.7 Navigation

Continue with [hardware constraints](section-02.md), [pin ownership](section-03.md), [boot behavior](section-05-part-01.md), and the [current issue ledger](../../.agents/rules/04_known_issues.md).
