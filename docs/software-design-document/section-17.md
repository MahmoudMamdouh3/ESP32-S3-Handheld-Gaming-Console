# SDD Section 17: Guardian commands and documentation upkeep

[Back to the SDD index](../software-design-document.md)

## 17.1 Current command groups

The current command names come from `python -m tools.guardian --help`:

- **Repository checks:** `audit`, `verify`, `diff-audit`, `patch-check`, `ui-audit`, `doctor`.
- **Firmware analysis:** `psram-budget`, `budget`, `bus-calc`, `profile-elf`, `bench-host`, `cppcheck`, `clang-tidy`.
- **Grounding and indexes:** `prompt-pack`, `route`, `lookup`, `explain`, `symbols`, `index`, `report`, `qol`, `benchmark-ai`.
- **Integration:** `mcp`, `export-harness`.

Use `python -m tools.guardian <command> --help` before relying on a command's flags. `python scripts/validate_repo.py` is a separate repository validator with phases 0 through 8.

## 17.2 Documentation ownership

- `AGENTS.md` is the repository entry point; active rules live in `.agents/rules/`.
- `04_known_issues.md` and dated handoffs own status and evidence for open findings.
- `CHANGELOG.md` records current changes; earlier entries are retained in `docs/archive/`.
- `docs/software-design-document.md` indexes the SDD sections. Hardware state belongs in Rule 01 and `docs/hardware-notes.md`.
- `docs/generated/`, `docs/consolidated-rules.md`, and `docs/consolidated-rules/` are generated outputs. Rebuild them using the corresponding Guardian or script command.
- `docs/archive/` is historical and may contain superseded source descriptions; use the active index and current source for present behavior.

After changing rules or architecture, run `python scripts/check_markdown_links.py`, `python -m tools.guardian verify`, the UI audit when applicable, `python -m tools.guardian symbols`, `python -m tools.guardian index`, and `python scripts/bundle_rules.py`. Review generated diffs and do not treat a generation command's output as independent source validation.
