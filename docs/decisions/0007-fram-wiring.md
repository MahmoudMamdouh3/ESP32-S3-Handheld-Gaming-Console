# ADR 0007: Optional FRAM Wiring

- Status: Proposed
- Date: 2026-11-07
- Decision owner: User

## Context

The user has no FRAM part or wiring plan. `config.h` currently labels GPIO43 and GPIO44 as I2C SDA/SCL for planned optional FRAM.

## Decision

Keep GPIO43/44 unchanged as a proposal. Do not claim FRAM exists or is functional, and do not move other signals to make room for an unspecified part.

## Consequences

Before enabling FRAM, identify the exact part and voltage, confirm ESP32-S3 pin capability and pull-ups, record its address, and add a bus probe/read-write test that cannot overwrite existing save data.
