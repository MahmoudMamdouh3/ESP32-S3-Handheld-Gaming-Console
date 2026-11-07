# Architecture Decision Records

These records explain decisions that affect hardware, firmware behavior, storage formats, or project policy. An ADR describes the reason and evidence; the active source remains authoritative for the implemented value. `Accepted` means the repository behavior is selected. `Proposed` means the direction is recorded but implementation or human review is still required. `Open` means no decision was made.

| ADR | Status | Decision |
| :--- | :--- | :--- |
| [0001](0001-hardware-configuration.md) | Accepted | Keep the soldered ESP32-S3 pin map and safety feature flags as hard stops. |
| [0002](0002-shared-spi-bus.md) | Accepted | Display and SD share SPI; all transactions pass through `SpiArbiter`. |
| [0003](0003-rom-keys-and-index-v2.md) | Accepted | Use case-preserving CRC32 path keys for app metadata and a versioned SD catalog index. |
| [0004](0004-battery-persistence.md) | Accepted | Persist battery-backed RAM only when ROM metadata says it exists. |
| [0005](0005-psram-and-doom-budget.md) | Accepted | Size catalog and DOOM allocations from measured free/largest PSRAM at runtime with fixed bounds. |
| [0006](0006-portable-power.md) | Proposed | Plan for 3×AA NiMH cells and a boost converter; exact electrical design is unselected. |
| [0007](0007-fram-wiring.md) | Proposed | Keep GPIO43/44 as FRAM proposal; no physical wiring or persistence claim. |
| [0008](0008-rom-policy-and-licensing.md) | Open | Await the user's separate ROM policy/license direction. |
| [0009](0009-emulator-exit-confirmation.md) | Accepted | `SELECT+UP` opens the quit action; two deliberate A presses are needed to exit. |
| [0010](0010-simulator-boundary.md) | Accepted | Browser/Wokwi simulation and firmware behavior are separate execution environments. |
| [0011](0011-index-path-storage.md) | Proposed | Compare larger fixed paths, string-pool offsets, and hashed identities before changing the catalog format. |
| [0012](0012-save-state-schema.md) | Proposed | Add core schema and ROM identity to a future BSS2 format with an explicit BSS1 policy. |
| [0013](0013-partition-layout.md) | Proposed | Preserve the current one-slot partition layout until the user selects an OTA/FFat policy. |
| [0014](0014-display-spi-clock.md) | Proposed | Keep the clock default pending hardware measurement and user review. |

New records should state the status, context, decision, alternatives, consequences, evidence, and any human approval still required.
