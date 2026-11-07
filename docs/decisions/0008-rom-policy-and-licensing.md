# ADR 0008: ROM Policy and Repository Licensing

- Status: Open
- Date: 2026-11-07
- Decision owner: User

## Context

The repository contains tracked ROM-derived headers and an existing root license. The user will provide a separate ROM policy/license decision. Provenance, redistribution rights, and third-party source licenses are not established by the current source tree alone.

## Decision

Do not add, remove, replace, or relabel ROM assets and do not change the project license. Treat asset policy and licensing as unresolved until the user supplies the intended policy. Keep repository data unchanged while preparing a provenance and vendor-license inventory from verifiable source headers and upstream records.

## Consequences

No legal conclusion is made in this record. Existing license text must not be treated as evidence that every bundled asset is covered by it.
