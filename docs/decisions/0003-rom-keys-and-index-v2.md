# ADR 0003: ROM Metadata Keys and Catalog Index Evolution (`BMOIDX02` -> `BMOIDX03`)

- Status: Accepted (Index layout superseded by [ADR 0011](0011-index-path-storage.md) `BMOIDX03`)
- Date: 2026-11-07 (Updated: 2026-11-15, verified at `d8d4b5e`)
- Source: `firmware/BmoGameboy/src/storage/path_key.h`, `rom_index.h`, `sd_catalog.cpp`, `scripts/rom_key.py`

## Context

The SD catalog can contain repeated basenames across console folders (`gb/tetris.gb` vs `nes/tetris.nes`) and case-insensitive prefix collisions. Basename-only artwork or save paths map distinct ROM files to the same sidecar file.

## Decision

1. **Deterministic Path Keys (`RomKey::make` / `make_rom_key`):** Generate sidecar filenames of up to `32` characters total in the form `<sanitized_stem_up_to_23_chars>_<8_hex_crc32>`, where the 8-character lowercase hexadecimal suffix is the standard IEEE CRC32 of the normalized relative ROM path (leading slashes stripped, backslashes converted to `/`, ASCII lowercased).
2. **Legacy Migration:** Firmware (`SaveManager`) and host deployment scripts (`scripts/deploy_sd_complete.py`) share the identical algorithm (`HOST_TESTED` parity in `tests/test_rom_key.py`). Legacy basename-keyed save files are migrated non-destructively when a unique catalog match exists.
3. **Catalog Binary Index:** Initially introduced as `BMOIDX02` (fixed 64-byte path entries) and subsequently upgraded in [ADR 0011](0011-index-path-storage.md) to `BMOIDX03` (`44`-byte `BmoIndexHeader`, `20`-byte `BmoIndexEntry`, variable-length UTF-8 path string pool up to `255` bytes per path, and `4,096`-byte ROM prefix CRC32). Legacy `BMOIDX02` files are automatically rebuilt to `BMOIDX03` on mount.

## Consequences

Full relative-path CRC32 keys prevent sidecar collisions across subdirectories while remaining human-identifiable via the sanitized stem prefix. Transactional index writes (`/.bmo_index.tmp` -> `/.bmo_index.bak` -> `/.bmo_index`) are `HOST_TESTED`; physical SD card power-loss behavior remains `NOT_TESTED` until hardware bring-up.
