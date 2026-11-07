# Section 11 — Meta-Rules for the Rules Directory Itself

Source: [`11_rules_meta.md`](../../.agents/rules/11_rules_meta.md).

# Meta-Rules for the Rules Directory Itself
**RULESET_VERSION: 8** <!-- bundle_rules.py reads this release version -->

- Every file in .agents/rules/ must stay under 12,000 characters (the
  original constraint that caused project-rules.md to be split). If a
  file grows past that, split it into a new numbered file and update
  README.md's index — don't silently let one file balloon.
- Numbered files (00-12 existing) are read in numeric order and are
  mandatory context for any nontrivial task. README.md is the index and
  is always safe to read first — it should never itself contain binding
  rules, only pointers.
- Any change to a numbered rules file requires a one-line changelog entry
  in 04_known_issues.md's Changelog section, dated, same as existing
  entries.
- If a rule in one file contradicts a rule in another, 00_hard_stops.md
  always wins, followed by 01_hardware.md (physical ground truth beats
  everything except hard stops). Flag any contradiction you find instead
  of silently picking one.
- Don't restate a hard stop's full text in another file — reference it by
  file+section (e.g. "per 00_hard_stops.md, OPI flash mode") so the
  constraint has exactly one canonical source and can't drift out of sync
  across files.