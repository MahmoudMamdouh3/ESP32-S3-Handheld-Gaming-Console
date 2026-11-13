# Storage & Binary Formats

## Binary ROM Catalog (`/.bmo_index` — `BMOIDX02`)

Defined in [`firmware/BmoGameboy/src/storage/rom_index.h`](../firmware/BmoGameboy/src/storage/rom_index.h) with static size/offset assertions (`HOST_TESTED` parity in `tests/test_rom_index_layout.py` and `tests/test_sd_deploy.py`):

- **Header (`36 bytes`):**
  - `magic[8]`: `"BMOIDX02"`
  - `version`: `2` (`uint32_t`)
  - `entryCount`: total indexed entries (`<= 36,000`)
  - `entrySize`: `72` (`sizeof(BmoIndexEntry)`)
  - `crc32`: IEEE CRC32 over all `BmoIndexEntry` records
  - `timestamp`: FAT root modification stamp (`0` disables cache reuse and forces a rescan)
  - `reserved[0]`: low 32 bits of total catalog byte count
  - `reserved[1]`: `BmoIndex::catalogFingerprint(entryCount, totalBytes, rootStamp, entriesCrc)`
- **Entry (`72 bytes`, packed):**
  - `filename[64]` (`char[64]`, NUL-terminated relative path)
  - `type` (`uint8_t`, `RomType`)
  - `isFavorite` (`uint8_t`), `hasBoxArt` (`uint8_t`), `reserved` (`uint8_t`)
  - `fileSize` (`uint32_t`)
- **Transactional updates:** `SDCard` writes to `/.bmo_index.tmp`, verifies, rotates `/.bmo_index` -> `/.bmo_index.bak`, installs `.tmp`, and recovers missing indexes from `.bak` on startup (`HOST_TESTED` index validation; card power-loss `NOT_TESTED`).

## Path Keys (`path_key.h` / `scripts/rom_key.py`)

Both firmware and `scripts/deploy_sd_complete.py` compute deterministic path keys from normalized relative ROM paths: `<sanitized_prefix>_<crc32_hex>`. Keys reduce filename collisions across nested folders (`HOST_TESTED` C++/Python parity via `tests/test_rom_key.py`).

## Saves, States, Favorites & Art

| Artifact | SD Path | Format & Behavior |
| :--- | :--- | :--- |
| Favorites | `/favorites.txt` | Line-delimited relative ROM filenames; transactional temp+backup rewrite |
| Recent History | `/history.txt` | Up to 10 most recently launched relative ROM paths, newest first |
| Battery RAM | `/saves/<rom-key>.sav` | Raw SRAM/PRG/BRAM dump gated by cartridge metadata in `battery_save.h` (legacy `<basename>.sav` fallback read supported) |
| Save States (`1..5`) | `/saves/<rom-key>.s01`..`.s05` | `60-byte` `BSS1` header (`SaveManager::SaveStateHeader`, version `1`, core ID, CRC32 over state + RAM) + payload (`HOST_TESTED` header/staging checks) |
| Screenshots | `/screenshots/<rom-key>_<millis>.bmp` | Bottom-up 24-bit uncompressed BMP converted from active framebuffer |
| Box Art | `/boxart/` or `/covers/` | `64x64` raw BGR565 or BMP asset indexed in one sorted pass by `BoxArt::populateCatalogAvailability()` |

## Local SD Deployment Tooling

- Copy local user-owned ROMs to a FAT32 card using:
  - `python scripts/deploy_to_sd.py --destination F:\` (dry-run by default; requires `--apply` and matching volume label to write)
  - `python scripts/deploy_sd_complete.py` (builds `BMOIDX02` index + keyed covers; honors `BMO_DATA_DIR` environment override when data is outside the repository root)
- No commercial ROMs or scraper tools are included in this repository (`BMO_ENABLE_BAKED_ROMS=0` by default).
