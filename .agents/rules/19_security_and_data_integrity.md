# Security & Data Integrity

The installed hardware has no Wi-Fi or Bluetooth feature enabled. SD files remain untrusted input. Keep storage bounds, checksums, and path containment checks in place; do not infer that local media is safe because it is user supplied.

## Save-state and battery files

SaveManager::SaveStateHeader is 60 bytes in the current header. It uses magic BSS1 (0x31535342) and version 1, carries a numeric core ID, state/RAM lengths, and CRC32. Reads verify file size, CRC, and core ID before a wrapper restores state. The raw C/C++ core structures are build-specific and are not portable formats.

Battery saves are allowed only for mapped nonvolatile regions. Current gating covers supported Game Boy cartridge RAM, NES PRG RAM when the iNES battery bit is set, PC Engine system BRAM, and the SNES SRAM size field supported by the current LoROM parser. Other wrappers report no battery save. Hardware power-loss and reboot round trips still need testing.

Writes stage a .tmp file, verify expected size and payload CRC, preserve an old version as .bak, install the new file, and verify the installed copy. On a later access, recovery uses the final/backup/temp state to retain the last verified file. SD rename and power-loss behavior still needs device fault-injection coverage.

## ROM catalog index

/.bmo_index uses BMOIDX02, version 2, a 36-byte header, and 72-byte entries. The loader checks count bounds, file size, path termination, entry flags/type, payload CRC, total file bytes, catalog fingerprint, and root directory timestamp. The cache is rebuilt when it fails validation. Temporary and backup files are recovered before loading. The root timestamp's reliability on the actual SD module has not been measured.

## Path keys and user data

Save, state, screenshot, and artwork names derive from the full relative ROM path through shared C++/Python RomKey logic. New keys are bounded and include a CRC32 suffix. On first use, legacy basename files can be read and copied to the keyed path after verification; the legacy file is retained. No migration deletes user data. Library counts and filename collision scans must report aggregates only, never filenames.

## Browser host endpoint

The optional playtest endpoint binds to loopback, checks the canonical Host/Origin, requires a per-process session token, accepts only its fixed POST action, limits requests, confines static paths to the simulator directory, avoids shell command construction, and uses a bounded subprocess. Host tests cover these checks; do not expose the server on a network interface.
