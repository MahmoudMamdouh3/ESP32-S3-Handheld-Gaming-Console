# SDD Section 08: BMO face renderer

[Back to the SDD index](../software-design-document.md)

## 8.1 Ownership and model

[`bmo_face.h`](../../firmware/BmoGameboy/src/core/bmo_face.h) declares the renderer API and the 18 expression enum values. [`bmo_face.cpp`](../../firmware/BmoGameboy/src/core/bmo_face.cpp) owns the expression targets, spring state, procedural distance functions, particles, speech, dance, and pixel composition. The expression target table remains in source; this document does not maintain a second parameter catalog. See [generated symbol references](../generated/symbols.md) for declared interfaces.

`HarmonicSpring::update()` uses a backward Euler solve for the damped spring. It ignores non-finite or non-positive `dt`, caps a positive step at 0.05 seconds, and snaps when `|value-target| < 0.0002` and `|velocity| < 0.0005`. `BmoFace::update()` substitutes a 0.01666-second step when its measured interval is non-positive or exceeds 0.1 seconds. The implicit solve is A-stable for this linear spring system and adds numerical damping; this does not establish the visual response on device.

The current source initializes 14 spring values from the IDLE expression: openness, eye width/height/offsets, mouth width, and mouth Y offset use `omega=22`, `zeta=0.72`; mouth curve uses `20/0.70`; mouth opening uses `24/0.68`; blush uses `16/0.75`; bounce and squash use `24/0.65`; gaze X/Y use `18/0.75`. Bounce starts at 0, squash at 1, and gaze at 0. The expression table remains the source of the other targets. `eyeOpenness` is an expression parameter and the rendered eye height clamps its multiplier to `[0.005, 1.2]`; it is not a normalized percentage label.

**Host verified:** `python -m unittest discover -s tests -p 'test_bmo_face_spring.py' -v` compiles the same spring header with the pinned WASI SDK and runs it through Node's WASI host. Starting at 0 with target 1, `omega=22`, and `zeta=0.72`, the observed maximum overshoot over one second was 0.018119 at `dt=1/60` and 0.005823 at `dt=0.05`. The fixture also checks convergence, finite/bounded values at both steps, the 0.05-second cap, non-finite and non-positive `dt`, and explicit snap termination.

## 8.2 Rendering and culling

`renderToBuffer()` and `renderFullScreen()` render from the shared expression state. The implementation fills the destination background, computes screen-space boxes for facial features and speech, evaluates pixels within those regions, then draws particle bounds. No host reference-render comparison proves that the culled path matches a full-frame evaluator for every transition state.

The source uses an eight-slot particle array. Sleeping mode attempts to spawn a dream particle after a 1.8-second interval; a full pool leaves that spawn unallocated. The speech buffer holds at most 63 bytes plus a terminator; its typewriter interval is 25 ms per byte. `say(nullptr, ...)` is ignored, and a later call replaces the current speech. Dance accepts BPM values strictly between 40 and 240 and uses 125 BPM for other values. These are source-derived behaviors; non-ASCII text is treated as bytes by the typewriter. No separate 172–226 pixel speech range is claimed; the active bubble bounds are computed from the destination size in source.

No face-render timing or culled-pixel percentage is recorded. The historical 18 ms, 72%, and 3.8 ms statements are unverified and are not performance evidence. Use the blank [face-render measurement template](../face-render-measurement-template.md) for a future host or device capture. Do not label host timings as device measurements.

## 8.3 Colors and verification limits

`bmo_face.cpp` contains renderer-specific RGB values packed locally. They are registered in [`DESIGN.md`](../../DESIGN.md) and checked against their source RGB arguments by `python scripts/verify_color_tokens.py`; `COL_TONGUE` is the renderer's pink/coral shade and is distinct from `BMO_TONGUE_PINK`. Renderer validation still lacks culling-equivalence, expression-table drift, native/WASM golden-frame parity, and robustness coverage for speech, particle-pool exhaustion, and non-finite SDF inputs.

Host tests can establish deterministic renderer behavior for a specified build and input. They cannot establish the appearance or timing on the soldered panel. Current open face-renderer work is recorded in [the known-issues ledger](../../.agents/rules/04_known_issues.md).
