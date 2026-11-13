# Security Policy

## Scope

This repository builds embedded ESP32-S3 firmware, local MicroSD preparation tools, and a static localhost web simulator.

## Key Security & Safety Practices

- **No secrets in git:** Never commit API keys, Wi-Fi credentials, Wokwi tokens (`WOKWI_CLI_TOKEN`), or private keys. `.githooks/pre-commit` scans staged diffs for common credential patterns before allowing commits.
- **Electrical & boot-safety guardrails:** On the ESP32-S3-N16R8 board, setting `FlashMode=opi` or enabling floating ADC/I2S pins (`FEATURE_BATTERY_MONITOR=1`, `FEATURE_AUDIO=1`) can cause boot loops or hardware malfunction. Keep these hard stops intact unless physically modifying the circuit.
- **Local simulator boundary:** `simulator/run_simulator.py` binds exclusively to `127.0.0.1`, serves static assets with a Content-Security-Policy (`script-src 'self' 'wasm-unsafe-eval'`), rejects `POST` and `/api/*` requests, and builds local WASM artifacts on demand without exposing host execution routes.
- **Binary ROM/Save parser safety:** Cartridge headers, `BMOIDX02` catalogs, and `BSS1` save-states read untrusted bytes from MicroSD. Bounds checks and CRC32 validation must precede heap allocation or memcpy into emulator RAM.

## Reporting a Vulnerability

Open a private security advisory on GitHub or contact the repository maintainer directly rather than filing a public issue with exploit details for parsed binary formats or host tools.
