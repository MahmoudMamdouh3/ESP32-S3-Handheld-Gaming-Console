# Security & Data Integrity
Note: The current hardware (`01_hardware.md`) has no network stack enabled (no `WiFi.h`/`BluetoothSerial`). If WiFi/OTA is ever added, this file MUST be revisited before that feature ships.

## Untrusted Storage & File Integrity Contracts
- Every SD-sourced file (ROM, save state, binary index, cover art) is untrusted input the moment it's read.
- A malformed file fails gracefully into the visible error or fallback state (`14_error_handling_and_fault_isolation.md`), never reads/writes outside its buffer.

### Save State Integrity (`SaveManager`)
- **Magic & Versioning:** Every save state file must begin with `SaveStateHeader` containing magic `BMOSS01` and `version == 1`.
- **Payload CRC32:** IEEE 802.3 CRC32 is calculated over the entire state and RAM payloads and verified before deserializing into core registers or memory.
- **Core ID Protection:** Header contains a unique 32-bit `coreId` (e.g. `'PNUT'`, `'WLNT'`, `'AGNS'`). Reject restores if `expectedCoreId != header.coreId` to avoid fatal core register corruption.
- **Atomic Writes:** Save states write to a `.tmp` scratch file first, and only replace the destination file upon successful write and flush, preventing brownouts from leaving corrupt zero-byte save files.

### Binary ROM Index Integrity (`.bmo_index`)
- **Magic & Bounds:** Header contains `BMOIDX01`, `version == 1`, and `entryCount`.
- **Size Verification:** Exact file size must equal `sizeof(BmoIndexHeader) + entryCount * sizeof(BmoIndexEntry)` (36 + N * 72 bytes).
- **CRC32 Checksum:** Header stores CRC32 over the payload. If corrupted or truncated, `SDCard::loadIndex()` immediately rejects the file and falls back to a fresh FAT directory crawl, rewriting a healthy `.bmo_index`.

### Box Art Decoding Integrity (`BoxArt`)
- **Dimension Guards:** Only exactly 64×64 pixel BMP images and 8,192-byte raw images are accepted.
- **BMP Format Guards:** BMP header must match `BM` (0x4D42), support only BI_RGB / BI_BITFIELDS (compression 0 or 3), and support only 16-bit (565) or 24-bit (888) color depths.
- **Buffer Safety:** Decodes directly into an 8KB PSRAM buffer with strict coordinate bounds checking, preventing buffer overruns.

## Path Traversal
- ROM/save filenames read from the SD directory must not be used to construct a path that escapes the intended directory (no `..` traversal). 
- All save state and battery RAM paths are forced to the `/saves/` directory. All box art paths are forced to `/boxart/` or `/covers/`.

## FRAM Integrity
- FRAM save-blob integrity: since saves moved to the FM24C FRAM module specifically for write endurance (`software-design-document.md` §7), add a checksum/CRC to the save blob so a partial write (e.g. from a brownout) is detected on load instead of silently loading a corrupted game state. (Note: Check if this already exists before proposing it as new).


## Python Tooling
- `process_games.py`/`validate_repo.py` process ROM `.zip` archives. 
- If these are ever run against files from an untrusted source (not the developer's own trusted local collection), note Python's `zipfile` is vulnerable to zip-bomb/path-traversal patterns and should validate member paths explicitly. (Forward-looking only; don't over-engineer a threat model that doesn't currently apply).
