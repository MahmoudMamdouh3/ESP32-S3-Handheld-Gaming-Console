# Contributing

Contributions to firmware, host tests, enclosure CAD, and the browser simulator are welcome.

## Setup

Use [`tasks.py`](tasks.py) as the unified cross-platform developer entry point (see [docs/build-and-ci.md](docs/build-and-ci.md) for profile details and CI documentation):

```powershell
# 1. Configure Git hooks, check Python deps, and install pinned ESP32 core/libs from sketch.yaml:
python tasks.py setup

# 2. Run fast local validation (< 1 min, no firmware compile):
python tasks.py check

# 3. Run full host test suites + repository validator:
python tasks.py test

# 4. Compile firmware (default profile + strict first-party warnings):
python tasks.py build
python tasks.py size
```

Equivalent direct commands:

```powershell
python scripts/install_git_hooks.py
python -m pip install -r requirements.txt
python -m unittest discover -s tests -v
python -m unittest discover -s tools/guardian/tests -v
.\arduino-cli.exe compile --profile esp32s3 firmware/BmoGameboy
python scripts/validate_repo.py
```

## Guidelines

- **No commercial content:** Do not commit copyrighted ROMs, BIOS images, box-art scrapes, or downloader scripts.
- **No large binary blobs:** Staged files must remain below 1 MiB (`.githooks/pre-commit` enforces `< 1,048,576` bytes and scans for credential patterns).
- **Preserve physical pin & safety contracts:** Do not enable `FEATURE_BATTERY_MONITOR` or `FEATURE_AUDIO`, alter pin assignments in `firmware/BmoGameboy/src/hal/config.h`, or use `FlashMode=opi`. See [`AGENTS.md`](AGENTS.md) and [`docs/hardware.md`](docs/hardware.md).
- **Evidence accuracy:** Report verification using `NOT_TESTED`, `HOST_TESTED`, `TARGET_COMPILED`, or `HARDWARE_TESTED`. Never present simulator or analytical numbers as physical hardware results.
- **Licensing:** Combined default firmware binaries are licensed under `GPL-2.0-or-later` (`LICENSE`) because they link upstream third-party emulator cores in [`firmware/BmoGameboy/src/third_party/`](THIRD_PARTY_NOTICES.md) (compiling with `-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0` selects the permissive MIT GB/GBC/NES core subset; first-party host/utility code remains permissive MIT). Any PR affecting `src/third_party/` or third-party build flags must update [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and the corresponding core `PROVENANCE.md`.
