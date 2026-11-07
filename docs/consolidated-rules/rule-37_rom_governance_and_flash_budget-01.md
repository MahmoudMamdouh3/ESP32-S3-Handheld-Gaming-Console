# Section 37 — ROM Governance & Flash-Budget Invariant

Source: [`37_rom_governance_and_flash_budget.md`](../../.agents/rules/37_rom_governance_and_flash_budget.md).

# ROM Governance & Flash-Budget Invariant
Purpose: resolve the standing ambiguity between `docs/hardware-notes.md`
§11 ("never commit a copyrighted commercial ROM... .gitignore excludes
ROM files and their generated headers") and `27_codebase_map.md` (which
shows registered baked ROMs the unconditional build depends on). Also
promotes the flash-budget check from a one-time manual checklist
(`29_adding_a_baked_rom.md`) to a standing invariant.

## Tracking status (verified ground truth)
Audit command executed in session:
```powershell
git ls-files firmware/BmoGameboy/src/assets/roms/
```
Literal output:
```text
firmware/BmoGameboy/src/assets/roms/aladdin.h
firmware/BmoGameboy/src/assets/roms/lego_racers.h
firmware/BmoGameboy/src/assets/roms/mario_deluxe.h
firmware/BmoGameboy/src/assets/roms/zelda_ages.h
```

**Finding:** All four ROM headers under `src/assets/roms/` are currently
tracked and committed in git history. The root `.gitignore` specifies
patterns `rom_*.h` and `rom_data*.h`, which do not match `aladdin.h`,
`lego_racers.h`, `mario_deluxe.h`, or `zelda_ages.h`.

## Governance & Legality Policy
- **Direct Policy Conflict:** The presence of commercial ROM C-arrays in git
  directly conflicts with `docs/hardware-notes.md` §11 ("never commit a
  copyrighted commercial ROM").
- **Standing Status:** Tracked as an `OPEN` technical debt item in
  `04_known_issues.md` for human operator resolution (licensing and
  distribution decisions are outside autonomous agent authority).
- In the interim, firmware builds compile against these four tracked headers
  (`mario_deluxe.h`, `zelda_ages.h`, `aladdin.h`, `lego_racers.h`).

## Flash-budget invariant (standing rule)
Any change that adds to `app0`'s compiled content — a new baked ROM, a new
large const table, or a new library — MUST satisfy:
$$\text{new\_total\_binary\_size\_bytes} < 8,388,608 \text{ bytes (app0 partition capacity)}$$
Verified by an ACTUAL `arduino-cli compile` run in the current session, not an estimate.

This check is a mandatory part of Definition of Done (`07_task_protocol.md`)
for ANY change touching `src/assets/` or adding vendor libraries.

## Partition-change protocol (repartitioning app0/ffat)
The `ffat` partition (~7.9MB) is currently unused. Growing `app0` at its
expense is possible in principle but is a HARD-STOP-ADJACENT change:
1. First grep the entire firmware for any `FFat.` / `SPIFFS.` / filesystem
   mount call to confirm `ffat` is truly unused before touching it.
2. A `partitions.csv` change requires the review gate in `05_git_workflow.md`
   (show `git diff --stat` + message, wait for explicit human approval)
   BEFORE committing, every time, no exceptions.
3. Never repartition speculatively "in case it's needed" — only when a
   specific, currently-blocked ROM registration is the stated reason,
   documented in the same commit.
4. After any partition change, re-run the FULL flash-budget verification
   above, plus confirm OTA metadata (`otadata`) still fits its 8KB
   allocation unchanged.