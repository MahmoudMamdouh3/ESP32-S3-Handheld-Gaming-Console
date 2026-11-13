# Test Fixtures

Optional local test ROM fixtures for emulator tests. Binary ROM files (`*.gb`, `*.gbc`, `*.nes`, etc.) are intentionally excluded by `.gitignore` and never tracked in Git.

## Blargg's Game Boy CPU Instruction Test (`cpu_instrs.gb`)

- **Expected path:** `tests/fixtures/cpu_instrs.gb` (65,536 bytes, header title `CPU_INSTRS`)
- **Author:** Shay Green (`blargg`)
- **Upstream:** `gb-test-roms` (`cpu_instrs`) — public domain / freely redistributable for emulator verification
- **Usage:** Place a local copy at `tests/fixtures/cpu_instrs.gb` when running extended Game Boy CPU validation outside CI. Standard host tests in `tests/` use synthetic in-memory ROM headers and do not require this file.
