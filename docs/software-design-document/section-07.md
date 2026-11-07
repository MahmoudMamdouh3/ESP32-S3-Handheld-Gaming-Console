# SDD Section 07: Display, Color, and SPI

[Back to the SDD index](../software-design-document.md)

## 7.1 Panel configuration and published timing

[`DisplayEmu::begin()`](../../firmware/BmoGameboy/src/core/display_emu.cpp) initializes the ST7789 with `TFT_WIDTH=240`, `TFT_HEIGHT=320`, rotation 3, the configured display SPI clock, and a manual MADCTL command/data write composed from the MY, MV, and BGR bits. The display is used as a 320×240 landscape viewport. The MADCTL setup path writes at 8 MHz during initialization. Panel color and rotation are source-defined; the physical view has not been calibrated in this audit.

The display and SD share SPI clock GPIO12 and MOSI GPIO11. Their transfers are serialized by `SpiArbiter`. The ST7789VW v1.0 datasheet gives a 16 ns serial write cycle, equivalent to 62.5 MHz; the configured 80 MHz has a 12.5 ns period and exceeds that published timing. This comparison assumes the fitted module uses an ST7789VW with the cited datasheet timing. The module marking and board behavior have not been measured. This is an open physical timing risk, not evidence that a specific panel fails. See [the datasheet](https://dl.espressif.com/dl/schematics/ST7789VW_datasheet.pdf) and [hardware test procedure](../hardware_measurements.md).

**Pinned-core source check (Arduino-ESP32 3.3.11):** for ESP32-S3, `getApbFrequency()` returns `APB_CLK_FREQ`; `spiFrequencyToClockDiv()` routes non-P4 targets through `_spiFrequencyToClockDivWithSource(freq, getApbFrequency())`. The target's APB source is 80 MHz, and the divider uses integer `(clkdiv_pre + 1) * (clkcnt_n + 1)` factors, selecting a clock no greater than the request. Under this source model, a 40 MHz request produces the next supported rate below 62.5 MHz. The 80 MHz request equals the source clock. `spiAttachMISO()` routes GPIO15 through `pinMatrixInAttach()`; the checked HAL has no additional software frequency cap for matrix-routed MISO. These are core-source findings, not an electrical measurement. Signal integrity, module limits, and the actual SD/display clock remain **NEEDS-HARDWARE**; selecting or changing the clock policy remains **NEEDS-USER**. See [ADR 0014](../decisions/0014-display-spi-clock.md).

## 7.2 Display worker and frame paths

- Full-frame buffers and async staging are allocated in PSRAM. `BMO_Display` is pinned to Core 0 and receives commands through a one-entry FreeRTOS queue.
- The main task waits for the prior display command before rotating staging buffers, copies frame data, and queues the next display command.
- Game Boy uses the direct row-stream path, scaling 160×144 content to a 240×216 viewport at offset (40,12).
- NES and DOOM submit indexed source frames; the display task converts indexed pixels to the firmware palette before writing rows.
- Other wrappers generally submit RGB565 frames through `streamRawFrameAsync()` with per-system viewports below.
- If async resources are unavailable, the display implementation falls back to synchronous SPI transfers.
- Before freeing/reusing a frame buffer, wait for outstanding display work. Never bypass bus arbitration.

## 7.3 Configured frame viewports

The following table records `setAddrWindow()` geometry in the current stream methods. It is source configuration, not a measurement of the active pixels on the physical panel.

| Wrapper index | Console/core | Viewport W×H | Offset X,Y | Source frame path |
| ---: | :--- | ---: | ---: | :--- |
| 0 | GBC / Walnut | 240×216 | 40,12 | Scaled RGB565 |
| 1 | GB / Peanut | 240×216 | 40,12 | RGB565 |
| 2 | NES | 256×240 | 32,0 | Indexed 8-bit, palette expanded |
| 3 | DOOM | 320×200 | 0,20 | Indexed 8-bit, palette expanded |
| 4 | Master System | 256×192 | 32,24 | RGB565 |
| 5 | Game Gear | 160×144 | 80,48 | Crop from 256×192 source |
| 6 | PC Engine | 256×240 | 32,0 | RGB565 |
| 7 | Atari 2600 | 160×192 | 80,24 | RGB565 |
| 8 | PICO-8 | 128×128 | 96,56 | RGB565 |
| 9 | Genesis | 320×224 | 0,8 | RGB565 |
| 10 | SNES | 256×224 | 32,8 | RGB565 |
| 11 | WonderSwan | 224×144 | 48,48 | RGB565 |
| 12 | Neo Geo Pocket | 160×152 | 80,44 | RGB565 |
| 13 | Atari Lynx | 160×102 | 80,69 | RGB565 |
| 14 | ColecoVision / SG-1000 | 256×192 | 32,24 | RGB565 |

The ideal raw serial wire time can be estimated from `bytes × 8 / configured_clock_hz`. This excludes commands, chip-select gaps, task scheduling, conversion, and controller timing limits; it is a model only. Do not use it as measured FPS.

## 7.4 Pixel word formats and color tokens

[`theme.h`](../../firmware/BmoGameboy/src/core/theme.h) defines RGB input colors, BGR565 conversion, and separate helpers: `makeUiColor()` for byte-swapped canvas-buffer words and `makePanelColor()` for direct panel calls. The theme tokens are mirrored in [`DESIGN.md`](../../DESIGN.md) and browser CSS. `python scripts/verify_color_tokens.py` checks the current source token values across these files.

| Firmware token | RGB value | UI canvas word | Direct panel word | CSS token |
| :--- | :--- | ---: | ---: | :--- |
| `BMO_SCREEN_MINT` | `#CEF5E4` | `0xB9E7` | `0xE7B9` | `--screen-mint` |
| `BMO_BODY_TEAL` | `#5FB49C` | `0xAB9D` | `0x9DAB` | `--chassis-color` |
| `BMO_DEEP_TEAL` | `#1A4B42` | `0x4342` | `0x4243` | `--bezel-color` |
| `BMO_DARK_FOREST` | `#0F2620` | `0x2121` | `0x2121` | `--bezel-lens` |
| `BMO_DPAD_YELLOW` | `#FFE033` | `0x1F37` | `0x371F` | `--dpad-color` |
| `BMO_CORAL_RED` | `#FF91A5` | `0x9FA4` | `0xA49F` | `--btn-b-color` |
| `BMO_BLUE_BUTTON` | `#52A6BA` | `0x2ABD` | `0xBD2A` | `--btn-a-color` |
| `BMO_WHITE` | `#F8FDFA` | `0xFFFF` | `0xFFFF` | `--text-main` |
| `BMO_MOUTH_GREEN` | `#4CAF50` | `0x6955` | `0x5569` | `--btn-x-color` |
| `BMO_TONGUE_PINK` | `#F48FB1` | `0x7EB4` | `0xB47E` | `--btn-y-color` |

Coral `#FF91A5` against deep teal `#1A4B42` has a calculated 4.6:1 contrast ratio; dark-forest text on the coral fill has 7.5:1. Firmware prompt pills and the browser B button use dark text on that fill. These are source RGB calculations, not a physical-panel measurement. The calculation and placement are covered by `DESIGN.md` and Rule 44.

## 7.5 Display ownership

`DisplayEmu` owns the ST7789 object, asynchronous command queue, staging buffers, menu canvas, framebuffer format/viewport functions, UI drawing routines, and diagnostics SPI comparison. Emulator wrappers must call the correct `stream*Frame()` method and use the matching resolution/pixel format. See [generated symbols](../generated/symbols.md) for declarations and [Section 09](section-09-part-01.md) for per-core routing.
