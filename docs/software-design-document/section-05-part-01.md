# SDD Section 05: Boot, Tasks, and Main State Machine

[Back to the SDD index](../software-design-document.md)

## 5.1 Firmware entry points

`BmoGameboy.ino` owns global launcher state, `setup()`, and `loop()`. Core state and display helpers live in `src/core/`; each emulator wrapper has its own header/source pair in `src/emulators/`. The main loop is state-based rather than an RTOS task per emulator.

## 5.2 Boot order

The current `setup()` performs these operations in source order:

1. Start serial at 115200 baud and wait 3 seconds in a debug build or 500 ms otherwise.
2. Log feature flags and initialize `SpiArbiter` and the global SPI instance using the configured TFT SCK, SD MISO, and TFT MOSI pins.
3. If `ENABLE_UNIT_TESTS` is defined, run the firmware unit tests and remain in test mode.
4. Initialize buttons, display, battery module, and BMO face.
5. Set the initial face expression and boot-splash timer.
6. Initialize SD catalog. If mounting succeeds, initialize `SaveManager`; if it fails, log the failure and continue without the SD library.
7. Size visible-ROM arrays to the available catalog pool, report PSRAM free/largest block and pool capacity, and report free IRAM.
8. Initialize the one-second FPS reporting clock.

The called `begin()` methods may perform their own allocation or bus setup. See those implementations before changing initialization order. `FEATURE_BATTERY_MONITOR=0` makes `Battery::begin()`/`update()` safe with no divider; no GPIO1 read is permitted. There is no FRAM probe in this boot sequence.

## 5.3 Display task and shared bus

`DisplayEmu::begin()` initializes the recursive SPI arbiter, allocates two 320×240×2-byte PSRAM staging buffers, creates a queue of depth one and a completion semaphore, then starts `BMO_Display` pinned to Core 0. The Arduino main loop performs launcher/core work and normally runs on the other application core. The display task waits for commands and writes the panel through the same `SpiArbiter` used by SD.

Most full-frame core wrappers call a `stream*Frame()` API. The async path waits for the prior transfer, copies the current frame into a PSRAM staging buffer, queues one display command, and rotates buffers. NES and DOOM use indexed source pixels and convert rows through dedicated buffers on the display task. Game Boy uses a direct scanline path (`startFrame`/`streamPixelRow`/`endFrame`) and holds the bus transaction across the frame. Fallback paths may draw synchronously if worker resources are unavailable.

`DisplayEmu::waitForDisplay()` is required before freeing or reusing a buffer that may still be read by the display task. SPI access also requires balanced acquire/release operations; a complete timeout and early-return audit is tracked in P1-06.

## 5.4 State enum

The eight `SystemState` values declared in `BmoGameboy.ino` are:

| State | Responsibility |
| :--- | :--- |
| `STATE_BOOT_SPLASH` | Startup splash and initial input/timer transition |
| `STATE_CONSOLE_MENU` | 16-entry carousel: Favorites plus 15 console types |
| `STATE_CONSOLE_MUSEUM` | Information modal for the selected console |
| `STATE_GAME_MENU` | ROM selection, favorites and title filters |
| `STATE_EMULATOR` | Active core input, update, frame output and pacing |
| `STATE_PAUSE_MENU` | Save/load, slot selection, battery save, screenshot and quit |
| `STATE_DIAGNOSTICS` | Memory/stack readouts, input display, SPI comparison, SD rescan |
| `STATE_IDLE_MASCOT` | Idle face animation and interaction |

The main lifecycle is:

```mermaid
stateDiagram-v2
    [*] --> STATE_BOOT_SPLASH
    STATE_BOOT_SPLASH --> STATE_CONSOLE_MENU
    STATE_CONSOLE_MENU --> STATE_GAME_MENU
    STATE_CONSOLE_MENU --> STATE_CONSOLE_MUSEUM
    STATE_CONSOLE_MENU --> STATE_DIAGNOSTICS
    STATE_CONSOLE_MENU --> STATE_IDLE_MASCOT
    STATE_GAME_MENU --> STATE_EMULATOR
    STATE_GAME_MENU --> STATE_GAME_MENU : invalid selection, ROM load or wrapper begin failure
    STATE_GAME_MENU --> STATE_CONSOLE_MENU
    STATE_EMULATOR --> STATE_PAUSE_MENU
    STATE_PAUSE_MENU --> STATE_EMULATOR
    STATE_PAUSE_MENU --> STATE_CONSOLE_MENU
    STATE_PAUSE_MENU --> STATE_PAUSE_MENU : battery save failure; retry or explicit quit-anyway choice
    STATE_CONSOLE_MUSEUM --> STATE_CONSOLE_MENU
    STATE_DIAGNOSTICS --> STATE_CONSOLE_MENU
    STATE_IDLE_MASCOT --> STATE_CONSOLE_MENU
```

For controls and state-specific details, see [Section 05.5](section-05-part-02.md) and [Section 05.6](section-05-part-03.md).
