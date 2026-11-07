# 38. Mandatory Documentation Update Protocol
**Purpose:** Every agent session that modifies code, APIs, or engineering decisions
MUST update the corresponding documentation before committing. This rule exists because
agents have repeatedly completed work, logged it as "done", and committed without updating
the SDD, symbol reference, or related rules — leaving stale documentation that misleads
future agents and causes re-work.

---

## The Rule: Documentation Is Part of the Task, Not Optional Cleanup

A task is **not complete** until every document listed below that is relevant to the
change has been updated. Omitting documentation updates and calling a task "done" is
equivalent to skipping a test — it violates the Definition of Done.

---

## Required Updates by Change Type

### If you added or modified any C++ public API (function, struct, macro in a `.h` file):
- **MUST update** [`10_symbol_reference.md`](10_symbol_reference.md)
  - Update the table for the affected file.
  - Update the **staleness date** at the top.
  - Add any new `.h` files to the staleness watch list.

### If you added, removed, or changed an emulator core:
- **MUST update** [`AGENT_MANIFEST.json`](../../AGENT_MANIFEST.json)
  - Add/remove the emulator entry.
  - Set `engine_status` (`production` / `stub` / `partial`).
  - Set `build_verified` and `last_hardware_verified`.
- **MUST update** [`docs/software-design-document.md`](../../docs/software-design-document.md)
  - Update the Engine Status Registry table (Section 1).
  - Bump the document version minor number.
- **MUST update** `10_symbol_reference.md` with new public APIs.

### If you changed verification status of any issue:
- **MUST update** [`04_known_issues.md`](04_known_issues.md)
  - Correct the status tag using only: OPEN, IN_PROGRESS, FIXED_UNVERIFIED,
    STUB, VERIFIED_HOST, VERIFIED_SIMULATOR, VERIFIED_HARDWARE, RESOLVED, WONT_FIX, DEBUNKED.
  - Quote the exact evidence (test output line, arduino-cli output line).

### After every task (mandatory, no exceptions):
- **MUST append** a dated entry to [`CHANGELOG.md`](../../CHANGELOG.md)
  - One or more of: Added / Fixed / Changed / Deprecated / Removed.
  - Reference files modified.
- **MUST update** [`04_known_issues.md`](04_known_issues.md) if any issue status changed.

### If you added a new agent rule file:
- **MUST register** it in [`README.md`](README.md).
- **MUST add** it to [`CONTEXT_INDEX.json`](CONTEXT_INDEX.json).
- **MUST add** a reference in [`31_quick_start_primer.md`](31_quick_start_primer.md) if it affects the task decision table.

### If you changed the flash/partition layout:
- **MUST update** [`docs/software-design-document.md`](../../docs/software-design-document.md)
  - Hardware Capability Matrix section.
- **MUST update** [`AGENT_MANIFEST.json`](../../AGENT_MANIFEST.json) if FQBN or partition scheme changed.

---

## Verification Checklist (Run Before Committing)

Before running `git commit` on any non-trivial change, answer every applicable question:

```
[ ] Did I update 10_symbol_reference.md staleness date?
[ ] Did I add all new .h files to the symbol reference watch list?
[ ] Did I update all new/changed public API signatures in the symbol table?
[ ] Did I update AGENT_MANIFEST.json for any emulator addition/change?
[ ] Did I update docs/software-design-document.md Engine Status Registry?
[ ] Did I update 04_known_issues.md with accurate status tags?
[ ] Did I append to CHANGELOG.md with a dated entry?
[ ] Did I register any new rule files in README.md and CONTEXT_INDEX.json?
[ ] Did validate_repo.py pass clean? (python scripts/validate_repo.py)
[ ] Did all unit tests pass? (python -m unittest discover tests)
```

---

## Anti-Pattern: "I'll Update Docs Later"

There is no "later" in a multi-agent sequential workflow. Each agent session is
isolated. If documentation is not updated before this session ends, the next agent
has no way to know it is stale. The only correct behavior is: **update docs now,
as part of this task, before the commit**.

---

## Enforcement

`validate_repo.py` Phase 4 (Structural Soundness) checks:
- All emulators in `AGENT_MANIFEST.json` have `engine_status` fields.
- All stream methods are declared in `display_emu.h`.

Future CI phases will be extended to check SDD version consistency and symbol
reference staleness dates. Until then, this rule is enforced by agent discipline
and code review.
