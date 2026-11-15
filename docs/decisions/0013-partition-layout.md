# ADR 0013: Flash Partition Layout and Coredump Proposal

- Status: Proposed (Hard Stop — `firmware/BmoGameboy/partitions.csv` unchanged pending owner approval)
- Date: 2026-11-13 (Updated: 2026-11-15, verified at `d8d4b5e`)
- Source: `firmware/BmoGameboy/partitions.csv`, `firmware/BmoGameboy/sketch.yaml`

## Context

`firmware/BmoGameboy/partitions.csv` defines the following layout across the 16 MiB (`0x1000000` B) Quad SPI flash on the `ESP32-S3-WROOM-1-N16R8`:

| Name | Type | SubType | Offset | Size | Current Usage |
| :--- | :--- | :--- | ---: | ---: | :--- |
| `nvs` | `data` | `nvs` | `0x009000` | `20 KiB` (`0x005000`) | Active (`bmo_boot` safe-mode counter, `bmo_disp` SPI Hz, `bmo_ui` theme) |
| `otadata` | `data` | `ota` | `0x00E000` | `8 KiB` (`0x002000`) | Required by ESP-IDF bootloader for `ota_0` boot selection, though no `app1` OTA slot exists |
| `app0` | `app` | `ota_0` | `0x010000` | `8,192 KiB` (`0x800000`) | Active firmware image (`3,293,225 B` / `39.3%` used in default 15-platform build) |
| `ffat` | `data` | `fat` | `0x810000` | `8,064 KiB` (`0x7E0000`) | **Unused** (zero `FFat` mount or file calls in firmware; all ROMs/saves reside on MicroSD) |
| *(unallocated tail)* | — | — | `0xFF0000` | `64 KiB` (`0x010000`) | Unpartitioned tail at end of 16 MiB flash |

Additionally, there is no `coredump` (`data, coredump`) partition, which prevents ESP-IDF from saving post-mortem panic backtraces to flash when the handheld is running on battery without a USB serial monitor attached.

## Proposed Options (Requires Owner Approval)

- **Option A (Minimal Non-Invasive Coredump Addition — Recommended):** Keep `nvs`, `otadata`, `app0` (`8 MiB`), and `ffat` at their exact existing offsets, and allocate the currently unpartitioned `64 KiB` tail (`0xFF0000`, size `0x010000`) as `coredump, data, coredump, 0xFF0000, 0x010000`.
- **Option B (Remove Unused `ffat` + Add `coredump`):** Reclaim or shrink `ffat` if internal flash storage is not planned, or keep `ffat` for future built-in homebrew/assets when no MicroSD card is inserted.
- **Option C (Status Quo):** Leave `partitions.csv` untouched.

## Decision

Preserve `firmware/BmoGameboy/partitions.csv` byte-for-byte until the owner selects an option in `docs/owner-decisions.md`.
