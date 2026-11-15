# ADR 0012: Save-State Schema v2 (`BSS2`) and ROM Identity Verification

- Status: Accepted (Implemented in `save_state_format.h` and `save_manager.cpp`)
- Date: 2026-11-13 (Updated: 2026-11-15, verified at `d8d4b5e`)
- Source: `firmware/BmoGameboy/src/storage/save_state_format.h`, `save_manager.h`, `save_manager.cpp`

## Context

The legacy `BSS1` save-state header (`60` bytes, `MAGIC_V1 = 0x31535342`) stored `magic`, `version`, `timestamp`, `coreId`, `stateSize`, `ramSize`, `crc32`, and `romTitle[32]`, but lacked an explicit `headerSize`, full relative-path hash (`pathKeyHash`), and `romSize` check.

## Decision

Implement `SaveStateFormat::HeaderV2` (`BSS2`, `72` bytes, `MAGIC_V2 = 0x32535342`, `VERSION_V2 = 2U`) in `firmware/BmoGameboy/src/storage/save_state_format.h`:

1. **`HeaderV2` Layout (`72` bytes):**
   - `magic` (`"BSS2"`), `version` (`2`), `headerSize` (`72`), `timestamp` (`millis()`), `coreId`, `pathKeyHash` (`RomKey::crc32RelativePath(romPath)`), `romSize`, `stateSize`, `ramSize`, `crc32` (IEEE CRC32 over state + RAM payload), and `romTitle[32]`.
2. **Validation Before Restore:** `SaveStateFormat::decodeHeader()` verifies magic, version, header size, `coreId`, `pathKeyHash` (when non-zero), `romSize` (when non-zero), `stateSize <= maxStateSize`, `ramSize <= maxRamSize`, and exact total file byte length before `SaveManager::loadState()` verifies the payload CRC32 in a temporary PSRAM staging buffer.
3. **Legacy `BSS1` Read Support:** `decodeHeader()` still accepts valid `60`-byte `BSS1` headers (`isLegacyV1 = true`) when `coreId`, bounds, file length, and payload CRC32 match, while all new save states are written in `BSS2` format.

## Consequences

- Prevents restoring a save state created for a different ROM path or ROM file size (`HOST_TESTED` in `tests/host/test_storage_formats.cpp` and `tests/test_save_roundtrip.py`; `TARGET_COMPILED`).
- Physical SD card power-loss durability during atomic `.tmp` -> `.bak` -> `.s0N` rotation remains `NOT_TESTED` on hardware.
