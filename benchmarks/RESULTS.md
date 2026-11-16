# BMO Handheld Benchmark Ledger (`benchmarks/RESULTS.md`)

> [!NOTE]
> All host microbenchmarks and simulator measurements below are classified **`HOST_TESTED`**.
> ESP32-S3 ELF sizes are classified **`TARGET_COMPILED`**.
> Physical device measurements remain **`HARDWARE_TESTED: 0` records** until bench bring-up.

---

## 1. Phase 1 Unedited BEFORE Baseline (`refactor/principal-overhaul`)

Captured on `2026-11-16` from [`benchmarks/baseline_before.json`](baseline_before.json):

### 1.1 Firmware Host Benchmarks (`bmo_bench_host.exe`, MSVC Release)

| Benchmark | BEFORE Value | Unit | Notes |
| :--- | ---: | :--- | :--- |
| `bmo_face_sdf_snapshot_320x240_us` | `1,349.39` | `us / frame` | Full 320x240 `BmoFaceSdf::renderTargetSnapshot` |
| `bmo_face_full_pipeline_320x240_us` | `162.75` | `us / frame` | `BmoFace::update` + `BmoFace::renderFullScreen` |
| `metadata_resolve_100_on_10k_us` | `1,085.82` | `us / 100 ops` | $O(F \times N)$ `RomPathMatch::resolveCatalogIndex` |
| `catalog_find_500_on_10k_us` | `7,776.68` | `us / 500 ops` | $O(N)$ linear `equalsNormalized` scan |
| `boxart_legacy_unique_1k_us` | `34,173.30` | `us / 1k ops` | $O(N^2)$ `legacyBaseIsUnique` scan |
| `rom_index_v3_verify_10k_us` | `6,606.18` | `us / save` | 4x full payload CRC passes (save + 3x verify) |
| `nes_8bit_to_565_60frames_us` | `766.31` | `us / 60 frames` | 256x240 8-bit palette blit |
| `pico_128x128_nibble_60frames_us` | `1,242.54` | `us / 60 frames` | Per-pixel `switch(drawMode)` + nibble unpack |
| `profiler_summarize_600_us` | `17.33` | `us / summary` | `BmoProfilerCore::summarizeFrames` (600 samples) |
| `screenshot_encode_320x240_us` | `92.05` | `us / frame` | 320x240 BGR565 -> 24-bit BGR row encode |

### 1.2 Simulator JS / WASM Benchmarks (`node --expose-gc bench_simulator.mjs`)

| Metric | BEFORE Value | Unit | Notes |
| :--- | ---: | :--- | :--- |
| `wasm_init_ms` | `23.53` | `ms` | WASM instantiate + `bmo_face_init` |
| `bmo_face_600_frames_ms` | `411.32` | `ms` | 600 frames via `renderBmoFrame` |
| `bmo_face_600_frames_imagedata_bytes` | `184,320,000` | `bytes` | 600 x 307,200 B `createImageData` churn |
| `gb_render_600_frames_ms` | `228.89` | `ms` | 600 frames Game Boy `renderToCanvas` |
| `gb_render_600_frames_imagedata_bytes` | `124,416,000` | `bytes` | 600 x 207,360 B `createImageData` churn |
| `sync_startup_script_bytes` | `3,374,844` | `bytes` | 14 synchronous blocking `<script>` tags |

### 1.3 Python Tooling & Build Pipelines (`bench_scripts.py` + `build_wasm_cores.py`)

| Metric | BEFORE Value | Unit | Notes |
| :--- | ---: | :--- | :--- |
| `convert_covers_ms` (40 images) | `470.58` | `ms` | Single-threaded PIL + Python BGR565 pack |
| `deploy_sd_cold_ms` (120 files) | `2,411.75` | `ms` | Repeated SHA-256 hashing + serial copy |
| `deploy_sd_incremental_ms` | `371.69` | `ms` | Full re-hash comparison |
| `wasm_full_build_ms` | `25,612.21` | `ms` | Clean scratch build (no incremental `.o` cache) |

### 1.4 Compiled ESP32-S3 Firmware Footprint (`TARGET_COMPILED`, Baseline)

| Profile | App Image (`app0 = 8,388,608 B`) | Static DRAM (`327,680 B`) | IRAM Used | First-Party Warnings |
| :--- | ---: | ---: | ---: | ---: |
| `default` | `3,293,216 B` (`39.3%`) | `185,272 B` (`56.5%`) | `87,040 B` | `0` |
| `no-doom` | `3,016,816 B` (`36.0%`) | `94,952 B` (`29.0%`) | `87,040 B` | `0` |
| `mit` | `893,808 B` (`10.7%`) | `69,856 B` (`21.3%`) | `77,568 B` | `0` |

---

## 2. Phase 5 Post-Overhaul BEFORE vs AFTER Comparison

*(To be populated at Phase 5 completion.)*
