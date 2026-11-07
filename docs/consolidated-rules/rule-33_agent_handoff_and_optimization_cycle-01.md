# Section 33 — 33. Agent Handoff & Continuous Optimization Cycle

Source: [`33_agent_handoff_and_optimization_cycle.md`](../../.agents/rules/33_agent_handoff_and_optimization_cycle.md).

# 33. Agent Handoff & Continuous Optimization Cycle

**Purpose:** Autonomous AI agents operate in sequential, isolated sessions. This rule defines the continuous improvement protocol where every agent leaves clean, verified artifacts and actionable logs, enabling the next agent to immediately build upon progress without friction or lost context.

---

## 1. The 4-Stage Agent Session Lifecycle

Every agent interacting with this repository MUST execute through this lifecycle:

```
[1. ORIENTATION] ───────> [2. INVESTIGATION] ───────> [3. VERIFIED EDIT] ───────> [4. HANDOFF LOG]
Read 31_quick_start.md     Grep active code           Apply minimal diff           Update 04_known_issues.md
Check known issues         Run validate_repo.py       Verify build & host tests    Update CHANGELOG.md
```

### Stage 1: Fast Orientation (First 90 Seconds)
1. Read [`31_quick_start_primer.md`](../../.agents/rules/31_quick_start_primer.md).
2. Check `04_known_issues.md` to see currently active bugs, debunked theories, and pending verifications.
3. Review `AGENT_MANIFEST.json` for current pin maps and build commands.
4. For any human-reported device-misbehavior task with no specific symptom yet, read `36_bug_intake_protocol.md` before Stage 2.

### Stage 2: Investigation & Truth Discovery
1. Never assume an API or struct member exists from LLM training data. Always check `10_symbol_reference.md` or grep the actual header file.
2. Run `python scripts/validate_repo.py` before making edits to confirm a clean baseline.

### Stage 3: Verified Execution & Zero-Regression Edits
1. Adhere to the `EmulatorCoreContract` and zero-allocation hot path invariants.
2. Verify all claims using active session execution (`arduino-cli compile`, `python scripts/validate_repo.py`, `python -m unittest discover tests`).

### Stage 4: Handoff & Optimization Logging
1. If you fixed or investigated a bug, update `04_known_issues.md` with explicit status tag:
   - `OPEN`: Bug confirmed present and unresolved.
   - `FIXED_UNVERIFIED`: Code fix committed; compiles cleanly, awaiting physical hardware confirmation.
   - `VERIFIED_HOST`: Verified by desktop test harness (`tools/host_test.cpp`).
   - `VERIFIED_HARDWARE`: Confirmed functioning on physical perfboard console.
   - `DEBUNKED`: Investigated and proven to be a non-issue or false alarm.
2. Append a dated entry to root [`CHANGELOG.md`](../../CHANGELOG.md).
3. If new public APIs were created or changed, update `10_symbol_reference.md`.

---

## 2. Standard Handoff Report Template

Every agent concluding a task should present findings using this structured format:

```markdown
## Verified by me this session
- [x] Command/Test: `python scripts/validate_repo.py` -> Passed in X ms.
- [x] Command/Test: `.\arduino-cli.exe compile ...` -> Exit code 0 (Sketch: X% flash, Y% RAM).
- [x] Exact code changes made and files touched.

## Known Issues & Handoff State for Next Agent
- Issue status updates logged in `04_known_issues.md`.
- Specific next steps or pending hardware tests for the subsequent session.

## Waiting on you (Human Reviewer)
- Specific questions or physical hardware tests needed.
```