# Repository Guide — Part 2: Systems, controls, and build

[Back to the repository overview](../../README.md)

## Platform selections and wrappers

The launcher has Favorites plus fifteen platform selections. SMS and Game Gear share a wrapper, so the firmware has fourteen wrapper implementations. File extensions are scanner selectors; they do not imply complete compatibility with every cartridge feature or image variant.

| Selection | ROM extensions | Wrapper |
| :--- | :--- | :--- |
| Game Boy DMG | `.gb` | Peanut-GB |
| Game Boy Color | `.gbc` | Walnut-CGB |
| NES | `.nes` | Agnes |
| DOOM | `.wad` | doomgeneric |
| Master System / Game Gear | `.sms`, `.gg` | SMS Plus |
| PC Engine | `.pce` | PCE |
| Atari 2600 | `.a26` | Atari wrapper |
| PICO-8-style runtime | `.p8` | Custom text-cart runtime; does not execute official Lua carts |
| Genesis | `.gen`, `.md`, `.smd` | Genesis |
| SNES | `.sfc`, `.smc` | SNES |
| WonderSwan | `.ws`, `.wsc` | WonderSwan |
| Neo Geo Pocket | `.ngp`, `.ngc` | NGP |
| Atari Lynx | `.lnx` | Lynx |
| ColecoVision / SG-1000 | `.col`, `.sg` | Coleco |

The [SDD emulator chapter](../software-design-document/section-09-part-01.md) links each wrapper to its source and describes the host and compatibility limits. No per-console conformance suite is currently recorded.

## SD card and saves

The scanner walks the SD root and subdirectories to its configured depth. The binary catalog is `/.bmo_index`; favorites and recently played metadata are stored in `/favorites.txt` and `/history.txt`. Save files, save states, and screenshots are under their dedicated `/saves/` and `/screenshots/` folders and use a key derived from the relative ROM path. Box art can be placed under `/boxart/` or `/covers/`.

`python scripts/deploy_to_sd.py --destination F:\` prints a dry-run plan. Applying writes requires `--apply` and a matching volume label. The deployment script stages and verifies copies and does not delete card data. Check `--help` before using it with a drive.

## In-game controls

- `SELECT + START`: open the pause menu.
- `SELECT + A` / `SELECT + B`: quick-save / quick-load the active slot.
- `SELECT + DOWN`: cycle the DMG palette on the GB/GBC wrappers.
- `SELECT + LEFT`: toggle A/B turbo.
- Hold `SELECT + RIGHT`: run without the outer pacing delay.
- `SELECT + UP`: open the pause menu with Quit selected. Press A twice within three seconds to confirm. B or Start resumes; other directional or Select input cancels the confirmation.
- In the pause menu, Up/Down changes the selection and A activates it. The quit row uses the same two-press confirmation. Left toggles A/B swap, Right changes scaling, and Select requests a screenshot.

Battery RAM is saved only for supported cartridge formats. A game with no supported battery region reports “NO BATTERY SAVE.” Save states are separate from battery RAM and have host-side format checks; full device power-loss coverage is still open.

## Build and verification

Requirements: Arduino CLI, ESP32 core `3.3.11` as pinned by CI and `firmware/BmoGameboy/sketch.yaml`, Adafruit GFX, and Adafruit ST7735/ST7789. The board target is ESP32-S3-N16R8 with QIO flash, OPI PSRAM, and the repository's custom partition table.

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi" firmware/BmoGameboy
python -m unittest discover -s tests -v
python scripts/validate_repo.py
python -m tools.guardian verify
```

The repository validator has phases 0–8. Phase 0 warns and skips target compile when Arduino CLI is missing in advisory mode; strict mode fails instead. A skipped build cannot be reported as a compile pass. See [Section 14](../software-design-document/section-14.md).

## License and game content

The project owner will provide a separate ROM policy and license direction. This page does not grant rights to ROMs, game art, or other third-party files. Do not add commercial ROM or artwork files to the repository.
