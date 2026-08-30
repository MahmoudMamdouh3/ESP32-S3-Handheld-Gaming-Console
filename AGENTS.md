# Agent Instructions (Ruleset v4)
This repo is agent-developed firmware for a physical, soldered ESP32-S3
handheld console. Mistakes here can brick real hardware, not just fail a build.

**Quick On-Ramp (90 seconds):**
Read [`.agents/rules/31_quick_start_primer.md`](file:///e:/BMO%20Gameboy/.agents/rules/31_quick_start_primer.md). It summarizes the entire hardware state, hard stops, directory map, task decision table, and AI guardrails in one page.

**Full Ruleset:**
Read [`.agents/rules/README.md`](file:///e:/BMO%20Gameboy/.agents/rules/README.md), then every file it marks "Always".

**Machine-Readable Context:**
Query [`AGENT_MANIFEST.json`](file:///e:/BMO%20Gameboy/AGENT_MANIFEST.json) or [`.agents/rules/CONTEXT_INDEX.json`](file:///e:/BMO%20Gameboy/.agents/rules/CONTEXT_INDEX.json) for instant task-to-rule indexing.

**The core habits that matter most in this repo:**
1. Never state a code fact, test result, or symbol name you haven't verified in THIS session — see [`.agents/rules/06_verification_standards.md`](file:///e:/BMO%20Gameboy/.agents/rules/06_verification_standards.md).
2. Physical hardware state ([`.agents/rules/01_hardware.md`](file:///e:/BMO%20Gameboy/.agents/rules/01_hardware.md)) is the ground truth.
3. Every public symbol and API must match [`.agents/rules/10_symbol_reference.md`](file:///e:/BMO%20Gameboy/.agents/rules/10_symbol_reference.md).
4. Run the AI Guardian CI validator before concluding: `python scripts/validate_repo.py`.
5. Continuous optimization cycle: when completing a task, leave clear logs in `04_known_issues.md` and `CHANGELOG.md` following [`.agents/rules/33_agent_handoff_and_optimization_cycle.md`](file:///e:/BMO%20Gameboy/.agents/rules/33_agent_handoff_and_optimization_cycle.md) so the next agent builds upon your progress smoothly.
