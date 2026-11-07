# ADR 0011: Catalog Path Storage Options

- Status: Proposed
- Context: `BmoIndexEntry` stores a 64-byte relative filename (63 bytes plus NUL). Longer paths are skipped. The index layout is BMOIDX02 with a 72-byte entry; changing it requires a format migration.
- Decision: Keep the current layout until path-length frequency and migration behavior are measured on the user's media. Do not silently truncate or alter BMOIDX02.
- Alternatives:

| Option | Trade-off |
| :--- | :--- |
| Larger fixed name | Simple access; increases every entry and ROM-pool slot whether needed or not. |
| String pool plus offsets | Stores long names compactly; needs bounds, compaction, migration, and recovery rules. |
| Hashed key plus display name | Compact identity lookup; requires collision handling and still needs the display name somewhere. |

- Cost basis: `python -m tools.guardian psram-budget` models 80 bytes per catalog slot today (72-byte entry plus 8 bytes of pointer/index arrays): 20,480 B at 256 slots, 200,000 B at 2,500 slots, and 2,880,000 B at 36,000 slots. A larger fixed name would add its per-slot delta at every capacity; string-pool cost depends on actual path lengths.
- Consequences: No storage-format change is selected. The current limit can omit valid deeply nested or long-named ROMs; the scanner must count and report skips. A human must choose a migration after reviewing a measured path distribution.
- Evidence: `firmware/BmoGameboy/src/core/rom_index.h`, `firmware/BmoGameboy/src/core/sd_card.cpp`, and the Guardian PSRAM budget.
- Human input: Required before changing the persistent index format.
