# Git & Commit Discipline

## Commit granularity
- One logical change per commit.
- Always checkpoint-commit BEFORE starting risky/multi-step work so any session is revertable to a known-good point.

## Mandatory Two-Part Commit Message Format (`-m "subject" -m "bullet body"`)
Every commit MUST provide **both** a concise subject line (`-m "type(scope): summary"`) AND a detailed bullet-point body (`-m "- bullet 1\n- bullet 2\n- Verified: ..."`). Single-line commit messages are rejected by the `.git/hooks/commit-msg` hook.

1. **Subject line (`-m "type(scope): summary"`):**
   - `type`: `fix | feat | docs | test | refactor | chore | perf`
   - `scope`: module touched (e.g. `emu_walnut`, `rules`, `sd_card`, `guardian`)
   - Summary must be concise, objective, and technical. **Never use marketing filler** (`groundbreaking`, `amazing`, `ultimate`, `state-of-the-art`, `production grade`, `overkill`).
2. **Detailed body (`-m "- ..."`):**
   - Must contain bullet points (`- ...`) explaining every file/subsystem changed and why.
   - Must state exact verification results (e.g. `- Verified: validate_repo.py PASS (Flash 33.9%, SRAM 66.8%), 120/120 unit tests`).
   - Example:
     ```powershell
     git commit -m "feat(guardian): add cross-document pin verifier and prompt-pack CLI" -m "- Added DocTruthVerifier in tools/guardian/core/doc_verifier.py to catch pin/symbol drift across docs and rules
     - Added prompt-pack and diff-audit subcommands to tools/guardian/cli.py
     - Fixed ElfAnalyzer sketch ELF path discovery in tools/guardian/core/elf_analyzer.py
     - Verified: python scripts/validate_repo.py PASS, 125/125 unit tests"
     ```

## Tags for hardware-verified states
- After a human confirms a PASS on real hardware per a handoff checklist, tag that commit: `git tag hw-verified-YYYYMMDD-<short-feature>`.
- List active tags at the top of `04_known_issues.md`.

## Branching
- Default to committing directly to `main` for docs-only, test-only, or verified changes.
- For experimental changes touching `00_hard_stops.md` categories, create a short-lived branch (`fix/<topic>` or `feat/<topic>`) and merge only after `python scripts/validate_repo.py` passes.

## What never gets committed
- Anything excluded by `.gitignore` (ROM archives, build directories, `.elf`/`.bin`/`.exe` outputs).
- Downloaded compiler toolchains or binary blobs.
