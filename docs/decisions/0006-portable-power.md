# ADR 0006: Portable Power Direction

- Status: Proposed
- Date: 2026-11-07
- Decision owner: User

## Context

The portable power source is not installed or measured. The user selected 3×AA NiMH cells plus a boost converter as the plan.

## Decision

Record that direction as the design plan only. Do not modify board wiring or battery feature flags based on it. The exact cell holder, converter, output voltage, maximum current, switch, fuse, low-voltage behavior, charging method, and thermal behavior remain unselected.

## Consequences

No direct battery-to-board wiring recommendation is made. Before implementation, match the exact board input and converter specifications, then capture a current-limited bench test and runtime record using the hardware measurement template.
