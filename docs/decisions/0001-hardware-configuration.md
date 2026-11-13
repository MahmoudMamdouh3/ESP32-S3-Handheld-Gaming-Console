# ADR 0001: Hardware Configuration Hard Stops

- Status: Accepted
- Date: 2026-11-07
- Source: `firmware/BmoGameboy/src/hal/config.h`, `docs/hardware.md`

## Context

The handheld is a soldered ESP32-S3-N16R8 design. A mistaken pin, flash, PSRAM, partition, audio, or battery assumption can affect boot or damage hardware. Source configuration records intended settings; it does not prove each peripheral is installed or electrically safe.

## Decision

Preserve QIO flash, OPI PSRAM, the custom partition table, and every configured pin unless the user explicitly approves a reviewed hardware change. Keep `FEATURE_BATTERY_MONITOR=0` and `FEATURE_AUDIO=0`. Never use GPIO1 for a battery reading; GPIO33–37 are reserved for OPI PSRAM and GPIO0/3/45/46 are avoided boot-strapping pins.

## Consequences

Documentation labels values as configured, proposed, measured, or unknown. A compile cannot close a physical validation item. The current measurements and safe test procedure are tracked in [the hardware measurement record](../hardware_measurements.md).
