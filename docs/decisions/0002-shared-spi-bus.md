# ADR 0002: Shared Display and SD SPI Bus

- Status: Accepted
- Date: 2026-11-07
- Source: `config.h`, `spi_arbiter.*`, `display_emu.cpp`, `sd_card.cpp`

## Context

The ST7789 display and SD card share GPIO11 MOSI and GPIO12 SCK, with separate chip-selects. Display work can run on a worker task while storage runs in the main loop. Concurrent transactions would corrupt both devices.

## Decision

Keep the current wiring and serialize device use through `SpiArbiter`. The configured display clock is 80 MHz, the SD clock is 25 MHz, and Diagnostics can compare display transfers at 40 MHz. These are software settings, not measured signal rates. The ST7789 data sheet specifies a 16 ns serial write cycle; operation at 80 MHz remains a hardware timing risk.

## Consequences

Every SPI call site must be audited for lock acquisition and release. A timeout cannot be silently ignored. Hardware validation must capture clock, chip-select, MOSI, and MISO behavior under real transfers before changing the default clock.
