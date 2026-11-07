# SDD Section 16: Vendored engines, provenance, and licensing

[Back to the SDD index](../software-design-document.md)

## 16.1 Source ownership

The firmware uses first-party adapters under `firmware/BmoGameboy/src/emulators/`, first-party engine code under `src/engine/`, and imported or adapted code under `src/vendor/`. Current directories include Agnes, Coleco, DOOM, Genesis, Lynx, Neo Geo Pocket, Peanut-GB, PC Engine, PICO-8-style runtime, SMS Plus, SNES, Stella, and WonderSwan. Walnut-CGB is in `src/engine/walnut_cgb/`.

A directory name or local patch does not establish upstream version, author, license grant, or redistribution rights. For each vendor component, inspect its own headers, license file, package history, and local modifications before distribution. The repository-wide commercial ROM policy and project license remain unanswered; the user said they will provide a separate policy/license. Do not add ROM files or change the root license in anticipation of that input.

## 16.2 Update discipline

Before updating a vendor engine, record its upstream URL and immutable revision, compare the patch against the local tree, preserve required local hardware hooks, and update attribution/notices only from source evidence. Run focused native fixtures where supported, then the target build. The current native runner covers selected C engines; it is not a conformance suite for all platforms.

## 16.3 ROM-bearing data

`firmware/BmoGameboy/src/assets/roms/` contains tracked firmware-resident game arrays; `firmware/BmoGameboy/src/assets/bmo_assets.h` contains other firmware artwork data. Treat the existing files as user-governed content. This documentation change did not modify those files. The CI workflow builds without downloading ROMs.
