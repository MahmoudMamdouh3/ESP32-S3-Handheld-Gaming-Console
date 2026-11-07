# ADR 0005: PSRAM Pools and DOOM Zone Bounds

- Status: Accepted
- Date: 2026-11-07
- Source: `tools/guardian/core/psram_budget.py`, `psram_zone_budget.h`, `emu_doom.cpp`

## Context

The ROM catalog can grow into tens of thousands of entries. DOOM requires one large contiguous working allocation; aggregate free PSRAM alone does not guarantee that allocation will succeed.

## Decision

Grow the ROM pool in bounded capacities and derive DOOM's zone size from the runtime largest free PSRAM block, with a 512 KiB margin and 2.5–6 MiB clamp. Fail the launch cleanly below the required minimum. The source budget reports aggregate allocation upper bounds but does not model fragmentation.

## Consequences

Host/source checks protect formulas and layout. A current target build and runtime largest-block observation are still required before claiming device success.
