# SDD Section 09: Emulator wrappers and engine boundaries (part 1)

[Back to the SDD index](../software-design-document.md)

## 9.1 Runtime ownership

`CONSOLES[]` index includes Favorites at index 0, followed by fifteen platform selections. Those fifteen map to fourteen C++ wrappers because SMS and Game Gear share `SmsEmu`. The mapping below joins the source `RomType`, carousel index, launch/destroy/frame wrapper index, scanner extensions, configured viewport, and nominal host-loop pacing. Accepted extensions do not guarantee that every image is compatible.

| `RomType` | `CONSOLES[]` | Wrapper index | Wrapper source | Scanner extensions | Viewport | Target period |
| :--- | ---: | ---: | :--- | :--- | :--- | ---: |
| `ROM_GB` | 1 | 1 | `src/emulators/emu_peanut.cpp` | `.gb` | 240×216 @(40,12) | 16,742 µs |
| `ROM_GBC` | 2 | 0 | `src/emulators/emu_walnut.cpp` | `.gbc` | 240×216 @(40,12) | 16,742 µs |
| `ROM_NES` | 3 | 2 | `src/emulators/emu_nes.cpp` | `.nes` | 256×240 @(32,0) | 16,667 µs |
| `ROM_WAD` | 4 | 3 | `src/emulators/emu_doom.cpp` | `.wad` | 320×200 @(0,20) | 28,571 µs |
| `ROM_SMS` | 5 | 4 | `src/emulators/emu_sms.cpp` | `.sms` | 256×192 @(32,24) | 16,667 µs |
| `ROM_GG` | 6 | 5 | `src/emulators/emu_sms.cpp` | `.gg` | 160×144 @(80,48) | 16,667 µs |
| `ROM_PCE` | 7 | 6 | `src/emulators/emu_pce.cpp` | `.pce` | 256×240 @(32,0) | 16,667 µs |
| `ROM_ATARI` | 8 | 7 | `src/emulators/emu_atari.cpp` | `.a26` | 160×192 @(80,24) | 16,667 µs |
| `ROM_PICO8` | 9 | 8 | `src/emulators/emu_pico.cpp` | `.p8` | 128×128 @(96,56) | 33,333 µs |
| `ROM_GENESIS` | 10 | 9 | `src/emulators/emu_genesis.cpp` | `.gen`, `.md`, `.smd` | 320×224 @(0,8) | 16,667 µs |
| `ROM_SNES` | 11 | 10 | `src/emulators/emu_snes.cpp` | `.sfc`, `.smc` | 256×224 @(32,8) | 16,667 µs |
| `ROM_WSWAN` | 12 | 11 | `src/emulators/emu_wswan.cpp` | `.ws`, `.wsc` | 224×144 @(48,48) | 16,667 µs |
| `ROM_NGP` | 13 | 12 | `src/emulators/emu_ngp.cpp` | `.ngp`, `.ngc` | 160×152 @(80,44) | 16,667 µs |
| `ROM_LYNX` | 14 | 13 | `src/emulators/emu_lynx.cpp` | `.lnx` | 160×102 @(80,69) | 16,667 µs |
| `ROM_COLEM` | 15 | 14 | `src/emulators/emu_colem.cpp` | `.col`, `.sg` | 256×192 @(32,24) | 16,667 µs |

The carousel has 16 entries including Favorites; source launch, destroy, and frame dispatch have 15 wrapper indices; there are 14 wrapper source files for these routes. [`tests/test_platform_mapping.py`](../../tests/test_platform_mapping.py) checks this table against `CONSOLES[]`, `BmoGameboy.ino` dispatch, `frame_pacing.h`, Section 7.3, and `AGENT_MANIFEST.json`. The table records the fixed viewport request; variable core output geometry and compatibility need separate validation.

The source files listed under `src/vendor/` and `src/engine/` are the engine implementations. The wrapper owns hardware-facing input translation, frame submission, memory setup, and calls into `SaveManager`; exact declarations are in [the generated symbol index](../generated/symbols.md). Do not infer CPU-test coverage, cycle accuracy, game compatibility, or complete hardware behavior from the existence of an adapter.

## 9.2 Frame submission

The wrapper selects a display method that matches the output format. Current viewport geometry and source frame methods are listed in [Section 07](section-07.md). Game Boy and Game Boy Color render through scanline callbacks while `DisplayEmu::startFrame()` holds the shared SPI transaction. Other wrappers submit indexed or RGB565 frames through `DisplayEmu`. This split matters for bus ownership: a wrapper that opens a frame transaction must always close it before returning from the frame path.

## 9.3 Input and pacing

The main loop samples the common button service and adapters translate it to each engine's input representation. `BmoGameboy.ino` owns SELECT-modified launcher hotkeys and pause routing. `FramePacing::targetFrameUs()` supplies nominal loop delays; the [canonical pacing table and host test](section-15.md#151-source-configured-frame-targets) describe the configured indices and limits.
