# Section 42 — 42. Quality of Life (QoL), UI/UX & System Roadmap Ledger (2/2)

Source: [`42_quality_of_life_and_roadmap_ledger.md`](../../.agents/rules/42_quality_of_life_and_roadmap_ledger.md).

### 2.5 Developer Experience (DX) & AI Guardian Tools

| Feature / Tool | Status | Description | Target Component |
| :--- | :--- | :--- | :--- |
| **AI Guardian CI Validator** | `[IMPLEMENTED]` | Multi-phase compilation, static AST linting, bus physics, and ROM health checks. | `scripts/validate_repo.py` |
| **Guardian CLI Suite** | `[IMPLEMENTED]` | `audit`, `bus-calc`, `profile-elf`, `bench-host`, `index`, `route`, `lookup`, `doctor`, `explain`, `verify`, `benchmark-ai`. | `tools/guardian/cli.py` |
| **Flash Budget Visualizer (`budget`)** | `[IMPLEMENTED]` | CLI explorer showing `app0` partition breakdown, baked ROMs, and free headroom. | `tools/guardian/cli.py` |
| **Roadmap & QoL Explorer (`qol`)** | `[IMPLEMENTED]` | CLI command to query active, in-progress, and backlog features directly from terminal. | `tools/guardian/cli.py` |
| **Host CPU C++ Verification Harness** | `[IMPLEMENTED]` | Native host C/C++ test harnesses (`tools/host_test.cpp` & `tools/host_cores_runner.c`) for desktop execution without hardware. | `tools/host_test.cpp`, `tools/host_cores_runner.c` |
| **AI-First Engineering Workflow** | `[IMPLEMENTED]` | Quantitative AI workflow benchmark (`guardian benchmark-ai`) measuring routing accuracy, pin parity, symbol grounding, and core coverage (`100%`). | `tools/guardian/`, `.agents/rules/`, `tests/` |
| **Headless Visual Regression Testing** | `[IMPLEMENTED]` | Native x64 C compilation & 50-frame FNV-1a framebuffer hash verification across all 10 custom C cores (`tools/host_cores_runner.c`). | `tools/host_cores_runner.c`, `tests/test_native_cores_execution.py` |
| **Real-Time Serial Telemetry GUI** | `[IMPLEMENTED]` | Serial telemetry parser, p50/p95/p99 jitter analyzer, and ASCII/JSON plotter (`tools/telemetry_plotter.py`). | `tools/telemetry_plotter.py` |

#### AI Environment Backlog: Evidence-Driven Work for Limited-Context Models

1. Audit routing, symbol lookup, generated-index freshness, mandatory-rule discovery, and test-command selection against representative tasks.
2. Audit developer utilities for bounded execution, visible errors, meaningful exit codes, diagnostic coverage, and focused regression tests.
3. Track false positives and missed findings with source-level adjudication; never treat an analyzer's output as a confirmed defect without checking the implementation and active configuration.
4. Keep handoffs self-contained: latest verified changes, exact test/build outcomes, unresolved findings, preserved worktree state, and concrete next actions.
5. Measure a baseline before choosing targets for task routing accuracy, unsupported-symbol claims, verification selection, and tool failure visibility. No model-capability improvement is claimed until measured.

---

### 2.6 Hardware-Dormant Features (Awaiting Physical Solder Rework)

| Feature / Tool | Status | Precondition | Safety Rule |
| :--- | :--- | :--- | :--- |
| **I2S Audio & Chiptune SFX Engine** | `[BLOCKED / HARDWARE-DORMANT]` | Physical MAX98357A / PCM5102A I2S DAC soldered to GPIO 4, 5, 6. | Rule 00: `FEATURE_AUDIO` must stay `0` |
| **Battery Fuel Gauge & Low Voltage Cutoff** | `[BLOCKED / HARDWARE-DORMANT]` | Physical 100k/100k voltage divider soldered to GPIO 1 with 100nF filter cap. | Rule 00: `FEATURE_BATTERY_MONITOR` must stay `0` |

---

## 3. UI/UX Design Standards for Future Additions

When designing new screens or overlays:
1. **Rule 08 Compliance:** All colors must be sourced from `src/core/theme.h`. No inline hex literals.
2. **Performance Budget:** Menu rendering must execute in `< 4ms` on ESP32-S3. Never allocate heap buffers per frame.
3. **Selection States:** Use outlines (`drawRoundRect`), never filled boxes, to maintain 60 FPS menu responsiveness.
4. **Non-Blocking Control:** All UI animations, carousels, and transitions must use non-blocking `millis()` deltas.