# SDD Section 12: Public declarations and symbol references

[Back to the SDD index](../software-design-document.md)

## 12.1 Declaration source of truth

Firmware declarations live in the headers beside their implementation. Exact public function, type, and constant signatures are extracted into [the generated symbol index](../generated/symbols.md), with pages grouped by core service, emulator wrapper, and engine headers. Use a live header when an edit changes an API; a generated page is a navigation aid and can lag until regenerated.

The main groups are:

- `src/core/config.h`: GPIO numbers, dimensions, feature flags, and logging macros.
- `src/core/buttons.h`, `display_emu.h`, and `spi_arbiter.h`: input, screen output, and shared bus services.
- `src/core/sd_card.h`, `save_manager.h`, `box_art.h`, and `battery.h`: catalog, persistent files, art loading, and battery UI interface.
- `src/emulators/*.h`: wrapper lifecycle and per-core operations. Some use `begin()` and others `init()`; the main dispatcher adapts to those existing names.

## 12.2 API change procedure

1. Read the declaration and all call sites in the current source.
2. Update the implementation, callers, host fixtures, and any compile-time checks that depend on the interface.
3. Run `python -m tools.guardian symbols` and inspect the generated diff.
4. Run the target compile and repository checks. The generated symbol listing does not prove that all call sites compile.

Do not copy signatures into a second hand-maintained catalog. Rule 10 is the maintenance pointer for this generated index.
