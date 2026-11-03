# Standard Task Protocol
Any agent (strong or weak model) starting ANY task in this repo should
follow this sequence. This exists so behavior doesn't depend on how
capable or careful the specific model is.

## Before starting
1. Read 00_hard_stops.md, 01_hardware.md in full — always, every task,
   regardless of how unrelated the task seems, since a task that looks
   pure-software can still touch a pin or a feature flag.
2. Read 27_codebase_map.md — this gives the full architecture, routing,
   PSRAM budget, SPI sharing, and bitmask layout in one place. Knowing
   this prevents the most common structural mistakes without reading 5 files.
3. Skim 30_common_agent_mistakes.md — 2 minutes, saves hours. If your task
   touches a category listed there, re-read the linked rule file in full.
4. Read 10_symbol_reference.md and confirm any function/struct/macro
   you're about to reference actually appears there. If it doesn't, grep
   for it yourself before using it in a claim — never assume a name
   exists because it "sounds right" for the codebase.
5. `git status`; checkpoint-commit if dirty.
6. State your plan in 3-6 bullets before writing code. If the task
   touches anything in 00_hard_stops.md's categories, say so explicitly
   and state which constraint applies.

## During
5. Prefer clean, minimal changes that solve the root cause.
6. **Proactive Defect Resolution:** If you discover an existing bug, memory safety hazard, stack overflow risk, or documentation drift while inspecting files for your task, fix it and verify it rather than walking past it, and document every fix in `CHANGELOG.md` and `04_known_issues.md`.
7. If you're about to fabricate a plausible-sounding detail because you
   don't actually know it (a line number, a variable name, a test
   result) — stop. State the uncertainty or verify it with a tool call instead.
8. **Stop-after-2-failures rule:** if the same interaction/approach fails
   twice in a row (a build flag, a test invocation, a specific fix
   attempt), STOP retrying variations of the same approach. Switch
   strategy entirely, or stop and ask.

## Before reporting done
9. Compile and run `python scripts/validate_repo.py` and `python -m tools.guardian diff-audit`. A successful compile and passing validator are required before reporting a code change complete.
10. Update `.agents/rules/` files per the Documentation Maintenance Protocol in `03_conventions.md`, using the status vocabulary from `06_verification_standards.md`.
11. Update `04_known_issues.md` and `CHANGELOG.md` with dated entries when firmware or tooling changes are made.

## Reporting verification clearly
Always clearly distinguish between:
- **Verified in this session on host:** commands, compiles, unit tests, and native C harness runs you executed in this session.
- **Requires physical hardware verification:** behavior that can only be confirmed after flashing onto the physical ESP32-S3 board.

## Definition of Done, by task type
- **Docs-only change:** compiles N/A; diff reviewed; cross-references
  checked (grep for old section numbers/filenames before renaming).
- **Bug fix, no hardware-timing implication:** compiles; host-side test
  covering the fix passes to full completion; known_issues updated.
- **Change to CPU core / memory access / ISR / timing-critical code:**
  compiles; host-side test passes to full completion; hardware handoff
  checklist prepared; known_issues stays `FIXED_UNVERIFIED` or
  `VERIFIED_HOST` until a human reports back `VERIFIED_HARDWARE`.
- **New hardware peripheral integration:** isolated standalone `.ino` in
  tools/ verified first (03_conventions.md rule already requires this);
  01_hardware.md and the pin map updated ONLY after physical soldering is
  confirmed by the human, never in anticipation of it.
- **New UI screen/menu:** follows 08_ui_style_guide.md; no new raw hex
  colors outside the shared theme constants file.
- **Change touching src/assets/ or adding a vendor library:** flash-budget invariant in `37_rom_governance_and_flash_budget.md` verified this session.
- **Rules-directory / process change:** diff reviewed; every new file under 12,000 chars; README index and RULESET_VERSION updated; changelog entries added; no hard-stop or hardware-ground-truth text restated elsewhere.
