# ADR 0008: ROM Policy and Repository Licensing

- Status: Accepted
- Date: 2026-11-13
- Decision owner: User

## Context

The repository originally held tracked commercial ROM header arrays, Archive.org / box-art downloader scripts, and custom C core stubs. The owner directed removing all copyrighted ROMs and scrapers (`BMO_ENABLE_BAKED_ROMS=0` by default), replacing all custom stubs with genuine upstream open-source emulators across all 15 console platforms (`BMO_EXPERIMENTAL_CORES=1`, `BMO_ENABLE_DOOM=1`), and aligning the combined distribution license with the bundled upstream copyleft cores.

## Decision

1. Track zero copyrighted ROM headers, scraped artwork, or remote ROM/art downloader scripts; keep `BMO_ENABLE_BAKED_ROMS=0` by default and remove legacy commercial blobs from active history upon owner sign-off (`scripts/purge_history_paths.txt`).
2. Keep genuine third-party engines (`firmware/BmoGameboy/src/third_party/`) enabled by default (`BMO_EXPERIMENTAL_CORES=1`, `BMO_ENABLE_DOOM=1`) and distribute combined firmware builds under `GPL-2.0-or-later` (`LICENSE`), backed by pinned URLs, commit SHAs, and modification logs in `THIRD_PARTY_NOTICES.md` and per-core `PROVENANCE.md` files.
3. Preserve permissive opt-out compilation flags (`-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0`) for an MIT-compatible GB/GBC/NES-only firmware build; standalone first-party utilities remain permissive MIT.

## Consequences

Clean clones compile without external secrets or copyrighted ROM blobs, and binary distributors have an explicit, accurate license and third-party provenance inventory.
