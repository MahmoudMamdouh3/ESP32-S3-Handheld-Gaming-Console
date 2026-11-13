# Storage & Binary Formats

## Binary ROM Catalog (`/.bmo_index` — `BMOIDX03`)

Defined in [`firmware/BmoGameboy/src/storage/rom_index.h`](../firmware/BmoGameboy/src/storage/rom_index.h) with static size/offset assertions (`HOST_TESTED` parity in `tests/test_rom_index_layout.py`, `tests/test_rom_index.py`, and `tests/test_sd_deploy.py`):

- **Header (`44 bytes`):**
  - `magic[8]`: `"BMOIDX03"` (legacy `"BMOIDX02"` files are automatically rebuilt to v3 on mount)
  - `version`: `3` (`uint32_t`)
  - `entryCount`: total indexed entries (`<= 36,000`)
  - `entrySize`: `20` (`sizeof(BmoIndexEntry)`)
  - `stringTableBytes`: total UTF-8 NUL-terminated path table size (`<= 8 MiB`)
  - `crc32`: standard IEEE CRC32 (`^ 0xFFFFFFFF`) over `(entries + stringTable)`
  - `timestamp`: FAT root modification stamp (`0` marks a host-prebuilt index stamped in place on first mount)
  - `totalBytesLow` / `totalBytesHigh`: 64-bit sum of indexed ROM sizes
  - `fingerprint`: `BmoIndex::catalogFingerprint(entryCount, stringTableBytes, totalBytes, rootStamp, payloadCrc)`
- **Entry (`20 bytes`, packed):**
  - `pathOffset` (`uint32_t`): byte offset into UTF-8 path string table (`<= MAX_ROM_PATH_LEN = 255` B per path)
  - `pathLength` (`uint16_t`): path byte length excluding trailing NUL
  - `type` (`uint8_t`, `RomType`), `flags` (`uint8_t`, `FLAG_FAVORITE | FLAG_BOX_ART | FLAG_POPULAR`)
  - `fileSize` (`uint32_t`)
  - `crc32Prefix` (`uint32_t`): IEEE CRC32 of the first `min(fileSize, 4096)` bytes (verified on launch against SD card swaps)
  - `reserved` (`uint32_t`, `0`)
- **Transactional updates & chunked scan:** `SDCard` writes to `/.bmo_index.tmp`, verifies, rotates `/.bmo_index` -> `/.bmo_index.bak`, installs `.tmp`, and recovers missing indexes from `.bak` on startup (`HOST_TESTED` index validation; card power-loss `NOT_TESTED`). Directory enumeration acquires/releases `SpiArbiter` per 16-entry chunk and skips FatFs hidden/system entries (`AM_HID | AM_SYS`).

## Path Keys (`path_key.h` / `scripts/rom_key.py`)

Both firmware and `scripts/deploy_sd_complete.py` compute deterministic path keys from normalized relative ROM paths: `<sanitized_prefix>_<crc32_hex>`. Keys reduce filename collisions across nested folders (`HOST_TESTED` C++/Python parity via `tests/test_rom_key.py`).

## Saves, States, Favorites & Art

| Artifact | SD Path | Format & Behavior |
| :--- | :--- | :--- |
| Favorites | `/favorites.txt` | Line-delimited full normalized relative ROM paths (legacy basenames auto-migrate when unique across catalog); transactional temp+backup rewrite |
| Recent History | `/history.txt` | Up to 10 most recently launched full relative ROM paths, newest first |
| Battery RAM | `/saves/<rom-key>.sav` | Raw SRAM/PRG/BRAM dump gated by cartridge metadata in `battery_save.h` and live core runtime state (`hasActiveBattery()`) (legacy `<basename>.sav` fallback read supported) |
| Save States (`1..5`) | `/saves/<rom-key>.s01`..`.s05` | `72-byte` `BSS2` header (`SaveStateFormat::HeaderV2`, version `2`, `headerSize`, `coreId`, `pathKeyHash`, `romSize`, CRC32 over state + RAM) with backward-compatible `60-byte` `BSS1` v1 loading (`HOST_TESTED` header/staging checks) |
| Screenshots | `/screenshots/<rom-key>_<millis>.bmp` | Bottom-up 24-bit uncompressed BMP converted from active framebuffer |
| Box Art | `/boxart/` or `/covers/` | `64x64` raw BGR565 or BMP asset indexed in one sorted pass by `BoxArt::populateCatalogAvailability()` |

## Local SD Deployment Tooling

- Copy local user-owned ROMs to a FAT32 card using:
  - `python scripts/deploy_to_sd.py --destination F:\` (dry-run by default; requires `--apply` and matching volume label to write)
  - `python scripts/deploy_sd_complete.py` (builds `BMOIDX03` index with 4 KiB ROM prefix CRCs + keyed covers; honors `BMO_DATA_DIR` environment override when data is outside the repository root)
- No commercial ROMs or scraper tools are included in this repository (`BMO_ENABLE_BAKED_ROMS=0` by default).
