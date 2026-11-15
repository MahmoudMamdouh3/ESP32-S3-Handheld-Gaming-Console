# ADR 0011: Variable-Length String-Pool Catalog Index (`BMOIDX03`)

- Status: Accepted (Implemented in `rom_index.h` and `sd_catalog.cpp`)
- Date: 2026-11-13 (Updated: 2026-11-15, verified at `d8d4b5e`)
- Source: `firmware/BmoGameboy/src/storage/rom_index.h`, `sd_catalog.cpp`, `scripts/deploy_sd_complete.py`

## Context

The earlier `BMOIDX02` index format stored a fixed 64-byte filename inside every 72-byte `BmoIndexEntry`, which truncated or skipped valid nested ROM paths longer than 63 bytes and wasted fixed padding on short paths.

## Decision

Implement `BMOIDX03` (`BMO_INDEX_MAGIC = "BMOIDX03"`, `BMO_INDEX_VERSION = 3U`) in `firmware/BmoGameboy/src/storage/rom_index.h`:

1. **`BmoIndexHeader` (`44` bytes packed):** Stores magic `"BMOIDX03"`, version `3`, `entryCount`, `entrySize` (`20`), `stringTableBytes`, payload `crc32` over `(entries + stringTable)`, FAT root `timestamp`, 64-bit aggregate `totalBytesLow`/`totalBytesHigh`, and `BmoIndex::catalogFingerprint(...)`.
2. **`BmoIndexEntry` (`20` bytes packed):** Stores `pathOffset` (`uint32_t`), `pathLength` (`uint16_t`, `1..255` bytes excluding NUL), `type` (`uint8_t`), `flags` (`uint8_t`), `fileSize` (`uint32_t`), `crc32Prefix` (`uint32_t` over the first `min(fileSize, 4096)` bytes), and `reserved` (`0`).
3. **In-Memory PSRAM Representation:** `SDCard` stores a `16`-byte `RomFile` descriptor per catalog entry (`pathOffset`, `fileSize`, `crc32Prefix`, `pathLength`, `type`, `flags`) plus a contiguous PSRAM UTF-8 string pool (`s_pathPool`).
4. **Backward Compatibility:** If `/.bmo_index` has legacy `"BMOIDX02"` magic on mount, `SDCard::loadIndex()` rejects the stale v2 cache and triggers a clean `BMOIDX03` rebuild.

## Consequences

- Supports relative ROM paths up to `255` bytes (`MAX_ROM_PATH_LEN = 255`) without inflating every entry slot to 256 bytes.
- Reduces per-slot PSRAM descriptor overhead from `72` bytes to `16` bytes (`RomFile`) plus actual UTF-8 path length (`HOST_TESTED` via `tests/test_rom_index_layout.py` and `tests/test_rom_index.py`; `TARGET_COMPILED`).
