# Performance status report

**Report date:** 2026-11-07
**Evidence boundary:** Source review and host checks only. No physical board measurements were collected in this report.

## Current configuration

- Display SPI: 80 MHz configured; the ST7789VW datasheet's published serial-write timing is equivalent to 62.5 MHz. A 40 MHz Diagnostics comparison exists, but no physical test result is recorded.
- SD SPI: 25 MHz configured; mounted-card throughput has not been measured here.
- Pacing targets: 16,742 us for GB/GBC, 28,571 us for DOOM, 33,333 us for the PICO-8-style runtime, and 16,667 us for remaining wrapper indices.
- Frame windows: firmware stores 600 pre-wait core execution samples and logs p50/p95/p99. No physical capture is attached.
- PSRAM: `python -m tools.guardian psram-budget` reports source-derived persistent allocations and computes the DOOM zone from largest-free-block at launch. It cannot model runtime fragmentation.

## Current session verification

Record only completed commands and literal final output from this session in the dated [remediation handoff](handoffs/2026-11-07-remediation.md). A failed or skipped Arduino package download is not a target compile. Old ELF numbers are not current binary metrics.

## Physical measurements waiting for collection

No boot time, card mount time, ROM load time, frame timing, stack watermark, SPI waveform, panel stability, current draw, or battery runtime is reported as measured here. Use [the capture procedure](hardware_measurements.md) and its templates after the board is available.

## Historical report

The former line-by-line report is retained at [`archive/performance-report-2026-11-03.md`](archive/performance-report-2026-11-03.md). Its metrics are historical and have not been re-measured against the current source.
