# GB-Emu-ESP32S3

A Game Boy emulator handheld built from scratch on an ESP32-S3, from
bare hardware bring-up through to running actual Game Boy homebrew
ROMs. Documenting the whole build: wiring, debugging, and firmware.

## Hardware

- ESP32-S3-N16R8 (16MB flash, 8MB octal PSRAM)
- 2.4" TFT SPI display, 240x320, ST7789 driver
- 8x tactile push buttons (Game Boy D-pad + A/B/Start/Select layout)
- microSD card module (planned - ROM storage)
- No audio hardware (video/gameplay only for now)

## Status

- [x] Milestone 1: Display + button bring-up, with live visual
      feedback on-screen
- [ ] Milestone 2: microSD card integration
- [x] Milestone 3: Game Boy emulator core (Peanut-GB) running a
      homebrew ROM

## Repo structure

```
firmware/
  01_display_button_test/   Arduino sketch for the current milestone
docs/
  wiring/                   Wiring reference per milestone
```

## Firmware architecture

Each milestone's firmware is modular:
- `config.h` - single source of truth for all pin wiring
- `buttons.h/.cpp` - button reading, debouncing-free polled state
- `display_emu.h/.cpp` - all screen drawing for that milestone
- the `.ino` file only wires modules together in `setup()`/`loop()`

## Building

1. Install Arduino IDE + ESP32 board support (esp32 by Espressif
   Systems) via Boards Manager.
2. Install libraries: "Adafruit ST7789", "Adafruit GFX Library".
3. Board: "ESP32S3 Dev Module". Flash Size: 16MB. PSRAM: OPI PSRAM.
4. Open the `.ino` inside the relevant `firmware/<milestone>/` folder
   and upload.

See `docs/wiring/` for the wiring diagram matching each milestone.

## License

MIT - see [LICENSE](LICENSE).
