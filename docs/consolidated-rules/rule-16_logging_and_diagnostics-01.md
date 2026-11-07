# Section 16 — Logging & Diagnostics

Source: [`16_logging_and_diagnostics.md`](../../.agents/rules/16_logging_and_diagnostics.md).

# Logging & Diagnostics

## Log Levels
- `ERROR`: Subsystem entered its fault state.
- `WARN`: Degraded but continuing (e.g. SD absent, falling back to baked ROM).
- `INFO`: State transitions: boot, menu, ROM load, core switch.
- `DEBUG`: Per-frame/high-frequency — must be compile-time gated out of any non-dev build, since `Serial.print` in a hot loop violates `15_performance_budgets.md`. Note: The current codebase contains violations of this rule (appears in 9 files, 41 total call sites); they are tracked in `04_known_issues.md`.

## Implementation Rules
- Gate `DEBUG` (and ideally `INFO`) behind one compile-time flag (e.g. `LOG_LEVEL` in `config.h`) — one knob, not scattered per-file `#ifdef DEBUG_FOO` blocks.
- The same banned-superlatives list as `06_verification_standards.md` applies to any log/diagnostic string shown to a human — a log line is a status report.

## Visible Crash States
- On-device crash/error screens state: which subsystem, which error code (`14_error_handling_and_fault_isolation.md`'s enums), and, where feasible, a short actionable next step ("re-insert SD card") — not just "Error."

## Boot Log
- Boot log (`INFO` minimum) states: firmware version (`17_release_and_versioning.md`), which `FEATURE_*` flags are compiled in, and the SD self-test result (`software-design-document.md` §7) — the fastest way for a human or the next agent session to know what they're looking at.

## Secrets
- Never log secrets/keys. Not currently applicable (no network stack), but stated now so it isn't forgotten if WiFi/OTA is ever added.