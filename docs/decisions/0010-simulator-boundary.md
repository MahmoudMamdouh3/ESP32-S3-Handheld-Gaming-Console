# ADR 0010: Simulator and Firmware Boundaries

- Status: Accepted
- Date: 2026-11-07
- Source: `simulator/`, `hardware/diagram.json`, `hardware/wokwi.toml`, `firmware/BmoGameboy/`

## Context

The repository has browser JavaScript/WebGL, WebAssembly core adapters, Wokwi configuration, native host tools, and ESP32-S3 firmware. Similar screen content does not make their execution or electrical behavior equivalent.

## Decision

Document and test each environment separately. Browser routes execute browser code; Wokwi is a circuit model; native runners execute selected host-portable code; only target builds compile the firmware; only measurements on the soldered board establish hardware behavior. The local simulator server is loopback-only and static readonly (rejecting `POST` and `/api/*` routes; host-core execution routes were removed in Phase 1).

## Consequences

No simulator result closes a physical display, SD voltage, power, input latency, or performance finding. Browser firmware-UI work remains a separate design task.
