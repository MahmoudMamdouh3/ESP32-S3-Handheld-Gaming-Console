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
- **Licensing:** Original first-party code is licensed under `MIT OR GPL-2.0-or-later`. The `mit` firmware build profile (`python tasks.py build --profile mit`, `-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0`) links only permissive MIT emulator cores (`Peanut-GB`, `Walnut-CGB`, `agnes`) and is the only profile distributed as a precompiled binary. The `default` and `no-doom` source builds (`BMO_EXPERIMENTAL_CORES=1`) link upstream `GPL-2.0`, `GPL-3.0` (`gwenesis`), and non-commercial (`snes9x`, MAME `z80.c`, Musashi v3.32) cores in [`firmware/BmoGameboy/src/third_party/`](THIRD_PARTY_NOTICES.md) and are for personal local compilation only (do not redistribute combined `default`/`no-doom` binaries). Any PR affecting `src/third_party/` or third-party build flags must update [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and the corresponding core `PROVENANCE.md`.
