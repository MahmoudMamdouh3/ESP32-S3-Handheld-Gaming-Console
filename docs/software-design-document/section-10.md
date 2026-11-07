# SDD Section 10: SD Catalog and Persistence Formats

[Back to the SDD index](../software-design-document.md)

## 10.1 Catalog scan and ROM dispatch

`SDCard::begin()` registers baked catalog entries, initializes the bounded ROM pool, attempts the SD catalog index, and falls back to directory scanning when the cache cannot be reused. The scanner traverses up to depth 3, skips hidden/system/art/save directories, classifies extensions using `determineType()`, stores the relative filename and file size, checks for matching art, sorts the catalog, and applies favorites/popular metadata. Unsupported or overlong names are skipped and counted; see the source for exact extension mapping.

The deployment script and firmware share path-key behavior through [`path_key.h`](../../firmware/BmoGameboy/src/core/path_key.h) and [`scripts/rom_key.py`](../../scripts/rom_key.py). Keys use normalized relative paths and CRC32 plus a readable sanitized basename prefix. CRC32 is a low-collision, non-cryptographic checksum component; the basename prefix reduces collisions but does not eliminate them.

## 10.2 BMOIDX02 SD catalog index

The binary format is defined by [`rom_index.h`](../../firmware/BmoGameboy/src/core/rom_index.h). The current magic is `BMOIDX02`, version 2. The header is 36 bytes; each entry is 72 bytes. The format has static size/offset assertions and a host parity test.

| Header field | Meaning |
| :--- | :--- |
| `magic[8]`, `version` | Format identity/version |
| `entryCount`, `entrySize` | Number and byte size of serialized entries |
| `crc32` | IEEE CRC32 of all entry records |
| `timestamp` | Root-directory modification timestamp; zero disables cache reuse |
| `reserved[0]` | Total catalog file bytes, low 32 bits |
| `reserved[1]` | CRC32 catalog fingerprint over count, byte total, root stamp, and entry CRC |

Each entry stores a null-terminated 64-byte relative filename, 1-byte `RomType`, 1-byte favorite flag, 1-byte art flag, 1 reserved byte, and 32-bit ROM file size. Loading checks magic/version, entry size, file size, CRC, root stamp/fingerprint, type/flag ranges, null termination, and total bytes before copying entries into the live pool.

Writes use `/.bmo_index.tmp` and `/.bmo_index.bak`, verify staged contents, rename transactionally, and recover interrupted rename sequences on startup. The cache depends on FAT root-directory timestamp behavior. If the timestamp is zero, cache reuse is disabled and the catalog is rescanned. Media replacement and timestamp granularity have not been confirmed on the physical card. The Diagnostics → Select action retries mounting when SD is absent and calls `SDCard::rebuildIndex()` when mounted; see [Section 5.5](section-05-part-02.md#55-diagnostics) for the user-facing behavior. Subdirectory-change detection and card-replacement detection still lack fake-filesystem coverage and physical verification.

## 10.3 Favorites and recent history

`/favorites.txt` and `/history.txt` are line-oriented relative-filename metadata. History keeps up to 10 entries, newest first. Favorites are applied to the current catalog by exact filename comparison. Both files are rewritten through a temporary file, read-back size/CRC verification, backup rename, final verification, and recovery logic. They are not append-only logs.

## 10.4 Save states and battery RAM

[`save_manager.h`](../../firmware/BmoGameboy/src/core/save_manager.h) defines five save slots and `SaveStateHeader` (`BSS1`, version 1, 60 bytes). The header identifies core, payload sizes, timestamp, ROM title, and CRC32 over state plus auxiliary RAM.

| Data | Path | Write/read behavior |
| :--- | :--- | :--- |
| Battery RAM | `/saves/<rom-key>.sav` | Raw RAM payload; supported cartridge/core paths only; staged, flushed, verified, and recovered on replacement |
| Save state slots | `/saves/<rom-key>.s01` … `.s05` | Header plus state and optional RAM; payload CRC checked before live emulator state is modified |
| Legacy save/state | `/saves/<basename>.sav` or `.s0N` | Read/migration fallback when the keyed path is absent; old source is retained |
| Screenshot | `/screenshots/<rom-key>_<millis>.bmp` | BMP header and bottom-up 24-bit pixel rows derived from the supplied BGR565 frame |

Replacement writes stage to a `.tmp` sibling, verify contents, move an existing final file to `.bak`, install the staged file, verify the result, and restore the prior file if the install or verification fails. At the next access, recovery restores a backup when the final path is missing and cleans up stale transaction files. This logic is host/source reviewed; card-level power interruption has not been tested.

Battery persistence is separate from battery voltage monitoring. GB/GBC and NES/SNES paths inspect supported cartridge metadata before saving. The PCE wrapper exposes backup RAM in its save path; whether a title has formatted or modified BRAM is not checked here. Battery-monitor ADC reads remain disabled. See [`battery_save.h`](../../firmware/BmoGameboy/src/core/battery_save.h) and [ADR 0004](../decisions/0004-battery-persistence.md).

## 10.5 Artwork lookup and migration

`BoxArt` loads 64×64 raw or BMP assets from `/boxart/` and `/covers/`, preferring keyed paths. Legacy basename art is considered only when unique in the catalog. Migration copies to a temporary file, verifies the copied size, renames the new keyed destination, and retains the old file. Artwork is cached in PSRAM and drawn through the display subsystem.

## 10.6 Data preservation

The repository validator and code changes must not rewrite or delete user ROM, save, or SD-card directories. Tests may use temporary fixtures, but may not use a user's `games/`, `roms/`, or `sdcard/` data as a write target. Policy/provenance for tracked baked assets remains open; see [ADR 0008](../decisions/0008-rom-policy-and-licensing.md).
