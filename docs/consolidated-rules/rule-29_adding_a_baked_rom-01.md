# Section 29 — How to Add a Baked ROM

Source: [`29_adding_a_baked_rom.md`](../../.agents/rules/29_adding_a_baked_rom.md).

# How to Add a Baked ROM
Purpose: adding a baked ROM header to sd_card.cpp is the most common
"simple" task that has several non-obvious landmines. This checklist
prevents each of them.

## What "baked ROM" means
A ROM baked as a C array in a `.h` file under `src/assets/roms/`.
The firmware can serve these WITHOUT an SD card. The ROM data lives in
the ESP32-S3 flash .rodata section.

## Pre-flight checklist (do all before touching sd_card.cpp)
This budget check is now a standing invariant, not a one-time step — see `37_rom_governance_and_flash_budget.md`.

1. **Check flash budget.** Run the arduino-cli build and inspect the
   binary size. The `app0` partition is exactly 8MB (8,388,608 bytes).
   The binary with 4 baked 1MB ROMs (mario, zelda, aladdin, lego_racers) is ~4.98MB.
   Each 1MB ROM adds ~1MB to the binary (do not confuse the ~6MB C source text size
   on disk with the actual 1MB compiled binary .rodata array size).
   Formula: `current_binary_size + new_rom_size_bytes < 8,388,608`
   If this does not hold, DO NOT proceed -- the flash will fail silently.

2. **Verify the ROM header.** A valid GBC/GB ROM has:
   - Nintendo logo at 0x0104-0x0133 (specific byte pattern)
   - Header checksum at 0x014D (computed as: x=0; for j in 0x134..0x14C: x=x-rom[j]-1; x&0xFF)
   - CGB flag at 0x0143: 0xC0 or 0x80 = GBC, anything else = DMG only
   Use the Python check: `python -c "import re; data=bytes([int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})', open('foo.h').read())]); print(hex(data[0x143]), hex(data[0x14D])); cs=0; [cs:=cs-data[j]-1 for j in range(0x134,0x14D)]; print('ok' if (cs&0xFF)==data[0x14D] else 'CHECKSUM FAIL')"`

3. **Confirm the rom_size variable.** The .h file must define both:
   - `const uint8_t <name>_rom[] = {...};` (the data array)
   - `const size_t <name>_rom_size = <N>;` (byte count matching the array)
   Both must be present. sd_card.cpp uses the `_rom_size` variable.

4. **Confirm the RomType.** GBC roms (.gbc) use ROM_GBC -> WalnutEmu.
   DMG-only roms (.gb) use ROM_GB -> PeanutEmu. Routing the wrong way
   will not compile-error but will produce wrong colors or crashes.

## Steps to register the ROM
After pre-flight passes, make these three changes in sd_card.cpp:

### 1. Add the include (top of file)
```cpp
#include "../assets/roms/<name>.h"
```

### 2. Register in SDCard::begin() (baked ROM block)
```cpp
strncpy(romList[numRoms].filename, "<Display Name> (Baked).<ext>", 63);
romList[numRoms].type = ROM_GBC;  // or ROM_GB
numRoms++;
```
Keep baked ROMs FIRST in the list (before SD scan). They are always available.

### 3. Add to SDCard::loadRom() (baked ROM dispatch)
```cpp
if (strcmp(filename, "<Display Name> (Baked).<ext>") == 0) {
    *outSize = <name>_rom_size;
    return (uint8_t*)<name>_rom;
}
```

### 4. Add to SDCard::freeRom() (protect from free())
```cpp
if (buffer == <name>_rom /* || buffer == other_baked_rom */) {
    return;  // flash .rodata -- never free
}
```
**CRITICAL:** forgetting this causes a crash when the user exits the game.
`heap_caps_free()` on a .rodata pointer is undefined behavior on ESP32-S3.

## After registration
- Build and check binary size fits in app0 (step 1 above).
- Add the ROM to `25_game_compatibility_ledger.md` with status UNTESTED.
- Update `04_known_issues.md` changelog.

## What NOT to do
- Do NOT #include the ROM header anywhere except sd_card.cpp. The array
  is declared without `static`, so including it in multiple TUs causes
  an ODR violation (multiply-defined symbol at link time).
- Do NOT add ROMs that exceed the flash budget without expanding partitions.csv.
- Do NOT change the partitions.csv without verifying OTA and ffat behavior.