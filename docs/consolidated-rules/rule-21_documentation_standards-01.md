# Section 21 — Documentation Standards

Source: [`21_documentation_standards.md`](../../.agents/rules/21_documentation_standards.md).

# Documentation Standards
Purpose: `docs/` currently exists as human-readable stubs (`hardware-notes.md`, `software-design-document.md`, `project-rules.md`, `wiring/`) with no stated content contract — give it one so it can't drift.

## Documentation Hierarchy
- `docs/` is for humans; `.agents/rules/` is ground truth the moment the two disagree.
- Any `docs/` page describing current hardware/architecture state links to the relevant `.agents/rules/` file rather than duplicating it.

## UI Screens
- Every new navigable UI state/screen (per `08_ui_style_guide.md`'s existing checklist item) gets one short `docs/` page: purpose, how to reach it, which `FEATURE_*` flags gate it.

## Code Headers
- File header comment template for new first-party `.h`/`.cpp` files: one line of purpose, which `FEATURE_*` flag (if any) gates the whole file, and a pointer to the governing rules file if applicable (e.g. `display_emu.cpp` → `08_ui_style_guide.md`).
- Function-level doc comments required for anything declared in a header (exposed outside its own file): one line of purpose plus any non-obvious precondition (e.g. "must be called after `SPI.begin()`").
- Not required for file-local `static` helpers where name + one-line body already says it.

## Architecture Decision Records (ADR)
- Lightweight ADR for any change matching `03_conventions.md` section 11's "Architecture" trigger.
- Add a short dated note under `docs/adr/` (create if absent) stating the decision and the one-sentence reason, cross-referencing the rules file updated as a result.
- Deliberately lightweight — a few sentences, not a formal template.