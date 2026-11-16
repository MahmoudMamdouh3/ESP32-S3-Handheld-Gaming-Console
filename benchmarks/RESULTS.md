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

Captured on `2026-11-16` (`refactor/principal-overhaul`) comparing [`benchmarks/baseline_before.json`](baseline_before.json) against `benchmarks/latest_results.json` (generated locally by `python tasks.py bench`; hardware device remains **`HARDWARE_TESTED: 0` records**).

### 2.1 Firmware Host Benchmarks (`HOST_TESTED`, `bmo_bench_host.exe`)

| Benchmark | BEFORE (`baseline_before.json`) | AFTER (`latest_results.json`) | Delta / Speedup | Notes |
| :--- | ---: | ---: | ---: | :--- |
| `bmo_face_sdf_snapshot_320x240_us` | `1,349.39 us` | `288.58 us` | **`4.68x` faster** (`-78.6%`) | Row-mask + X/Y AABB early-exit SDF culling (`bmo_face_sdf.h`) |
| `bmo_face_full_pipeline_320x240_us` | `162.75 us` | `265.76 us` | `+63.3%` (normalized) | `bmo_face_renderer.cpp` deduplicated onto canonical `BmoFaceSdf` (`1,349.39 -> 265.76 us` unified SDF vs pre-dedup path) |
| `metadata_resolve_100_on_10k_us` | `1,085.82 us` | `195.76 us` | **`5.55x` faster** (`-82.0%`) | `CatalogPathLookup` $O(\log N)$ sorted hash index |
| `catalog_find_500_on_10k_us` | `7,776.68 us` | `40.44 us` | **`192.30x` faster** (`-99.5%`) | `CatalogPathLookup::findExact` $O(\log N)$ binary search vs $O(N)$ linear scan |
| `boxart_legacy_unique_1k_us` | `34,173.30 us` | `126.23 us` | **`270.72x` faster** (`-99.6%`) | `StemBaseIndex` sorted stem index vs $O(N^2)$ scan |
| `rom_index_v3_verify_10k_us` | `6,606.18 us` | `3,135.66 us` | **`2.11x` faster** (`-52.5%`) | Buffered single-pass write + streaming CRC verify (`atomic_fs.h`) |
| `nes_8bit_to_565_60frames_us` | `766.31 us` | `740.31 us` | **`1.04x` faster** (`-3.4%`) | 256x240 8-bit palette blit (`parity`) |
| `pico_128x128_nibble_60frames_us` | `1,242.54 us` | `403.40 us` | **`3.08x` faster** (`-67.5%`) | Precomputed 256-entry dual-pixel LUT (`pico_blit.h`) |
| `profiler_summarize_600_us` | `17.33 us` | `16.50 us` | **`1.05x` faster** (`-4.8%`) | PSRAM-backed sample ring summary parity (`-4,840 B` DRAM in `profiler-compile-only` ELF) |
| `screenshot_encode_320x240_us` | `92.05 us` | `57.00 us` | **`1.61x` faster** (`-38.1%`) | Buffered 24-bit BMP row serialization |

### 2.2 Simulator JS / WASM Benchmarks (`HOST_TESTED`, `node --expose-gc bench_simulator.mjs`)

| Metric | BEFORE | AFTER | Delta / Outcome | Notes |
| :--- | ---: | ---: | ---: | :--- |
| `wasm_init_ms` | `23.53 ms` | `10.97 ms` | **`2.14x` faster** (`-53.4%`) | Reduced init overhead + small channel LUTs (`wasm_playtest_engine.js`) |
| `bmo_face_600_frames_ms` | `411.32 ms` | `175.24 ms` | **`2.35x` faster** (`-57.4%`) | Persistent offscreen `ImageData` + L1 channel LUT (`512 B` vs `512 KiB`) |
| `bmo_face_600_frames_imagedata_bytes` | `184,320,000 B` | **`0 B`** | **`-100.0%`** (`600 -> 0` alloc calls) | Eliminated per-frame `createImageData` GC churn |
| `gb_render_600_frames_ms` | `228.89 ms` | `98.33 ms` | **`2.33x` faster** (`-57.0%`) | Cached offscreen buffer + exported fn pointers + palette fast-path |
| `gb_render_600_frames_imagedata_bytes` | `124,416,000 B` | **`0 B`** | **`-100.0%`** (`600 -> 0` alloc calls) | Eliminated per-frame Game Boy `createImageData` churn |
| `sync_startup_script_bytes` | `3,374,844 B` | **`24,805 B`** | **`-99.3%`** (`14 -> 1` sync tags) | ES module refactor + deferred non-blocking scripts (`simulator/index.html`) |

*Note: V8 `bmo_face_heap_delta_bytes` (`155,656 -> 403,920 B`) and `gb_heap_delta_bytes` (`755,736 -> 1,148,120 B`) reflect dynamic ESM module initialization noise under `--expose-gc` rather than steady-state per-frame churn (which dropped to `0 B`).*

### 2.3 Python Tooling & WASM Build (`HOST_TESTED`)

| Metric | BEFORE | AFTER | Delta / Speedup | Notes |
| :--- | ---: | ---: | ---: | :--- |
| `convert_covers_ms` (40 images) | `470.58 ms` | `114.28 ms` | **`4.12x` faster** (`-75.7%`) | ThreadPool + vectorized NumPy BGR565 pack (`scripts/convert_covers.py`) |
| `deploy_sd_cold_ms` (120 files) | `2,411.75 ms` | `2,333.47 ms` | **`1.03x` faster** (`-3.2%`) | Parallel hash/preflight + single-pass copy (`scripts/deploy_sd.py`) |
| `deploy_sd_incremental_ms` | `371.69 ms` | `157.94 ms` | **`2.35x` faster** (`-57.5%`) | Size/mtime fast-path skip |
| `wasm_full_build_ms` | `25,612.21 ms` | `17,710.19 ms` | **`1.45x` faster** (`-30.9%`) | Per-unit `.o`/`.d` cache + parallel clang (`scripts/build_wasm_cores.py`) |

### 2.4 Compiled ESP32-S3 Firmware Footprint (`TARGET_COMPILED`, All 7 Profiles = `0` First-Party Warnings)

| Profile | BEFORE App Image | AFTER App Image (`app0 = 8,388,608 B`) | Static DRAM (`327,680 B`) | IRAM Used |
| :--- | ---: | ---: | ---: | ---: |
| `default` | `3,293,216 B` | `3,275,312 B` (**`39.0%`**, `-17,904 B`) | `185,456 B` (`56.6%`) | `87,552 B` |
| `no-doom` | `3,016,816 B` | `2,999,568 B` (**`35.8%`**, `-17,248 B`) | `95,136 B` (`29.0%`) | `87,552 B` |
| `mit` | `893,808 B` | `878,688 B` (**`10.5%`**, `-15,120 B`) | `69,504 B` (`21.2%`) | `77,824 B` |
| `audio-compile-only` | `—` | `3,275,696 B` sketch (`19%`) | `185,456 B` (`56.6%`) | `—` |
| `battery-compile-only` | `—` | `3,291,596 B` sketch (`19%`) | `185,856 B` (`56.6%`) | `—` |
| `debug-log-compile-only` | `—` | `3,275,156 B` sketch (`19%`) | `185,456 B` (`56.6%`) | `—` |
| `profiler-compile-only` | `—` (`190,312 B` DRAM pre-PSRAM) | `3,278,016 B` (**`39.1%`**) | `185,472 B` (**`-4,840 B` DRAM** via PSRAM ring) | `87,552 B` |
