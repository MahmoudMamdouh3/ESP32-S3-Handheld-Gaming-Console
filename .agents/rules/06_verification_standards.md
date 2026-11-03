# Verification & Evidence Standards
This file exists because two separate incidents already happened: a fully
hallucinated bug report (fabricated function names, presented with
confident specific detail), and a partial test result (9/11 sub-tests)
reported as a full pass with celebratory language. Both came from
rounding uncertain or incomplete evidence up to a confident claim. These
rules exist to make that structurally harder to do by accident.

## The core rule
**Every claim about code or test results must be backed by evidence
obtained in the CURRENT session, quoted literally, not paraphrased, and
not carried forward from memory of a prior session's summary.** If you
did not personally run the command/read the file in this session, say
"I have not re-verified this" rather than restating it as fact.

## Status vocabulary — use these exact terms, nothing else, in
   04_known_issues.md and commit messages:
- `OPEN` — known bug, no fix attempted yet.
- `IN_PROGRESS` — fix attempted, not yet compiled/tested.
- `FIXED_UNVERIFIED` — compiles, but no test has run against it.
- `STUB` — feature/engine is present as a scaffold only; renders a blank
  screen or no-op. Not functionally emulating the target hardware.
- `VERIFIED_HOST` — **requires `arduino-cli compile` to pass** with
  `PartitionScheme=custom` FQBN (see §Verification Ladder below), AND at
  least one Python unit test ran to full completion. Python-only text
  pattern checks DO NOT qualify for VERIFIED_HOST. State the arduino-cli
  binary size output AND the test final line of output literally.
- `VERIFIED_SIMULATOR` — confirmed correct behavior in the PC-side
  BMO Simulator (tools/bmo_simulator/). Note what was verified.
- `VERIFIED_HARDWARE` — a human confirmed a PASS on physical hardware
  against an explicit, pre-stated PASS/FAIL definition. State the git tag
  created for this (see 05_git_workflow.md).
- `RESOLVED` — VERIFIED_HARDWARE has been reached AND no further action
  is needed.
- `WONT_FIX` — deliberately deferred, with a one-line reason.
- `DEBUNKED` — the "bug" was shown not to exist; state exactly how you
  proved its nonexistence (not "recompiled cleanly," since that's
  evidence of absence of a *syntax* error only).
- Never use "successfully passed," "fully resolved," "confirmed working,"
  or similar informal phrasing in the log — always one of the terms above.

## Partial results are not passes
If a test suite has N defined sub-checks (a ROM test, a multi-assert unit
test, a checklist), and fewer than N completed with the suite's own
designed success signal (e.g. a final "Passed" banner, an exit code, a
specific terminal state), the result is `IN_PROGRESS`, not
`VERIFIED_HOST`. State the exact fraction (e.g. "9/11 sub-tests reached
their :ok marker; sub-test 10 did not complete within the allotted
budget"). Do not editorialize about how likely the remaining tests are to
pass. If a resource limit (frame budget, timeout, memory) is the
suspected cause of an incomplete run, that is itself something to fix and
rerun, not something to reason past.

## Banned AI Marketing Jargon & Hyperbolic Slop
No superlatives, hype, or marketing framing in status reports, PRs, docs, or commit messages.
- Banned terms: "amazing", "groundbreaking", "state-of-the-art", "game-changing", "revolutionary", "production-grade", "reviewer-grade", "masterclass", "overkill", "insane", "flawlessly", "massive victory", "perfection", "miracle", "blazing fast", "ultra-optimized", "bulletproof", "unprecedented", "seamless", "supercharged".
- Mandate: Report what ran, exact output numbers, measured latency in microseconds, memory consumption in bytes, and precise issue status. Use objective, scientific software engineering terminology (see 41_engineering_communication_and_critical_pushback.md). Neutral, empirical, and reproducible reporting is mandatory.

## Quoting code
When asked to show code, or when justifying a claim about existing code,
paste the literal current file content (with a fresh read in this
session) inside a fenced block with line numbers if available. A
description of code ("it just checks bounds and returns") is not a
substitute when the standing order calls for showing code — that
distinction is exactly what caught the semicolon hallucination.

## Verification Ladder

The following ladder defines exactly what evidence is required at each tier.
An agent MUST NOT claim a higher tier without the stated evidence.

```
STUB
  └── Code exists, no emulation. Renders blank screen. STUB_ENGINE sentinel present.

FIXED_UNVERIFIED
  └── Code change made. Not yet compiled or tested.

VERIFIED_HOST  ← minimum bar for any "it works" claim
  ├── arduino-cli compile with FQBN PartitionScheme=custom passes (exit 0)
  ├── Binary size quoted (e.g. "4,997,068 bytes = 59.6% of 8,388,608 bytes")
  ├── At least one Python unit test ran to full completion (exit 0, OK output)
  └── Both outputs quoted literally in the session/log.
  NOTE: Python text-pattern tests alone are NOT sufficient.
  NOTE: A 158% flash overflow indicates wrong partition scheme, not just big code.

VERIFIED_SIMULATOR
  └── BMO Simulator (tools/bmo_simulator/) confirmed behavior visually.

VERIFIED_HARDWARE
  └── Human flashed and tested on the physical ESP32-S3 board.
  └── PASS/FAIL criteria stated in advance. Git tag created per 05_git_workflow.md.

RESOLVED
  └── VERIFIED_HARDWARE reached, no follow-up needed.
```

## Anti-Pattern: Python Tests Are Not Compilation Verification

Previous agents claimed `VERIFIED_HOST` based solely on `python -m unittest
discover tests` passing. This is a **critical mistake**. The Python test suite
checks file existence and string presence — it **cannot detect**:
- Flash overflow (158% partition error)
- Link errors
- Type mismatch or include errors in C++
- PSRAM exhaustion at runtime

The canonical compile command must be run and pass before VERIFIED_HOST is claimed:
```powershell
.\arduino-cli.exe compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi" firmware/BmoGameboy
```

## Numeric claims (counts, hit totals, percentages)
If you state a count derived from a search/grep/audit, and you later
restate that count, it must be identical or you must explicitly flag and
explain the discrepancy — do not let a number silently drift between
messages. If unsure, rerun the search rather than recalling the earlier
number.
