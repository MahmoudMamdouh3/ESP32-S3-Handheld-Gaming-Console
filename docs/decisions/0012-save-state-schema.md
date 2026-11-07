# ADR 0012: Save-State Schema and ROM Identity

- Status: Proposed
- Context: BSS1 uses a 60-byte `SaveStateHeader`: magic, version, `millis()` timestamp, core ID, state size, RAM size, payload CRC32, and a 32-byte ROM title. It has no per-core schema identifier or ROM-content CRC. CRC32 protects payload integrity, not compatibility or authenticity.
- Decision: Do not change the BSS1 layout in this remediation. Propose BSS2 with an explicit core-state schema ID and a ROM identity field derived from the loaded image. A future loader must reject BSS1 or require a clear user warning before restoring it.
- Alternatives: Continue with BSS1's core ID, payload lengths, title, and CRC checks; reject all existing states on firmware update; implement an additive BSS2 version with explicit legacy policy.
- Consequences: BSS1 may accept a state from a different ROM revision at the same path or a state whose struct layout changed without changing size. Current timestamp is `millis()` since boot, not wall-clock time; the UI does not display it as calendar time.
- Evidence: `firmware/BmoGameboy/src/core/save_manager.h` and `save_manager.cpp`. State bytes are staged and integrity-checked before copying into the live emulator buffer; schema and ROM identity remain absent.
- Human input: Required before selecting BSS1 legacy handling and the BSS2 migration policy.
