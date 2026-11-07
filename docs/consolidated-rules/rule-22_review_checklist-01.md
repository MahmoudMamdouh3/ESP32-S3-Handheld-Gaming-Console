# Section 22 — Everyday Self-Review Checklist

Source: [`22_review_checklist.md`](../../.agents/rules/22_review_checklist.md).

# Everyday Self-Review Checklist
Purpose: a lightweight everyday counterpart to `05_git_workflow.md`'s review gate, which only fires for specific high-risk triggers.

Checklist to run before marking any task done, regardless of size:
- No leftover debug `Serial.print`/commented-out code from the working process.
- No hardcoded pins, colors, or magic timing constants outside `config.h`/`theme.h`.
- Every `TODO` added has a scope + description (`13_code_style.md`'s format), or is a `04_known_issues.md` entry instead.
- `10_symbol_reference.md` regenerated if any public function/struct/macro changed shape.
- `04_known_issues.md` changelog updated (`07_task_protocol.md` step 12).
- Any `.agents/rules/` file that should have changed per `03_conventions.md` section 11's Documentation Maintenance Protocol actually did.
- Compile re-run after the last edit — not just "it compiled before this last tweak."
- Final report uses the two-header format with nothing blurred between "verified" and "waiting on you."

This checklist is self-review, not a second reviewer — it doesn't replace the explicit human-approval gates already required elsewhere. It covers everything below that bar, which currently has no checklist at all.