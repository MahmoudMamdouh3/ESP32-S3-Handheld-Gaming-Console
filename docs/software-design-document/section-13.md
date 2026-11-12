# SDD Section 13: Browser simulator, WASM, and Wokwi

[Back to the SDD index](../software-design-document.md)

## 13.1 Browser simulator

`tools/bmo_simulator/index.html`, `simulator.css`, and `simulator.js` implement a browser-side interface, controls, local storage, and optional WebSerial connection. `bmo_3d_console.js` renders an 82 × 124 × 26 mm nominal console at centimetre scale. The front shell has a display opening, a flush black glass base with a PBR material, a nearest-filtered active screen canvas, shaped button caps, and centered L/R legends. The scene keeps one consistent 9 mm plan-view corner radius, a 2.1 mm nominal wall in the printable model, and a two-half rear seam.

The browser model places the vertical BMO marks and seven-hole speaker clusters on both sides, and depicts the rear battery cover, vents, screws, and external ports. Its side marks and rear access details are visual meshes; the print generator makes the side marks, grille bores, port openings, and rear vents as CAD cuts. The button positions, body envelope, side-feature coordinates, and display window are kept aligned by hand between the simulator and [`generate_bmo_enclosure.py`](../../tools/enclosure/generate_bmo_enclosure.py). The simulator geometry is not generated from the CAD file and is not a direct mesh export. The CAD meshes and Blender/Unity import steps are documented in [the enclosure guide](../enclosure-3d-printing.md).

The interactive model does not simulate electrical behavior, display timing, board state, or physical button switches. The GLB is a static assembly preview, while the seven STLs are the printable parts. Neither simulator rendering nor mesh validation proves fit on the soldered perfboard or thermal performance. Browser colors use CSS values, while firmware uses byte-swapped BGR565 words; `python scripts/verify_color_tokens.py` compares the source token definitions.

The playtest server in `run_simulator.py` binds to loopback. Static files are resolved under the simulator directory. The host-cores endpoint is a fixed POST action with per-process session token, Host/Origin checks, request limits, a bounded subprocess, and no shell command construction. Tests exercise these server boundaries; they do not make the service suitable for exposing on a network.

## 13.2 WebAssembly boundaries

`scripts/build_wasm_cores.py` builds the face renderer and a selected set of playtest adapters when the WASI toolchain is installed. The WASM image does not include `BmoGameboy.ino`, FreeRTOS, the full Arduino display driver, the SD stack, or the full set of firmware wrappers. A passing WASM test proves only the named exported functions and test fixtures in that build.

## 13.3 Wokwi

`diagram.json` and `wokwi.toml` describe a simulator circuit. The diagram is not an as-built wiring record; compare it with [Rule 01](../../.agents/rules/01_hardware.md) before using it to reason about the physical perfboard. Electrical levels and actual parts require physical measurement.
