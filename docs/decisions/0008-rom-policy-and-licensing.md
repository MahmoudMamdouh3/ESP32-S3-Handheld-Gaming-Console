# ADR 0008: ROM Policy, Provenance, and Build Profile Licensing

- Status: Partially Accepted (Build profile split accepted; First-Party License Policy **Pending Owner Decision 9**)
- Date: 2026-11-13 (Updated: 2026-11-15, reconciled on `techlead/audit-2`)
- Decision owner: User

## Context

1. The repository originally held tracked commercial ROM header arrays and remote artwork scraper scripts, which were removed (`BMO_ENABLE_BAKED_ROMS=0` by default) and purged from git history (`0` blobs `>= 1 MiB` across all refs).
2. All 15 console platforms are backed by 13 genuine upstream emulators under `firmware/BmoGameboy/src/third_party/`.
3. A legal audit of the 13 vendored upstream engines (`THIRD_PARTY_NOTICES.md`) established that while `Peanut-GB`, `Walnut-CGB`, and `agnes` are `MIT` and `FAKE-08`/`Handy` are permissive (`MIT`/`Zlib`/`WTFPL`), the experimental core set combines `GPL-2.0-only` (`oswan`), `GPL-2.0-or-later` (`doom`, `race`, `stella2014`, `huexpress`, `smsplus`), `GPL-3.0` (`gwenesis`), and **three non-commercial clauses** (`snes9x/LICENSE`, MAME Z80 v2.7 in `smsplus/z80.c`, and Musashi v3.32 in `gwenesis/cpus/M68K/m68k.h`).
4. Under GNU GPL v2 Section 6 and GPL v3 Section 7, a static binary combining GPL code with non-commercial-only code cannot be publicly redistributed as a precompiled binary, although users may compile it locally from source for personal use.

## Decision

1. **Zero Commercial ROMs or Scrapers:** Track zero copyrighted ROM headers, BIOS images, scraped artwork, or remote downloader scripts (`BMO_ENABLE_BAKED_ROMS=0` enforced by `config.h`, `.githooks/pre-commit`, and `validate_repo.py`).
2. **First-Party License Policy (Pending Owner Decision 9):** Root [`LICENSE`](../../LICENSE) is GNU GPL v2 (`GPL-2.0`). Automated `MIT OR GPL-2.0-or-later` SPDX headers stamped across first-party files in commit `addcc07` lacked explicit owner approval and improperly labeled core wrappers coupled to GPL/non-commercial engines. Selection of first-party licensing (`MIT OR GPL-2.0-or-later` for purely original code only vs `GPL-2.0`) is deferred to [`docs/owner-decisions.md`](../owner-decisions.md) Decision 9; derivative/coupled wrappers (`emu_*` + `ui/display_worker.cpp`) must follow their coupled upstream licenses.
3. **Separate Personal-Use vs Redistributable Build Profiles:**
   - **`mit` profile (`-DBMO_EXPERIMENTAL_CORES=0 -DBMO_ENABLE_DOOM=0`):** Compiles only `MIT`-licensed cores (`Peanut-GB`, `Walnut-CGB`, `agnes`) plus Espressif/Adafruit libraries. This is the **only** profile whose compiled binary is uploaded or published in CI / GitHub Releases (`.github/workflows/ci.yml`, `.github/workflows/release.yml`).
   - **`default` and `no-doom` profiles (`BMO_EXPERIMENTAL_CORES=1`):** Compiled in CI strictly to verify `TARGET_COMPILED` build health and memory budgets, and built locally by the device owner for personal use on their own hardware, but **never** published as precompiled release binaries unless incompatible cores are replaced or relicensed (see `docs/owner-decisions.md`).

## Consequences

- Clean clones compile all profiles without copyrighted ROMs or secrets.
- CI and GitHub Releases never distribute binaries that violate GPL/non-commercial license compatibility.
