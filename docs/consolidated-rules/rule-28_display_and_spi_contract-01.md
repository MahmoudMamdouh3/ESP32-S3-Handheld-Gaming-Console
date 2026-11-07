# Section 28 — 28. Display and SPI Contract

Source: [`28_display_and_spi_contract.md`](../../.agents/rules/28_display_and_spi_contract.md).

# 28. Display and SPI Contract

## Hardware facts and configured values

config.h assigns ST7789 pins MOSI=11, SCK=12, CS=10, DC=8, reset=9. SD shares MOSI/SCK and uses MISO=15, CS=13. Display frequency is configured to 80 MHz; Diagnostics can compare with 40 MHz. SD is configured to 25 MHz. These are software requests, not measured board limits. The ST7789VW datasheet specifies a 16 ns serial write cycle (62.5 MHz equivalent), so the configured 80 MHz exceeds that published timing. Physical panel behavior has not been measured.

The TFT uses BGR mode. UI canvas values and direct panel values use different byte order helpers in theme.h. Use the matching makeUiColor() or makePanelColor() path; do not change MADCTL to compensate for an isolated palette error. python scripts/verify_color_tokens.py checks the current semantic token mapping.

## Frame ownership

- The display task is BMO_Display, pinned to Core 0. It receives queued display work and uses the shared bus arbiter.
- GB/GBC startFrame() waits for queued display work, locks the SPI arbiter, sets a GB viewport, and opens a synchronous scanline path. Every successful call must pair with endFrame().
- Complete-frame APIs enqueue/copy into staging buffers where the asynchronous path is available. Wait for completion before freeing or reusing referenced memory.
- NES/DOOM indexed paths and all other wrapper geometry are documented in SDD Section 07. Match the source pixel format and viewport.
- SD operations must not access the shared bus concurrently. Acquire/release the arbiter through the existing service contract and check all early returns.

Do not claim 60 FPS or measured transfer overlap from this architecture alone. The current serial telemetry reports pre-pacing wrapper/core execution; it does not isolate SPI or panel scan-out.

## Extending the display API

1. State dimensions, pixel encoding, and buffer ownership in the declaration.
2. Keep each start/end frame pair balanced, including failure paths.
3. Synchronize with outstanding display work before releasing or mutating submitted buffers.
4. Preserve SpiArbiter ownership and the configured physical wiring.
5. Run the UI audit, target build, and affected host checks. Physical clock and color behavior require an on-device record.