# ADR 0010: Simulator and Firmware Boundaries

- Status: Accepted
- Date: 2026-11-07
- Source: `tools/bmo_simulator/`, `diagram.json`, `firmware/BmoGameboy/`

## Context

The repository has browser JavaScript/WebGL, WebAssembly core adapters, Wokwi configuration, native host tools, and ESP32-S3 firmware. Similar screen content does not make their execution or electrical behavior equivalent.

## Decision

Document and test each environment separately. Browser routes execute browser code; Wokwi is a circuit model; native runners execute selected host-portable code; only target builds compile the firmware; only measurements on the soldered board establish hardware behavior. The local simulator server is loopback-only and its host-core endpoint is authenticated, rate-limited, and fixed-command.

## Consequences

No simulator result closes a physical display, SD voltage, power, input latency, or performance finding. Browser firmware-UI work remains a separate design task.
