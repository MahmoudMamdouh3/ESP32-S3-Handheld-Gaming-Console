# BMO Gameboy Software Design Document

This is the repository's architecture guide and chapter index. It describes the present code, its external data boundaries, and the evidence available for each claim. It is not a substitute for live headers or the hardware record: source configuration proves what firmware requests, while only a recorded measurement proves what the soldered device does.

## How to use this document

1. Start with [scope and truth labels](software-design-document/section-01.md), then read [hardware constraints](software-design-document/section-02.md) before changing firmware.
2. Use [the GPIO matrix](software-design-document/section-03.md) and [repository map](software-design-document/section-04.md) to find ownership and interfaces.
3. Follow the chapters on [boot and state flow](software-design-document/section-05-part-01.md), [memory](software-design-document/section-06.md), [display](software-design-document/section-07.md), [emulators](software-design-document/section-09-part-01.md), and [storage](software-design-document/section-10.md) before altering those subsystems.
4. Check [known issues](../.agents/rules/04_known_issues.md), [decision records](decisions/README.md), and the [hardware measurement procedure](hardware_measurements.md) for current gaps.
5. Use [generated symbol references](generated/symbols.md) for exact declarations. Generated pages come from source headers and are not an independent API contract.

## System at a glance

| Layer | Role | Source of truth |
| :--- | :--- | :--- |
| ESP32-S3 firmware | Launcher, input, display, storage, emulator lifecycles, diagnostics | `firmware/BmoGameboy/BmoGameboy.ino`, `src/core/`, `src/emulators/` |
| Emulator engines | CPU/system emulation and per-core memory/state | `firmware/BmoGameboy/src/vendor/`, `src/engine/`, wrapper headers and source |
| Game data | External SD library under `games/`; baked assets in firmware source | `sd_card.cpp`, tracked ROM headers, user-owned card contents |
| Browser simulator | Local web server, WebGL console, browser input/storage, selected WASM cores | `tools/bmo_simulator/`, `scripts/build_wasm_cores.py` |
| Wokwi model | Circuit representation for the parts listed in `diagram.json` | `diagram.json`, `wokwi.toml` |
| Host verification | Python checks and selected native C/C++ runners | `tests/`, `tests/host/`, `tools/host_cores_runner.c` |
| Repository checks | Rule, source, UI, symbol, PSRAM, link, and build validation | `tools/guardian/`, `scripts/validate_repo.py` |

The console carousel has 16 entries: Favorites plus 15 console types. Firmware routes those console types through 14 wrappers because SMS and Game Gear share one wrapper. Browser WASM and native host coverage are subsets of firmware; they do not execute the full ESP32 image.

## Build and validation entry points

The canonical target is ESP32-S3 with `FlashMode=qio`, `FlashSize=16M`, the repository custom partition table, and `PSRAM=opi`. The exact FQBN and reproducible build steps are in [Section 02](software-design-document/section-02.md) and [Section 14](software-design-document/section-14.md). The committed profile pins Arduino-ESP32 3.3.11 and its libraries for both local and CI builds. A local result is valid only when the command actually completes in the current session.

```powershell
python -m unittest discover -s tests -v
python scripts/validate_repo.py
python -m tools.guardian verify
python -m tools.guardian ui-audit
python -m tools.guardian diff-audit
```

See [Section 14](software-design-document/section-14.md) for each check's scope and failure boundaries. Do not upload firmware to a physical board as part of repository validation.

## Chapters

1. [Scope, runtime environments, evidence labels, and feature truth](software-design-document/section-01.md)
2. [Hardware state, wiring, and safety constraints](software-design-document/section-02.md)
3. [GPIO assignments and feature flags](software-design-document/section-03.md)
4. [Repository directory and ownership map](software-design-document/section-04.md)
5. [Boot sequence, main loop, state machine, and user input](software-design-document/section-05-part-01.md), [Diagnostics](software-design-document/section-05-part-02.md), and [gameplay, pause, pacing, and teardown](software-design-document/section-05-part-03.md)
6. [Memory pools, flash partitions, and allocation rules](software-design-document/section-06.md)
7. [Display pipeline, SPI sharing, color representation, and pacing](software-design-document/section-07.md)
8. [BMO face renderer](software-design-document/section-08.md)
9. [Emulator wrapper and engine contracts](software-design-document/section-09-part-01.md), [per-core interfaces](software-design-document/section-09-part-02.md), and [launch, lifecycle, and compatibility](software-design-document/section-09-part-03.md)
10. [ROM catalog, SD index, artwork, saves, and migration](software-design-document/section-10.md)
11. [Disabled peripherals, diagnostics, and telemetry](software-design-document/section-11.md)
12. [Public declarations and generated symbol index](software-design-document/section-12.md)
13. [Browser simulator, WASM, and Wokwi boundaries](software-design-document/section-13.md)
14. [Host checks, Guardian, target build, and CI](software-design-document/section-14.md)
15. [Performance configuration, measurements, and unresolved limits](software-design-document/section-15.md)
16. [Vendored engines and attribution status](software-design-document/section-16.md)
17. [Guardian commands, agent rules, and documentation maintenance](software-design-document/section-17.md)

## Current decisions and unresolved user input

Architecture decisions are indexed in [`docs/decisions/`](decisions/README.md). The selected 3×AA NiMH plus boost-converter direction is a plan, not a completed circuit. GPIO43/44 remain proposed FRAM pins. The user will provide a separate ROM policy/license direction; no ROM asset or project license was changed for this audit.

## Evidence status

The 2026-11-07 follow-up obtained a fresh target build with the committed profile; the build evidence and scope are recorded in [Section 02](software-design-document/section-02.md), [Section 06](software-design-document/section-06.md), and [the dated handoff](handoffs/2026-11-07-remediation.md). No CI workflow run or physical device measurements are claimed. Current open findings are linked from [the issue ledger](../.agents/rules/04_known_issues.md).
