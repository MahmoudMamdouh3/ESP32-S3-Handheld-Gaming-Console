# Section 36 — Human-Reported Hardware Bug Intake Protocol

Source: [`36_bug_intake_protocol.md`](../../.agents/rules/36_bug_intake_protocol.md).

# Human-Reported Hardware Bug Intake Protocol
Purpose: prevent a repeat of INC-1 (Hallucinated Bug Report,
`23_incident_postmortem_log.md`). When a human reports the device "isn't
behaving as expected" with no further detail, an agent's default
instinct is to form a plausible-sounding hypothesis and start editing
code. This file makes that instinct structurally harder to act on.

## When this applies
Any report of unexpected physical-device behavior with fewer than:
(a) a specific visual/behavioral description, (b) a when/how-to-reproduce
note, and (c) whether it's a regression. If a report already includes
all three, you may skip straight to the static-audit phase below. If it
doesn't, do NOT start editing code — do intake first.

## Phase 1: Structured intake (ask, don't assume)
Ask the human, as a concrete checklist (not open-ended prose):
1. **WHEN does it happen?** (always / specific state or transition /
   intermittent / after N minutes)
2. **WHAT does it look like, specifically?** Offer a menu of concrete
   options relevant to the subsystem in question rather than an open
   "describe the bug" — humans under-describe visual bugs in prose, a
   checklist gets better signal.
3. **Any correlated action?** (button presses, SD removal, console switch)
4. **Regression or always-broken?** If regression, roughly when did it last
   work (a date, a firmware commit, "before the last flash")?
5. **Can they capture a Serial Monitor log around the event?**

If some questions go unanswered, proceed with what's available but list
the gaps explicitly in your final report's "Waiting on you" section —
never backfill an unanswered question with an assumption stated as fact.

## Phase 2: Static audit BEFORE hypothesis
Before naming a root cause, read the actual live code for the implicated
subsystem and check it against every documented contract/mistake entry
for that subsystem (e.g. `28_display_and_spi_contract.md` +
`30_common_agent_mistakes.md` for display bugs; `35_bmo_face_contract.md` for
mascot bugs). For each relevant contract, report PASS (quote the line
proving it) or FAIL (quote the violating line) — "looks fine" without a
quoted line is not acceptable evidence (`06_verification_standards.md`).

## Phase 3: Map symptom to hypothesis
Only after Phase 1 + Phase 2, state which hypothesis the EVIDENCE
supports. A hypothesis whose corresponding Phase 2 check came back PASS
is ruled out — do not pursue it further. If no hypothesis is supported
with confidence, say so explicitly and propose gated LOG_DEBUG
instrumentation (`16_logging_and_diagnostics.md`) as the next step, rather
than picking the most plausible-sounding guess.

## Phase 4: Instrumentation is the last resort, not the first move
If Phase 2 can't rule a hypothesis in or out from static reading alone,
add the minimum `LOG_DEBUG` calls needed, gated behind `LOG_LEVEL`, and
explicitly hand off to the human for a physical capture — this is a
"Waiting on you" item, not something you can verify yourself.

## Phase 5: Fix only with evidence, verify with an explicit PASS bar
State the fix's plan in 3-6 bullets citing which Phase 2/4 evidence
justifies it (`07_task_protocol.md`). Before claiming any status better
than `FIXED_UNVERIFIED`, state an explicit, human-checkable PASS/FAIL
description (`06_verification_standards.md`'s `VERIFIED_HARDWARE`
requirement) — e.g. "X transitions within 1 second, no corruption over 5
repetitions" — not "should be fixed now."

## Anti-pattern this file exists to prevent
Naming a specific function, line number, or variable in a bug report
that you have not personally greped from a live file THIS session. This
is precisely how INC-1 happened — a fabricated, confidently-specific
report. If you don't know it, say "I have not verified this" instead of
inventing a plausible detail.