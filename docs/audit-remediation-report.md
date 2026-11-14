# Tech-Lead Audit & Remediation Report (Phases 1–14)

**Branch:** `audit/tech-lead`  
**Classification Vocabulary:** `NOT_TESTED` | `HOST_TESTED` | `TARGET_COMPILED` | `HARDWARE_TESTED`  
**Physical Hardware Status (`HARDWARE_TESTED`):** None recorded as of `2026-11-14`. Execute [`docs/hardware-validation-kit.md`](hardware-validation-kit.md) on the soldered ESP32-S3 perfboard to promote runtime hardware claims.

---

## Executive Summary

Across Phases 1–14, the repository transitioned from an AI-governed prototype with tracked large blobs, baked commercial ROM assets, and unverified custom stub engines to a clean-clone-buildable, license-compliant firmware and tooling workspace:

1. **Repository & History Cleanup (Phases 1–4):** Removed baked commercial ROM headers and ROM downloader/scraper scripts; relocated user-data tooling to `BMO_DATA_DIR`; purged `>= 1 MiB` historical blobs (`git filter-repo`); replaced verbose `.agents/rules/` bloat with concise [`AGENTS.md`](../AGENTS.md) and [`firmware/AGENTS.md`](../firmware/AGENTS.md); upgraded all 10 custom stub emulators to genuine upstream cores under `firmware/BmoGameboy/src/third_party/` with full provenance and `GPL-2.0-or-later` compliance ([`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md)).
2. **Build Infrastructure, C++17 Host Suite & Hardening (Phases 5–9):**
   - Added [`tasks.py`](../tasks.py) supporting 7 firmware profiles (`default`, `no-doom`, `mit`, `audio-compile-only`, `battery-compile-only`, `debug-log-compile-only`, `profiler-compile-only`), ELF size budget checks (`app0 <= 85%`, `DRAM <= 85%`), clang-format checks, and CI workflows ([`ci.yml`](../.github/workflows/ci.yml)).
   - Hardened shared SPI (`SpiArbiter::ScopedLock`), boot Safe Mode (`40 MHz` fallback), atomic `BMOIDX03` catalog writes, `BSS2` PSRAM save states, SNES/PCE battery gating, `BmoProfiler`, storage fuzz testing, SM83 opcode suites (`peanut_opcode_test`, `walnut_opcode_test`), and 10/10 host+Python mutation sanity killing (`scripts/run_mutation_sanity.py`).
   - Extracted shared `CoreHelpers` (`firmware/BmoGameboy/src/cores/core_helpers.h`) across 10 console wrappers and eliminated unaligned pointer casts plus WRAM Bank 0 vs Bank 2 aliasing in `walnut_cgb.h`.
3. **Simulator Modularization, Design Tokens & Validation Kit (Phases 10–14):**
   - Split browser simulator responsibilities into `simulator/js/{constants.js, bmo_mode.js, internals.js, museum.js, webserial.js}` with live loopback HTTP contract tests (`tests/test_simulator_security.py`).
   - Added canonical [`design/tokens.json`](../design/tokens.json) checked against `theme.h`, `bmo_face.cpp`, `simulator.css`, and [`DESIGN.md`](../DESIGN.md), shared [`tools/bmo_tools/`](../tools/bmo_tools/__init__.py), `firmware/BmoGameboy/src/app/version.h`, [`.github/workflows/release.yml`](../.github/workflows/release.yml), and [`docs/hardware-validation-kit.md`](hardware-validation-kit.md).

---

## Hardware Hard Stops Preserved (`TARGET_COMPILED`)

- `FEATURE_BATTERY_MONITOR=0` and `FEATURE_AUDIO=0` in `firmware/BmoGameboy/src/hal/config.h` (opt-in compile-only validation profiles require `-DBMO_HARDWARE_CONFIRMED_BATTERY=1` / `-DBMO_HARDWARE_CONFIRMED_AUDIO=1`).
- `FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi` in [`firmware/BmoGameboy/sketch.yaml`](../firmware/BmoGameboy/sketch.yaml) with `8 MB` `app0` in [`firmware/BmoGameboy/partitions.csv`](../firmware/BmoGameboy/partitions.csv).
- Fixed perfboard GPIO map (`TFT_SCK=12`, `TFT_MOSI=11`, `SD_MISO=15`, `TFT_CS=10`, `TFT_DC=8`, `TFT_RST=9`, `SD_CS=13`; wired buttons `4,5,6,7,16,17,18,21`; unpopulated `14,42,47,48`; reserved strapping/PSRAM pins `0,3,33–37,45,46`).

---

## Verification Ledger & Honest Boundaries

| Verification Area | Status Level | Backing Evidence |
| :--- | :--- | :--- |
| Python syntax, structural checks, link integrity & color tokens | `HOST_TESTED` | `python -u scripts/validate_repo.py`, `scripts/check_markdown_links.py`, `scripts/verify_color_tokens.py` |
| Native C++17 unit, storage fuzzing & SM83 opcode tests | `HOST_TESTED` | `CMakeLists.txt` / CTest (25 test targets) |
| Host Python test suite (including WASI 11-core execution & loopback HTTP contracts) | `HOST_TESTED` | `python -m unittest discover -s tests -v` + `tools/guardian/tests` |
| Mutation sanity gate (C++ + Python) | `HOST_TESTED` | `scripts/run_mutation_sanity.py` |
| ESP32-S3 firmware profiles (`default`, `no-doom`, `mit`, `audio-compile-only`, `battery-compile-only`, `debug-log-compile-only`, `profiler-compile-only`) | `TARGET_COMPILED` | `python -u tasks.py build --profile all --warnings strict --fail-on-warnings` (`0` first-party `-Wall -Wextra -Wshadow` warnings) |
| Flash & static DRAM budget gate | `TARGET_COMPILED` | `python -u tasks.py size --elf build/target-compile/BmoGameboy.ino.elf --check-budgets` (`app0 <= 85%`, `DRAM <= 85%`) |
| Soldered ESP32-S3 SPI clock signal integrity, `GPIO 15` 3.3V safety, SD mount & physical button tests | `NOT_TESTED` (`HARDWARE_TESTED` pending) | Pending owner execution of [`docs/hardware-validation-kit.md`](hardware-validation-kit.md) |

---

## Next Action for Owner

Follow [`docs/hardware-validation-kit.md`](hardware-validation-kit.md) steps 1–7 on your soldered unit and record the multimeter, button, SD, and `tools/telemetry_plotter.py` readings to move bench claims from `TARGET_COMPILED` to `HARDWARE_TESTED`.
