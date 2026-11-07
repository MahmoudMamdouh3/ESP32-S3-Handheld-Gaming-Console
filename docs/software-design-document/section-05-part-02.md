# SDD Section 05.5: State Input and Transitions

[Back to the SDD index](../software-design-document.md) · [Previous: boot and task flow](section-05-part-01.md) · [Next: gameplay and persistence](section-05-part-03.md)

This chapter summarizes behavior in `BmoGameboy.ino`. Exact button edge/debounce semantics are implemented by `Buttons`; exact screen copy is implemented by `DisplayEmu` and may change independently.

## Boot splash and console carousel

- `STATE_BOOT_SPLASH` displays the splash and transitions after roughly 3.5 seconds or when an accepted button press is detected.
- `STATE_CONSOLE_MENU` exposes a Favorites entry plus GB, GBC, NES, DOOM, SMS, Game Gear, PC Engine, Atari, PICO-8, Genesis, SNES, WonderSwan, Neo Geo Pocket, Lynx, and Coleco/SG-1000 categories.
- Left/right change the selected category. A opens its game list when the visible count is nonzero. Down or Select opens the console information view. Start opens Diagnostics. Up enters the idle face mode. The 30-second idle timeout also enters the face mode.
- The list is defined by `CONSOLES[]`; the index is not an emulator wrapper index. Wrapper indices are selected only after a ROM's `RomType` is mapped during launch.

## Game list and launch

- Up/down move within visible titles. Left/right perform an alphabetical initial-letter jump.
- Start toggles the Top Picks filter when not in Favorites.
- Select toggles the selected title's favorite flag. On the Favorites list, the visible set is rebuilt so removal hides the entry.
- B returns to the carousel and resets transient list filters.
- A resolves the selected `RomFile`, unloads artwork, and launches by ROM type. DOOM WAD is opened by its wrapper from the SD VFS path; other ROMs are copied to PSRAM by `SDCard::loadRom()`.
- After a successful launch the filename and buffer size are retained for save paths and battery-header checks. The launch path attempts battery-RAM load only for routed core types, resets timing counters, then sets `STATE_EMULATOR`.

## Console information and idle face

- The console information view is closed with A, B, or Select.
- The idle face view accepts face/input interactions and returns to the console carousel through its Start/B exit behavior. Its expression and motion implementation belongs to `BmoFace`; it does not run during active gameplay or the pause overlay.

## 5.5 Diagnostics

- Start from the console carousel enters Diagnostics. B returns to the carousel.
- A toggles the SPI stress pattern. While the pattern is active, left/right toggles the display clock between the configured 80 MHz default and the 40 MHz comparison value. Leaving the pattern restores the configured default.
- Select, while the stress pattern is inactive, retries SD mounting when the card is absent; after a successful mount it initializes `SaveManager`. When mounted, Select runs `SDCard::rebuildIndex()` and displays a rescan result message.
- The canonical dashboard readouts are SD mount state and nominal divider-derived clock, free internal 8-bit memory, free PSRAM, largest free PSRAM block, free executable memory, the 12-button bitmask, skipped ROM paths, and loop/display task stack high-water marks. High-water values are reported in words, not bytes. The dashboard also supports the SPI stress pattern and configured/fallback clock toggle. A serial stack line is logged every five seconds while Diagnostics is open. The display clock label reports the configured default and marks board behavior unmeasured; see [Section 07.1](section-07.md#71-panel-configuration-and-published-timing).

The display clock comparison and all diagnostic numbers are configured/runtime readings. They do not constitute electrical measurement or pass/fail qualification without a written hardware record.
