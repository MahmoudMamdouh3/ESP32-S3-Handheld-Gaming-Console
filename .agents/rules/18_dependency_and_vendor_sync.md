# Dependency & Vendor-Library Sync

## Vendor Patching
- Any local modification to a `src/vendor/<name>/` file gets an inline `// BMO-PATCH: <one-line reason>` comment at the exact changed line(s) — not just a commit message. (Note: Currently no `BMO-PATCH` tags exist in the repo. Any future local modifications must use this).
- This ensures a future upstream sync can grep `BMO-PATCH` and know exactly what to re-check, instead of diffing the whole file from scratch.
- Never reformat a vendor file wholesale ("cleaning it up") — that destroys upstream diffability. Vendor files stay byte-for-byte upstream except `BMO-PATCH`-marked lines, operationalizing `03_conventions.md`'s existing "as close to upstream as possible" rule.

## Syncing Upstream
Before syncing a vendor library to a newer upstream version: 
1. Grep that directory for `BMO-PATCH` first.
2. List every patch found.
3. Confirm each is still necessary, superseded, or needs re-applying.
4. State this explicitly in the commit body.

## Version Pinning
- Pin exact versions for every vendor/Arduino-library dependency in `02_architecture.md`'s toolchain table (already done for the ST7789 and SD libraries).
- Extend this to `peanut_gb`, `walnut_cgb`, `agnes`, `doomgeneric` with an exact commit hash or release tag, not "latest," matching the existing Zig-pinning rationale.

## Patch Ledger
- Maintain a short patch ledger (a table here, or a linked `docs/VENDOR_PATCHES.md` if it outgrows this file's char budget): file, line, one-line reason, date added.

| File | Line | Reason | Date Added | Status |
|---|---|---|---|---|
| `src/emulators/emu_walnut.cpp` | ~L84, ~L90 | Replaced unsafe unaligned pointer casts with byte-wise little-endian reconstruction | 2026-08-29 | ACTIVE |
| `src/engine/walnut_cgb/walnut_cgb.h` | ~L67 | Enabled 16-bit fast paths (DUALFETCH + 16BIT_OPS) | 2026-08-29 | **REVERTED 2026-08-30** -- caused Mario Deluxe GBC freeze (INC-3). See `24_vendor_flag_safety.md`. |
| `src/vendor/doom/src/doomgeneric.c` | ~27 | Size the indexed framebuffer using its pixel type instead of four bytes per pixel | 2026-10-21 | ACTIVE |
| `src/vendor/doom/src/p_mobj.c` | ~646, ~803 | Stop before indexing the object table for unknown map thing types | 2026-10-21 | ACTIVE |
| `src/vendor/doom/src/w_checksum.c` | ~48 | Preserve the old file list and report a fatal error if realloc fails | 2026-10-21 | ACTIVE |

## Vendor flag changes require 24_vendor_flag_safety.md compliance
Any change to a `#define` value in `src/engine/*/` or `src/vendor/*/`
that is NOT a whitespace/comment-only change must follow the protocol in
`24_vendor_flag_safety.md` before being committed. Add a row to this
table when you make such a change, and update its Status if reverted.
