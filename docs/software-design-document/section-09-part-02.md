# SDD Section 09: Emulator lifecycle and persistence (part 2)

[Back to the SDD index](../software-design-document.md)

## 9.4 Wrapper lifecycle

ROM launch is dispatched in `BmoGameboy.ino`. Wrappers have different entry names (`begin()` or `init()`); do not normalize those names in documentation. On launch failure, the caller releases the ROM buffer and returns to the game menu. During normal exit, the caller waits for outstanding display work, saves supported battery RAM, invokes the active wrapper's `destroy()` path, releases the loaded ROM, and clears session state. Read the live dispatcher before changing exit behavior.

`destroy()` must tolerate repeated calls and release wrapper-owned heap allocations. ROM buffers returned by `SDCard::loadRom()` remain caller-owned and are released by `SDCard::freeRom()`. A board compile and repeated-launch test are still needed to validate every failure branch and allocation lifecycle on target.

## 9.5 Battery-backed storage as implemented

The current code persists only the wrapper regions listed below. “Supported” describes source gating and path routing, not a completed physical reset or power-loss test.

| Wrapper | Current `.sav` source region | Gate in current source |
| :--- | :--- | :--- |
| Walnut-CGB, Peanut-GB | Cartridge RAM buffer | Cartridge type and RAM-size fields must map to a supported battery cartridge and capacity |
| NES / Agnes | PRG RAM | iNES battery flag must be set and the core must expose PRG RAM |
| PC Engine | PCE BRAM | Current launcher treats the system BRAM as backup memory |
| SNES | SRAM | Current wrapper derives a size from the LoROM header field at the copier-header-adjusted offset |
| SMS/Game Gear, Atari, PICO-8, Genesis, WonderSwan, NGP, Lynx, Coleco, DOOM | None | Wrapper battery save/load returns false or the wrapper has no battery-save API |

The portable `BatterySave` header functions are host-tested for Game Boy, NES, and SNES field mapping. The emulator state and SD persistence path are not covered by a full host round trip. Unsupported games must not be presented as having a battery save.

## 9.6 Save states

Save-state support is wrapper-specific. `SaveManager` owns the common file envelope, slot naming, CRC checks, and path keys. Some wrappers serialize engine state directly; others stage a state copy and restore pointers or separately allocated RAM after loading. `SaveManager` validates file bounds and checks the expected core ID before wrapper state is accepted. Consult each wrapper's `saveState()` and `loadState()` source before modifying a layout; never treat raw C structs as portable across firmware versions.

Per-game files use the shared relative-path-derived key. Legacy basename files are read and copied to the keyed destination after the new file has passed size/CRC verification; the old path is retained. Details and migration limits are in [Section 10](section-10.md).
