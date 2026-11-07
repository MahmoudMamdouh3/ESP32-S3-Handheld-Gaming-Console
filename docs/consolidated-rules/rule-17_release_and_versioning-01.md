# Section 17 — Release & Versioning

Source: [`17_release_and_versioning.md`](../../.agents/rules/17_release_and_versioning.md).

# Release & Versioning

## Version Location
- Firmware version lives in one place (`src/core/version.h` — create if absent) as `FW_VERSION_MAJOR/MINOR/PATCH` plus a build-date string, printed in the boot log per `16_logging_and_diagnostics.md`. Note: currently no version constant exists; the next agent to prepare a release must create it.

## Bump Rules
- Bump **PATCH** for bug fixes.
- Bump **MINOR** for new features/cores/screens.
- Bump **MAJOR** for anything breaking save-file/ROM compatibility with a prior version.
- State this explicitly in the commit body, since "I lost my save" is a real risk on a personal device, not an abstract semver nicety.

## Tagging
- `hw-verified-YYYYMMDD-<feature>` tags (`05_git_workflow.md`) verify one feature on hardware; a version bump is "what's actually flashed right now."
- A version can bundle several already-hw-verified features accumulated since the last bump — don't conflate the two tagging systems.

## Release Checklist
Before flashing a build meant for actual day-to-day use (not bench iteration):
- Compiles clean.
- Every touched subsystem is at least `VERIFIED_HOST` (and `VERIFIED_HARDWARE` for anything CPU-core/memory/ISR-related).
- `04_known_issues.md` reviewed for any `OPEN`/`IN_PROGRESS` entry affecting features in use.
- Version bumped and confirmed printed in the boot log.

## Human Changelog
- Keep a human-facing version changelog (e.g. `docs/CHANGELOG.md`) distinct from `04_known_issues.md`'s granular per-task log.
- The task log is for agents/developers; the version log is "what changed since I last flashed this."