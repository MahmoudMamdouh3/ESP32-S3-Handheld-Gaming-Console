# Contributing

Contributions to firmware, host tests, enclosure CAD, and the browser simulator are welcome.

## Setup

1. Enable the lightweight pre-commit hook:

```powershell
python scripts/install_git_hooks.py
```

2. Run the host test suite:

```powershell
python -m unittest discover -s tests -v
python -m unittest discover -s tools/guardian/tests -v
```

3. Compile firmware (requires Arduino CLI with `esp32:esp32@3.3.11`, `Adafruit BusIO`, `Adafruit GFX Library`, and `Adafruit ST7735 and ST7789 Library`):

```powershell
.\arduino-cli.exe compile --profile esp32s3 firmware/BmoGameboy
python scripts/validate_repo.py
```

## Guidelines

- **No commercial content:** Do not commit copyrighted ROMs, BIOS images, box-art scrapes, or downloader scripts.
- **No large binary blobs:** Staged files must remain below 1 MiB (`.githooks/pre-commit` enforces `< 1,048,576` bytes and scans for credential patterns).
- **Preserve physical pin & safety contracts:** Do not enable `FEATURE_BATTERY_MONITOR` or `FEATURE_AUDIO`, alter pin assignments in `firmware/BmoGameboy/src/hal/config.h`, or use `FlashMode=opi`. See [`AGENTS.md`](AGENTS.md) and [`docs/hardware.md`](docs/hardware.md).
- **Evidence accuracy:** Report verification using `NOT_TESTED`, `HOST_TESTED`, `TARGET_COMPILED`, or `HARDWARE_TESTED`. Never present simulator or analytical numbers as physical hardware results.
- **Licensing:** First-party code is MIT. Any PR affecting [`firmware/BmoGameboy/src/third_party/doom/`](firmware/BmoGameboy/src/third_party/doom/PROVENANCE.md) (`BMO_ENABLE_DOOM`) or adding third-party code must update [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
