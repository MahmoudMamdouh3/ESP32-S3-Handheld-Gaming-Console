# 32. Emulator Wrapper Template

This template is a checklist, not a second API contract. Read 12_extensibility_contract.md, 26_emulator_exit_contract.md, config.h, and the generated symbol index before adding a wrapper.

## Integration checklist

1. Identify the engine license, source revision, supported image format, and limits. Keep third-party data and ROM policy separate.
2. Add the engine under src/vendor/ when preserved as imported code or src/engine/ when project-owned/adapted. Record attribution and local patches.
3. Implement a wrapper beside the other emu_*.h/.cpp files using the existing source style. Do not require one universal method spelling; the current wrappers use both begin() and init().
4. Register the file extension in sd_card.cpp, the RomType, console selection, launch dispatch, frame path, pause actions, and destruction dispatch in BmoGameboy.ino.
5. Add bounds/header checks before engine parsing. On partial allocation failure, release wrapper-owned resources. Keep the caller-owned ROM buffer under SDCard ownership.
6. Add battery persistence only when the core exposes a mapped nonvolatile region and its image format identifies it. Otherwise return false and show no battery support. Save states are separate and need stable core IDs and pointer-safe restore logic.
7. Match frame dimensions, pixel encoding, and display method to display_emu.*; balance all bus/frame ownership pairs.
8. Add host fixtures for supported input and failure cases. Do not label the engine CPU-conformant without a standard test suite and recorded results.
9. Wire destroy() into confirmed session exit and ensure it is idempotent.
10. Update the SDD, issue ledger, manifest evidence, symbols, and changelog. Run the target compile, repository tests, Guardian checks, and diff audit.

## Design reminders

- Large buffers use PSRAM and every allocation is checked.
- Per-frame code performs no heap allocation.
- Button input comes from Buttons, not repeated digitalRead() calls.
- The physical pin map and partition table are fixed unless the owner changes the hardware plan.
- Never claim full emulation support from a successful build or a synthetic-frame smoke test.
