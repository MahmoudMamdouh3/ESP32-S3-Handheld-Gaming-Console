# Project State Summary for AI Agents (`docs/ai/STATE.md`)

- **Branch / Baseline:** `refactor/principal-overhaul` (branched from `techlead/audit-2` @ `0b32109`, audited `2026-11-16`).
- **Hardware Target:** Hand-soldered `ESP32-S3-DevKitC-1-N16R8` (`44-pin` carrier, `2.54 × 6.33 cm`, carrying `ESP32-S3-WROOM-1-N16R8` with 16 MB **Quad SPI** flash + 8 MB **Octal SPI** PSRAM) on a `31 × 46` hole (`7.87 × 11.68 cm`, `0.254 cm` pitch) perfboard inside an `8.80 × 12.40 × 3.20 cm` BMO shell ([`hardware/layout.json`](../../hardware/layout.json)).
- **Display & Storage:** `2.4"` (`6.10 cm` diagonal, `240×320`) ST7789VW SPI TFT + MicroSD slot sharing FSPI (`SCK=12`, `MOSI=11`, `MISO=15`, `TFT_CS=10`, `TFT_DC=8`, `TFT_RST=9`, `SD_CS=13`) serialized via `SpiArbiter::ScopedLock`.
- **Buttons:** 8 soldered (`INPUT_PULLUP`: `UP=4`, `DOWN=5`, `LEFT=6`, `RIGHT=7`, `A=16`, `B=17`, `START=18`, `SELECT=21`); 4 configured unsoldered expansion (`X=14`, `Y=42`, `L=47`, `R=48`).

## Verification & Evidence Status

| Category | Status | Details |
| :--- | :--- | :--- |
| **`HARDWARE_TESTED`** | **`0` records** | Physical board bring-up not yet executed. See [`docs/guides/bringup.md`](../guides/bringup.md) and [`firmware/bringup/bringup.ino`](../../firmware/bringup/bringup.ino). |
| **`TARGET_COMPILED`** | **Verified** | All 3 main firmware profiles (`default`, `no-doom`, `mit`) + `firmware/bringup` compile with `0` first-party warnings under `esp32:esp32@3.3.11` (`FlashMode=qio,PSRAM=opi`). |
| **`HOST_TESTED`** | **Verified** | Python unit tests (`tests/`, `tools/guardian/tests/`), native C++17 `CTest` suite (`tests/host/`), and WASI/WASM core tests pass. |

## Active Hard Stops (Do Not Change Without Owner Approval)

1. `FEATURE_BATTERY_MONITOR=0` and `FEATURE_AUDIO=0` in `firmware/BmoGameboy/src/hal/config.h`.
2. `FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi` in `sketch.yaml` and `partitions.csv`.
3. Current GPIO mapping in `config.h` / `pins_check.h` (never assign strapping `0, 3, 45, 46` or flash/PSRAM `26–37`).
4. Licensing split: Only the `mit` profile (`GB`, `GBC`, `NES`) is redistributable as a binary; `default` and `no-doom` link non-commercial cores and are personal-use local builds only ([`THIRD_PARTY_NOTICES.md`](../../THIRD_PARTY_NOTICES.md)). First-party SPDX split awaits **Decision 9** (staged unmerged on `license/spdx-review`).

## Next Actions & Open Owner Decisions

- Owner decisions awaiting human sign-off are queued in [`docs/owner-decisions.md`](../owner-decisions.md).
- Step-by-step physical bring-up instructions for the owner are in [`docs/guides/owner-guide.md`](../guides/owner-guide.md) and [`docs/guides/bringup.md`](../guides/bringup.md).

<!-- SYNC:BEGIN -->
- **Synced Metrics (`sync_ai_codemap.py`):** `121` first-party firmware C/C++ files (`17,048` lines, max `ui_menus.cpp`=`648`), `17` scripts, `31` C++ host test suites.
<!-- SYNC:END -->
