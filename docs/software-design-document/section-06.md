# SDD Section 06: Memory and Flash Layout

[Back to the SDD index](../software-design-document.md)

## 6.1 Flash partitions

The exact layout is [`firmware/BmoGameboy/partitions.csv`](../../firmware/BmoGameboy/partitions.csv). The target profile selects this custom table.

| Name | Type | Offset | Size | Purpose |
| :--- | :--- | ---: | ---: | :--- |
| `nvs` | data/nvs | `0x9000` | `0x5000` (20 KiB) | ESP-IDF key/value store |
| `otadata` | data/ota | `0xE000` | `0x2000` (8 KiB) | OTA selection metadata |
| `app0` | app/ota_0 | `0x10000` | `0x800000` (8 MiB) | Firmware image and compiled assets |
| `ffat` | data/fat | `0x810000` | `0x7E0000` | On-flash FAT partition |

The firmware source has no `FFat` mount or file-operation call. The table ends at `0xFF0000`, leaving the final `0x10000` (64 KiB) of the 16 MiB address space outside a partition. `otadata` is present and the table contains one application slot (`ota_0`); this repository has no OTA update path. The partition CSV is unchanged. Any proposed layout change needs user review and a proposed decision record; see [ADR 0013](../decisions/0013-partition-layout.md).

Only a successful current compile provides current image-size data. Old build output, source byte totals, and the sum of ROM header text are not substitutes for target binary metrics.

## 6.2 Memory classes and allocation policy

The ESP32-S3-N16R8 provides internal memory and 8 MiB OPI PSRAM. Runtime free bytes and the largest free block differ; a large allocation must use the largest-block measurement, not total free memory.

| Class | Use | Policy |
| :--- | :--- | :--- |
| Internal DRAM | RTOS/task stacks, frequently accessed small state, driver state | Do not place full ROMs, frame buffers, or large save-state staging blocks here. Check stack high-water in words and target memory usage after a fresh build. |
| Internal instruction/cache resources | Executable firmware, IRAM-attributed hot code, interrupt paths | Keep attributes and code placement intentional; verify the final ELF/map rather than infer from annotations. |
| OPI PSRAM | ROM catalog, ROM buffers, render buffers, large emulator state and temporary save-state data | Check allocation return values and free partial allocations on failure. |
| SD card | ROMs, catalog index, saves, states, artwork and screenshots | Access through the shared SPI arbitration contract; stage and verify replacement writes. |

The frame timing collector stores 600 target `unsigned long` samples in static storage. On ESP32-S3 this is 2,400 bytes; it is reset for each emulator launch. This is source-derived, not a current linker map measurement.

## 6.3 ROM catalog pool and DOOM zone

`PsramBudgetAnalyzer` derives persistent allocation estimates from source declarations, including the ROM pool and display/artwork buffers. Current source-ledger checkpoints are:

| ROM slots | Persistent allocation model | Aggregate PSRAM remaining model | DOOM zone upper bound |
| ---: | ---: | ---: | ---: |
| 256 | 796,672 B | 7,591,936 B | 6,291,456 B |
| 2,500 | 976,192 B | 7,412,416 B | 6,291,456 B |
| 36,000 | 3,656,192 B | 4,732,416 B | 4,194,304 B |

These are source model values, not free-memory readings. Catalog capacity grows in bounded steps. DOOM calculates its zone from the current largest free PSRAM block, subtracts a 524,288-byte margin, and clamps the result to 2,621,440–6,291,456 bytes. Fragmentation and concurrent allocations can lower the actual available block. See [`psram_zone_budget.h`](../../firmware/BmoGameboy/src/emulators/psram_zone_budget.h) and `python -m tools.guardian psram-budget`.

## 6.4 Failure handling and alignment

- Check allocation results before dereferencing; unwind already acquired allocations on partial failure.
- The launcher keeps bounded fallback arrays when the large visible-ROM arrays cannot be allocated.
- Save-state reads stage payload into PSRAM, verify size and CRC, then copy into live emulator state only after validation succeeds.
- `RomFile` and `BmoIndexEntry` have explicit size/offset assertions. Current entry size is 72 bytes and its serialized `fileSize` field begins at byte 68.
- Some contexts and rows declare 32-byte alignment. Alignment is a source invariant, not a measured speedup claim.

See [Section 10](section-10.md) for index and save formats, and [Section 14](section-14.md) for current memory verification commands.
