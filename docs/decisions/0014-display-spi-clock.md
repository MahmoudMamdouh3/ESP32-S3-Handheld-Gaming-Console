# ADR 0014: Display SPI Clock Risk

- Status: Proposed
- Tags: NEEDS-HARDWARE, NEEDS-USER
- Context: Firmware requests 80 MHz for the display by default. The cited ST7789VW v1.0 timing gives a 16 ns serial write cycle (62.5 MHz); the configured clock period is 12.5 ns. The fitted panel/module marking and board behavior are not measured. The profile can compile a 40 MHz comparison.
- Pinned-source finding: Arduino-ESP32 3.3.11 routes ESP32-S3 SPI requests through the 80 MHz APB source and integer clock-divider factors. Its divider selection does not exceed the request; 40 MHz is the next source-derived rate below 62.5 MHz. `spiAttachMISO()` routes GPIO15 with `pinMatrixInAttach()` and the checked HAL defines no separate software cap for matrix-routed MISO. This does not establish electrical margin or card/display reliability.
- Decision: Keep the configured default unchanged in firmware. Record the risk and require a physical measurement before selecting a permanent operating limit.
- Current implementation: `config.h` still sets `TFT_SPI_HZ` to 80 MHz, and Diagnostics provides a 40 MHz comparison. The production-readiness handoff records a requested 40 MHz default with an explicit 80 MHz opt-in, but that policy has not been implemented. Do not describe the requested policy as active firmware behavior.
- Consequences: The default may exceed the fitted module's published timing. Lower-frequency fallback/override compiles are configuration checks only. A person must measure the soldered display and SD path before accepting a policy.
- Evidence: Arduino-ESP32 3.3.11 `esp32-hal-cpu.c`, `esp32-hal-spi.c`, `boards.txt`, and the ST7789VW v1.0 datasheet linked from SDD Section 07.1.
- Human input: Required to accept the operating clock or change the default after hardware results are recorded.
- Note: ADR 0010 already records the accepted simulator/firmware boundary. This clock record is numbered 0014 to avoid replacing or duplicating that existing decision.
