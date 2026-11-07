# SDD Section 04: Repository map and ownership

[Back to the SDD index](../software-design-document.md)

## Purpose

This chapter maps active source ownership. It deliberately avoids a line-count inventory: generated statistics become stale as soon as files change. Use `rg --files` for a live file list and the generated symbol index for declarations.

## Top-level files

| Path | Owner / purpose |
| :--- | :--- |
| `AGENTS.md` | Repository entry instructions and required checks |
| `AGENT_MANIFEST.json` | Machine-readable platform, wrapper, and rule metadata; verify generated counts before use |
| `AGENT_KNOWLEDGE_GRAPH.json`, `AGENT_DECISION_TREE.json` | Generated Guardian indexes; do not edit by hand |
| `CHANGELOG.md` | Current user-visible engineering changes and verification references |
| `DESIGN.md` | UI colors, geometry, typography, and interaction constraints |
| `README.md` | Quick entry point; detail pages live in `docs/readme/` |
| `diagram.json`, `wokwi.toml` | Wokwi circuit and firmware configuration; neither proves physical wiring |

## Firmware (`firmware/BmoGameboy/`)

- `BmoGameboy.ino`: Arduino setup, main loop, launcher state machine, emulator dispatch, input chords, and session teardown.
- `partitions.csv`: custom N16 flash layout. The board FQBN and QIO/OPI selections are described in Section 02.
- `src/core/`: hardware-facing services and shared UI. `config.h` owns pin and feature configuration; `display_emu.*` owns the TFT and display worker; `sd_card.*` owns media discovery and catalog metadata; `save_manager.*` owns save paths, formats, CRC checks, and staged writes; `spi_arbiter.*` protects shared SPI; `theme.h` owns firmware colors.
- `src/emulators/`: fourteen wrapper implementations. SMS and Game Gear share `emu_sms.*`; launcher indices identify fifteen platform selections.
- `src/engine/`: first-party engine code not housed under `vendor/`, including Walnut-CGB.
- `src/vendor/`: imported or adapted CPU/console engines. Attribution and provenance are tracked in Section 16 and the relevant upstream files.
- `src/assets/`: firmware-resident data and baked ROM arrays. Treat as ROM-bearing content; do not add, fetch, or redistribute game data without the user's separate policy.
- `src/tests/`: firmware-side test code; host tests under root `tests/` are separate from board execution.

## Tools and checks

| Path | Responsibility |
| :--- | :--- |
| `tools/guardian/` | Static checks, repository indexes, symbol extraction, budgets, UI checks, and CLI entry point |
| `tools/bmo_simulator/` | Browser UI, local-only HTTP server, WebAssembly playtest adapters, and 3D visualization |
| `tools/host_cores_runner.c` | Native harness for the selected host-portable C engines; it is not a firmware build |
| `tools/telemetry_plotter.py` | Parses supported serial frame timing records |
| `scripts/validate_repo.py` | Repository phases, including optional Arduino compile gate |
| `scripts/check_markdown_links.py` | Checks local Markdown links and anchors |
| `scripts/verify_color_tokens.py` | Verifies firmware, design, and browser UI color-token consistency |
| `scripts/bundle_rules.py` | Regenerates consolidated rule views |
| `scripts/build_wasm_cores.py` | Rebuilds tracked WASM outputs from available toolchains |
| `scripts/deploy_to_sd.py`, `scripts/fetch_covers.py`, `scripts/rom_key.py` | SD deployment and artwork utilities using shared ROM-key rules |
| `tests/`, `tests/host/` | Python test orchestration and small C/C++ host-contract fixtures |

## Documentation and generated artifacts

- `docs/software-design-document.md` is the SDD entry and chapter index; its sections describe source behavior and evidence boundaries.
- `docs/decisions/` records decisions and unresolved choices.
- `docs/hardware_measurements.md` and its templates define human-collected device evidence; blank templates are not measurements.
- `docs/generated/` is generated output. Regenerate through the owning Guardian command instead of hand-editing.
- `docs/archive/` contains dated snapshots. Historical claims there describe the repository at the time of writing and may conflict with current source.
- `.agents/rules/` contains current agent constraints and handoff state. `docs/consolidated-rules.md` and `docs/consolidated-rules/` are generated views of those files.

## Evidence boundaries

Source constants and host tests show intended code behavior. A target compile that meets the conditions in [Section 1.3](section-01.md#13-truth-labels) shows compilation and link behavior. Neither shows electrical behavior, panel timing, power stability, or emulator compatibility with the user's whole ROM collection.
