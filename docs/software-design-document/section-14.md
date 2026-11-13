# SDD Section 14: Verification and CI

[Back to the SDD index](../software-design-document.md)

## 14.1 Local entry points

`python scripts/validate_repo.py` runs repository phases numbered 0 through 8. Its default mode clearly reports a skipped target compile and remains useful for local source checks. CI must use strict mode; `--require-target-compile` or `BMO_REQUIRE_TARGET_COMPILE=1` makes an unavailable Arduino CLI a failure. Host C/C++ fixture tests also fail instead of skipping for a missing compiler when that environment variable is set.

| Phase | Current scope |
| ---: | :--- |
| 0 | Pinned Arduino CLI target compile, repository partition-table parity, and app image fit against app0; optional skip in default mode |
| 1 | Python syntax compilation |
| 2 | Firmware hard-stop, teardown, and release-header/profile debug-macro guardrails |
| 3 | Static vendor-source flash budget |
| 4 | Wrapper, stream-method, and manifest structure checks |
| 5 | ROM archive data health for repository fixtures/assets |
| 6 | Rule and manifest integrity |
| 7 | Cross-document and rule truth checks, including local Markdown links |
| 8 | UI design-system checks |

`python -m unittest discover -s tests -v` runs the Python suite, including host C/C++ fixtures and selected native engine runners. The final suite exit status is the relevant result; partial output is not a pass. `python -m tools.guardian verify` runs Guardian analyzers and its test suite. `python scripts/check_markdown_links.py` runs the standalone Markdown link check. `python scripts/verify_color_tokens.py` compares registered colors, packed BGR565 values, source-local face colors, and selected contrast pairs across firmware, CSS, DESIGN, and the SDD. `ui-audit`, `patch-check`, and `diff-audit` have narrower scopes and should be run separately when the change touches those areas.

## 14.2 Target compilation

The committed profile is [`firmware/BmoGameboy/sketch.yaml`](../../firmware/BmoGameboy/sketch.yaml), selected as `esp32s3`. It pins Arduino-ESP32 3.3.11, Adafruit BusIO 1.17.4, Adafruit GFX Library 1.12.6, and Adafruit ST7735 and ST7789 Library 1.11.0 with FQBN `esp32:esp32:esp32s3:CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi,USBMode=default`. CI installs those same versions and compiles with `arduino-cli compile --profile esp32s3`. A result is **Target compile verified** only when a fresh build passes, the generated partition table matches [`firmware/BmoGameboy/partitions.csv`](../../firmware/BmoGameboy/partitions.csv), and the `.bin` app image fits the 8 MiB `app0` partition. The CLI's custom partition menu reports a 16 MiB maximum, so that summary alone is not the app0 size check.

The release build keeps `TFT_SPI_HZ` at its configured default. To compile the configured 40 MHz comparison, use `arduino-cli compile --profile esp32s3 --build-property compiler.cpp.extra_flags=-DTFT_SPI_HZ=40000000UL firmware/BmoGameboy`. `compiler.cpp.extra_flags` adds the override while preserving the ESP32 core's required build definitions. `config.h` emits a compile-time diagnostic when the selected clock exceeds the published timing described in [Section 7.1](section-07.md#71-panel-configuration-and-published-timing). The pinned Arduino-ESP32 SPI/HAL source findings, including the APB divider and GPIO-matrix MISO path, are also recorded there. The profile uses TinyUSB CDC; the pinned `USBCDC.cpp` write path can wait under TX backpressure, so setup selects `Serial.setTxTimeoutMs(0)` and logging is best-effort. The checked source path implements zero-timeout writes as non-blocking. Do not flash the board as part of automated validation. Compilation confirms source integration and link constraints; it does not verify hardware behavior.

## 14.3 Continuous integration

`.github/workflows/ci.yml` pins every GitHub Action by full commit SHA. CI installs the Arduino versions from the sketch profile and the pinned WASI SDK, builds the target, runs Guardian budget and ELF reports against that fresh ELF, runs host tests and `python scripts/validate_repo.py --require-target-compile`, runs `python -m tools.guardian verify`, `python scripts/check_markdown_links.py`, `python scripts/verify_color_tokens.py`, and `python -m tools.guardian ui-audit`, then regenerates symbols, the deterministic knowledge index, and bundled rules and fails on working-tree drift. The index does not embed timestamps or host microbenchmark samples; use `python -m tools.guardian bench-host` for a fresh host-only report. PRs #2 and #3 have completed CI validation; this is evidence for those commits only, not for unbuilt changes or physical hardware. A local package-index download failure is an environment limitation and must include the exact command and output.
