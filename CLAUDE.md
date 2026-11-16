# Claude Code Instructions

Canonical rules, hardware hard stops, AI anti-regression policies, verification commands, and the 4-tier evidence vocabulary (`NOT_TESTED`, `HOST_TESTED`, `TARGET_COMPILED`, `HARDWARE_TESTED`) live in [`AGENTS.md`](AGENTS.md).

- Start by reading [`docs/ai/STATE.md`](docs/ai/STATE.md) and [`docs/ai/CODEMAP.md`](docs/ai/CODEMAP.md).
- For C/C++ firmware changes, also follow [`firmware/AGENTS.md`](firmware/AGENTS.md).
- Run `python scripts/install_git_hooks.py` (`core.hooksPath .githooks`) and verify edits with `python tasks.py check` + `python scripts/sync_ai_codemap.py --check`.
- Never flash hardware, alter `games/`, `roms/`, `sdcard/`, `.rom_cache/`, `Book/`, or fabricate runtime metrics.
