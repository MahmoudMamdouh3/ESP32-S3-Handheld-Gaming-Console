# ADR 0003: ROM Metadata Keys and Catalog Index v2

- Status: Accepted
- Date: 2026-11-07
- Source: `path_key.h`, `rom_index.h`, `sd_card.cpp`, `scripts/rom_key.py`

## Context

The catalog contains many repeated basenames and case-insensitive prefix collisions. Basename-only artwork/save paths can map distinct ROM files to one sidecar. The previous catalog index did not include file sizes or a catalog fingerprint.

## Decision

Generate 32-character lowercase hexadecimal CRC32 keys from the complete normalized relative ROM path. Firmware and deployment tooling share the algorithm; old basename-keyed data is copied to the new key without deleting the old file. Use `BMOIDX02` entries with a 64-byte filename, one-byte type/flags, and 32-bit file size. Validate index structure, entry values, aggregate sizes, content CRC, root timestamp, and catalog fingerprint. Write through temporary and backup paths and recover interrupted rename transactions.

## Consequences

CRC32 makes accidental collisions unlikely but is not cryptographic identity. The measured `games/` tree has many prefix and basename collisions, which is why full-path keys are required. FAT root timestamps may be unavailable or coarse; a zero timestamp disables cache reuse. Migration, timestamp semantics, and power-loss recovery still need confirmation on the actual card.
