# SDD Section 09: Exit sequence and unresolved coverage (part 3)

[Back to the SDD index](../software-design-document.md)

## 9.7 Exit sequence

The emulator exit chord opens the pause menu with Quit selected. The user must press A twice within three seconds to confirm. B or Start resumes; directional or Select input cancels the armed action. The ordinary Quit row uses the same confirmation. On confirmation, supported battery RAM is saved before wrapper teardown, the display worker is drained, the wrapper is destroyed, and the caller-owned ROM buffer is released. This is source-verified; physical button behavior has not been tested on the board.

The interaction is recorded in [ADR 0009](../decisions/0009-emulator-exit-confirmation.md). The list of actual wrapper symbols and signatures is generated from headers in [the symbol index](../generated/symbols.md).

## 9.8 Coverage limits

The native host runner exercises a selected subset of C engines with synthetic input. It does not compile the complete ESP32 firmware and does not prove CPU instruction conformance, timing accuracy, cartridge coverage, battery persistence, or compatibility with the user's library. Browser WASM builds cover only their explicit adapter list. No standard per-console CPU conformance suite or physical console-to-panel comparison is recorded in this repository audit.
