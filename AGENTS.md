# Agent Instructions (Ruleset v8)
This repo is agent-developed firmware for a physical, soldered ESP32-S3
handheld console. Mistakes here can brick real hardware, not just fail a build.

**Quick On-Ramp (90 seconds):**
1. Read [`.agents/rules/31_quick_start_primer.md`](file:///e:/BMO%20Gameboy/.agents/rules/31_quick_start_primer.md), [`.agents/rules/00_hard_stops.md`](file:///e:/BMO%20Gameboy/.agents/rules/00_hard_stops.md), and [`.agents/rules/01_hardware.md`](file:///e:/BMO%20Gameboy/.agents/rules/01_hardware.md).
2. Run `python -m tools.guardian prompt-pack "<your task>"` to generate a self-contained context packet with exact target files, live C/C++ signatures, pin mappings, and verification commands.

**Full Ruleset:**
See [`.agents/rules/README.md`](file:///e:/BMO%20Gameboy/.agents/rules/README.md) for task-specific rules, or query [`AGENT_MANIFEST.json`](file:///e:/BMO%20Gameboy/AGENT_MANIFEST.json) and [`.agents/rules/CONTEXT_INDEX.json`](file:///e:/BMO%20Gameboy/.agents/rules/CONTEXT_INDEX.json).

**The core habits that matter most in this repo:**
1. Never state a code fact, test result, or symbol name you haven't verified in THIS session — see [`.agents/rules/06_verification_standards.md`](file:///e:/BMO%20Gameboy/.agents/rules/06_verification_standards.md).
2. Physical hardware state ([`.agents/rules/01_hardware.md`](file:///e:/BMO%20Gameboy/.agents/rules/01_hardware.md) & [`config.h`](file:///e:/BMO%20Gameboy/firmware/BmoGameboy/src/core/config.h)) is the ground truth.
3. Every public symbol and API must match [`.agents/rules/10_symbol_reference.md`](file:///e:/BMO%20Gameboy/.agents/rules/10_symbol_reference.md).
4. Audit your uncommitted changes with `python -m tools.guardian diff-audit` and run the full CI validator before concluding: `python scripts/validate_repo.py`.
5. Leave clear logs in `04_known_issues.md` and `CHANGELOG.md` following [`.agents/rules/33_agent_handoff_and_optimization_cycle.md`](file:///e:/BMO%20Gameboy/.agents/rules/33_agent_handoff_and_optimization_cycle.md), and **always** use two-part commit messages (`git commit -m "type(scope): summary" -m "- bullet 1\n- bullet 2\n- Verified: ..."`).
