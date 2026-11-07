# ADR 0013: Flash Partition Layout

- Status: Proposed
- Context: `firmware/BmoGameboy/partitions.csv` has NVS, `otadata`, one `ota_0` app slot, and `ffat`. Its final partition ends at `0xFF0000`; the final `0x10000` bytes of the 16 MiB flash address space are unpartitioned. Firmware source contains no FFat mount or file-operation call and no OTA update path.
- Decision: Preserve the existing CSV in this change. Any partition layout or OTA/FFat policy change requires explicit user review.
- Consequences: The FFat area remains reserved but unused by current firmware. `otadata` exists despite the absence of an update workflow; a single app slot does not provide an A/B OTA target. The 64 KiB tail remains unallocated.
- Evidence: The tracked partition CSV and a source search of `firmware/BmoGameboy/src` and `BmoGameboy.ino` for `FFat` and OTA update APIs.
- Human input: Required before changing partition offsets, sizes, labels, OTA strategy, or FFat use.
