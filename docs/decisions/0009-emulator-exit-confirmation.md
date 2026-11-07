# ADR 0009: Deliberate Emulator Exit

- Status: Accepted
- Date: 2026-11-07
- Decision owner: User delegated interaction choice to the audit
- Source: `BmoGameboy.ino`, Rule 26

## Context

The `SELECT+UP` chord previously could exit directly and lose in-memory progress. The user asked for the safer interaction to be chosen.

## Decision

`SELECT+UP` opens the pause menu with Quit selected. One A press arms a three-second second-press confirmation; another A exits. B or Start resumes and cancels. Any directional/select input while armed cancels the confirmation. On confirmed exit, supported battery RAM is saved before the emulator is destroyed.

## Consequences

The flow is source-reviewed but needs button testing on the physical unit. The pause menu's explicit Quit selection uses the same confirmation path.
