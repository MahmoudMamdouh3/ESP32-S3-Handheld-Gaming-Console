# Section 20 — Multi-Agent / Cross-Session Protocol

Source: [`20_multi_agent_protocol.md`](../../.agents/rules/20_multi_agent_protocol.md).

# Multi-Agent / Cross-Session Protocol
Purpose: this repo is worked on by different AI agents/models across sessions with no shared working memory between them — the changelog already credits "(Agent Antigravity)" by name. Make the handoff explicit rather than accidental.

## Crediting & Logging
- Every `04_known_issues.md` changelog entry representing a distinct agent/model session credits which agent produced it, in parentheses, matching the existing `(Agent Antigravity)` convention.

## Verification Boundaries
- Never trust a prior session's summary of "what's true about the code" as verified fact for your own session — re-run the check yourself if it matters to what you're about to do.
- This restates `06_verification_standards.md`'s core rule for the specifically cross-agent case: a previous agent's confident, well-formatted claim is exactly as unverified to you as an unverified guess of your own.
- If you discover a prior changelog/`04_known_issues.md` status was wrong (e.g. claimed `VERIFIED_HOST` but the quoted output doesn't actually show a full pass), correct it with a new dated entry stating what was wrong and the corrected status — don't silently overwrite the old entry, and don't leave it standing uncorrected.

## Handoffs & Pauses
- When stopping mid-task (context limit, session end, handoff), leave the repo so `git status` plus the latest changelog entry fully explain what's done, half-done, and next.
- Don't rely on an out-of-band chat transcript surviving to the next session, since the next agent may be a different model reading only the repo.

## Conflict Resolution
- If two agents' changes conflict, resolve via git history and commit messages as source of truth, not by guessing intent.
- If genuinely ambiguous, ask the human rather than silently picking a side, per `03_conventions.md` section 10's existing agent-behavior rules.
- Write your plan and final report (`07_task_protocol.md`) assuming zero shared context with whoever reads it next — even if that's "future you" in a new session.