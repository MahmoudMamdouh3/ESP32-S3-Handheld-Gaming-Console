# Claude Code Instructions

Follow [`AGENTS.md`](AGENTS.md) for hardware hard stops, verified build/test commands, and the 4-tier evidence vocabulary (`NOT_TESTED`, `HOST_TESTED`, `TARGET_COMPILED`, `HARDWARE_TESTED`).

- For C/C++ firmware changes, also follow [`firmware/AGENTS.md`](firmware/AGENTS.md).
- Configure local git hooks with `python scripts/install_git_hooks.py` (`core.hooksPath .githooks`).
- Never flash hardware, alter user data directories (`games/`, `roms/`, `sdcard/`, `.rom_cache/`, `Book/`), or invent runtime measurements.
