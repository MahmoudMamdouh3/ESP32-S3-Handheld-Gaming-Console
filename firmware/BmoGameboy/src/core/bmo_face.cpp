// -----------------------------------------------------------------------
// bmo_face.cpp  –  Procedural SDF mascot face renderer.
//
// Every pixel of the face is computed mathematically using 2-D signed
// distance functions.  No bitmap, sprite, or precomputed pixel arrays
// exist anywhere in this file.
//
// Rendering pipeline (per draw() call):
//   1. Walk every pixel in faceBuf[FACE_FB_W × FACE_FB_H].
//   2. Map pixel (fx, fy) → normalised face space [-1, +1].
//   3. Evaluate SDF for left eye, right eye, and mouth.
//   4. Apply smoothstep() over a ~1.5-pixel AA band per shape.
//   5. Blend background → shape colour via the resulting alpha.
//   6. Nearest-neighbour scale-blit faceBuf to the requested (x,y,size).
//
// Colour format: BGR565 byte-swapped for SPI — identical to the format
// used by DisplayEmu::CLASSIC_PALETTE and DisplayEmu::NES_PALETTE.
// -----------------------------------------------------------------------

#include "bmo_face.h"
#include "display_emu.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>

// ===========================================================================
// Internal helpers (anonymous namespace — no external linkage)
// ===========================================================================

namespace {

// ---------------------------------------------------------------------------
// Colour packing — produces byte-swapped BGR565 matching DisplayEmu palettes.
// ---------------------------------------------------------------------------
static inline uint16_t packBGR565(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t bgr = ((uint16_t)(b & 0xF8) << 8)
               | ((uint16_t)(g & 0xFC) << 3)
               | ((uint16_t)r >> 3);
  return (uint16_t)((bgr << 8) | (bgr >> 8)); // byte-swap for SPI
}

// ---------------------------------------------------------------------------
// Math helpers
// ---------------------------------------------------------------------------
static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static inline float smoothstepf(float lo, float hi, float t) {
  t = clampf((t - lo) / (hi - lo), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

static inline float lerpf(float a, float b, float t) {
  return a + (b - a) * t;
}

// ---------------------------------------------------------------------------
// SDF primitives — normalised face space [-1, +1].
// Negative value = inside the shape, positive = outside.
// ---------------------------------------------------------------------------

// Axis-aligned ellipse with semi-axes (rx, ry) centred at origin.
// Uses the Inigo Quilez ellipse SDF approximation.
static inline float sdfEllipse(float px, float py, float rx, float ry) {
  float sx = px / (rx + 1e-9f);
  float sy = py / (ry + 1e-9f);
  float r  = sqrtf(sx * sx + sy * sy);
  float gx = sx / (rx * r + 1e-9f);
  float gy = sy / (ry * r + 1e-9f);
  float gl = sqrtf(gx * gx + gy * gy);
  return (r - 1.0f) / (gl + 1e-9f);
}

// Axis-aligned rectangle with half-extents (hw, hh) centred at origin.
static inline float sdfRect(float px, float py, float hw, float hh) {
  float dx = fabsf(px) - hw;
  float dy = fabsf(py) - hh;
  float ox = dx > 0.0f ? dx : 0.0f;
  float oy = dy > 0.0f ? dy : 0.0f;
  float outside = sqrtf(ox * ox + oy * oy);
  float inside  = (dx < 0.0f && dy < 0.0f) ? (dx > dy ? dx : dy) : 0.0f;
  return outside + inside;
}

// Parabolic-arc mouth — approximates a curved smile/frown.
//
//   width    : half-width of the arc, x ∈ [-width, +width]
//   curve    : parabolic bend (>0 = smile / concave-up, <0 = frown)
//   openness : stroke half-thickness
//
// For |x| ≤ width: distance = |y - curve*x²| - openness
// For |x| > width: distance to the nearest arc endpoint.
//
// True Bézier closest-point requires solving a cubic (too expensive on
// an embedded CPU).  This parabolic approximation is accurate to within
// ~1 pixel for |curve| ≤ 1.0, which covers all expression targets.
static inline float sdfMouth(float px, float py,
                              float width, float curve, float openness) {
  if (fabsf(px) > width) {
    // Distance to the nearest arc endpoint.
    float epx = (px < 0.0f) ? -width : width;
    float epy = curve * epx * epx;
    float ddx = px - epx;
    float ddy = py - epy;
    return sqrtf(ddx * ddx + ddy * ddy) - openness;
  }
  // Inside width range: signed distance to the parabola, minus stroke.
  float arcY = curve * px * px;
  return fabsf(py - arcY) - openness;
}

// ---------------------------------------------------------------------------
// Face parameter struct — one per expression, live-interpolated.
// ---------------------------------------------------------------------------
struct FaceParams {
  float eyeOpenness;    // [0=closed .. 1=fully open] eye vertical scale
  float eyeWidth;       // eye horizontal semi-axis (normalised units)
  float eyeOffsetX;     // horizontal distance from centre to each eye
  float eyeOffsetY;     // vertical position of the eyes (>0 = up)
  float mouthWidth;     // mouth arc half-width
  float mouthCurve;     // parabolic bend (>0 smile, <0 frown)
  float mouthOpenness;  // mouth stroke half-thickness
  float mouthOffsetY;   // mouth vertical position (<0 = lower on face)
};

// ---------------------------------------------------------------------------
// Expression target table — indexed by (int)BmoExpression.
// Vertical layout: face origin is centre, Y up.  Eyes sit above centre,
// mouth below.  All values are in normalised [-1,+1] face units.
//
//                          eyeOpen  eyeW   eyeOffX eyeOffY  mthW  mthCrv mthOpen mthOffY
static const FaceParams EXPR_TARGETS[] = {
  /* IDLE        */ { 0.90f, 0.28f, 0.30f,  0.15f, 0.26f,  0.40f, 0.055f, -0.25f },
  /* SURPRISED   */ { 1.00f, 0.35f, 0.30f,  0.18f, 0.14f,  0.00f, 0.055f, -0.32f },
  /* HAPPY       */ { 0.50f, 0.32f, 0.30f,  0.15f, 0.30f,  0.70f, 0.065f, -0.22f },
  /* SLEEPY      */ { 0.20f, 0.30f, 0.30f,  0.12f, 0.22f,  0.15f, 0.045f, -0.28f },
  /* LOW_BATTERY */ { 0.60f, 0.25f, 0.28f,  0.10f, 0.24f, -0.30f, 0.050f, -0.28f },
  /* CHARGING    */ { 0.85f, 0.28f, 0.30f,  0.15f, 0.24f,  0.30f, 0.050f, -0.28f },
  /* ERROR       */ { 1.00f, 0.28f, 0.30f,  0.15f, 0.28f, -0.50f, 0.055f, -0.20f },
  /* SHUTDOWN    */ { 0.05f, 0.30f, 0.30f,  0.10f, 0.20f,  0.05f, 0.040f, -0.30f },
  /* HIDDEN      */ { 0.00f, 0.00f, 0.00f,  0.00f, 0.00f,  0.00f, 0.000f,  0.00f },
};

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------
static BmoFace::BmoExpression  s_expr          = BmoFace::HIDDEN;
static FaceParams               s_current;
static bool                     s_useXEyes      = false;
static bool                     s_dirty         = false;

// Blink state
static unsigned long            s_lastBlinkMs   = 0;
static unsigned long            s_nextBlinkMs   = 3000;
static float                    s_blinkProgress = 0.0f;   // 0=open, 1=closed
static bool                     s_blinking      = false;
static unsigned long            s_blinkStartMs  = 0;
static const unsigned long      BLINK_HALF_MS   = 75;     // ms to close/open

// Update rate cap
static unsigned long            s_lastUpdateMs  = 0;
static const unsigned long      UPDATE_PERIOD_MS = 33;    // ~30 fps

// Exponential decay rate (1/sec)
static const float              EASE_RATE = 8.0f;

// ---------------------------------------------------------------------------
// Static framebuffer — module-level, 4-byte aligned.
// Follows the same pattern as rowBuffer[] in emu_peanut.cpp / emu_walnut.cpp.
// 128 × 128 × 2 = 32 KB — fits in internal DRAM.
// ---------------------------------------------------------------------------
static uint16_t __attribute__((aligned(4))) faceBuf[FACE_FB_W * FACE_FB_H];

// Row scratch for scale-blit — up to FACE_LARGE_SIZE (160) pixels wide.
static uint16_t __attribute__((aligned(4))) rowOut[FACE_LARGE_SIZE];

// ---------------------------------------------------------------------------
// Helper: does this expression auto-blink?
// ---------------------------------------------------------------------------
static bool expressionBlinks(BmoFace::BmoExpression e) {
  return e == BmoFace::IDLE
      || e == BmoFace::HAPPY
      || e == BmoFace::CHARGING;
}

// ---------------------------------------------------------------------------
// renderFace() — fills faceBuf using SDF + smoothstep AA.
//
// Coordinate system: origin at buffer centre, X right, Y up.
// normalised units so the face fits inside [-1, +1] × [-1, +1].
// ---------------------------------------------------------------------------
static void renderFace() {
  // Anti-aliasing band: 1.5 pixels expressed in normalised [-1,+1] units.
  const float aaStep = (2.0f / (float)FACE_FB_W) * 1.5f;

  // Eye vertical semi-axis — driven by eyeOpenness; never fully zero to
  // avoid a degenerate ellipse (it just becomes a flat sliver).
  const float eyeH = s_current.eyeWidth * 0.55f
                   * (s_current.eyeOpenness * (1.0f - s_blinkProgress));
  const float eyeHSafe = eyeH < 0.004f ? 0.004f : eyeH;

  // Pupil semi-axes (fraction of eye size).
  const float pupilW = s_current.eyeWidth * 0.38f;
  const float pupilH = eyeHSafe * 0.40f;

  // Eye centres in normalised space.
  const float lEyeX = -s_current.eyeOffsetX;   // left eye (negative X)
  const float rEyeX =  s_current.eyeOffsetX;   // right eye (positive X)
  const float eyeY  =  s_current.eyeOffsetY;   // both at same Y

  for (int fy = 0; fy < FACE_FB_H; ++fy) {
    // Invert Y so row 0 = top of face = positive Y in face space.
    float ny = 1.0f - 2.0f * ((float)fy + 0.5f) / (float)FACE_FB_H;
    uint16_t* row = &faceBuf[fy * FACE_FB_W];

    for (int fx = 0; fx < FACE_FB_W; ++fx) {
      float nx = -1.0f + 2.0f * ((float)fx + 0.5f) / (float)FACE_FB_W;

      // ---------------------------------------------------------------
      // Background colour (BMO teal-green) as float RGB [0,1].
      // ---------------------------------------------------------------
      float cr = 83.0f  / 255.0f;
      float cg = 198.0f / 255.0f;
      float cb = 181.0f / 255.0f;

      // ---------------------------------------------------------------
      // Evaluate eye SDFs (left and right share the same logic).
      // ---------------------------------------------------------------
      float dEyeL, dEyeR, dPupilL, dPupilR;

      if (!s_useXEyes) {
        // --- Normal ellipse eyes ---
        dEyeL   = sdfEllipse(nx - lEyeX, ny - eyeY, s_current.eyeWidth, eyeHSafe);
        dEyeR   = sdfEllipse(nx - rEyeX, ny - eyeY, s_current.eyeWidth, eyeHSafe);

        // Pupils: offset slightly down and toward the nose for a natural look.
        dPupilL = sdfEllipse(nx - lEyeX + s_current.eyeWidth * 0.12f,
                             ny - eyeY  - eyeHSafe * 0.18f,
                             pupilW, pupilH);
        dPupilR = sdfEllipse(nx - rEyeX - s_current.eyeWidth * 0.12f,
                             ny - eyeY  - eyeHSafe * 0.18f,
                             pupilW, pupilH);
      } else {
        // --- ERROR X-eyes: two crossing rectangles, rotated ±45° ---
        // Left X
        float lx = nx - lEyeX;
        float ly = ny - eyeY;
        float lxa =  lx * 0.7071f + ly * 0.7071f;
        float lya = -lx * 0.7071f + ly * 0.7071f;
        float lxb =  lx * 0.7071f - ly * 0.7071f;
        float lyb =  lx * 0.7071f + ly * 0.7071f;
        dEyeL   = fminf(sdfRect(lxa, lya, s_current.eyeWidth * 0.85f, s_current.eyeWidth * 0.11f),
                        sdfRect(lxb, lyb, s_current.eyeWidth * 0.85f, s_current.eyeWidth * 0.11f));
        dPupilL = 1.0f; // X shape drawn entirely via dEyeL

        // Right X
        float rx = nx - rEyeX;
        float ry = ny - eyeY;
        float rxa =  rx * 0.7071f + ry * 0.7071f;
        float rya = -rx * 0.7071f + ry * 0.7071f;
        float rxb =  rx * 0.7071f - ry * 0.7071f;
        float ryb =  rx * 0.7071f + ry * 0.7071f;
        dEyeR   = fminf(sdfRect(rxa, rya, s_current.eyeWidth * 0.85f, s_current.eyeWidth * 0.11f),
                        sdfRect(rxb, ryb, s_current.eyeWidth * 0.85f, s_current.eyeWidth * 0.11f));
        dPupilR = 1.0f;
      }

      // ---------------------------------------------------------------
      // Evaluate mouth SDF.
      // ---------------------------------------------------------------
      float dMouth = sdfMouth(nx,
                               ny - s_current.mouthOffsetY,
                               s_current.mouthWidth,
                               s_current.mouthCurve,
                               s_current.mouthOpenness);

      // ---------------------------------------------------------------
      // Composite: background → eye white → pupil/X → mouth.
      // Each layer uses smoothstep() for AA.  Order matters: later
      // layers paint over earlier ones.
      // ---------------------------------------------------------------

      // Eye whites (both eyes share the same white colour).
      float dEye = fminf(dEyeL, dEyeR);
      float eyeAlpha = smoothstepf(aaStep, 0.0f, -dEye);
      cr = lerpf(cr, 0.96f, eyeAlpha);
      cg = lerpf(cg, 0.95f, eyeAlpha);
      cb = lerpf(cb, 0.95f, eyeAlpha);

      // Pupil / X-eye stroke.
      if (!s_useXEyes) {
        float dPupil = fminf(dPupilL, dPupilR);
        float pupilAlpha = smoothstepf(aaStep, 0.0f, -dPupil);
        cr = lerpf(cr, 0.10f, pupilAlpha);
        cg = lerpf(cg, 0.10f, pupilAlpha);
        cb = lerpf(cb, 0.12f, pupilAlpha);
      } else {
        // X-eye: the stroke IS the dEye shape, drawn in red.
        // eyeAlpha already masks the X region; tint it red.
        // We re-evaluate alpha for the filled-X colour here.
        float xAlpha = smoothstepf(aaStep, 0.0f, -dEye);
        cr = lerpf(83.0f / 255.0f, 0.88f, xAlpha);
        cg = lerpf(198.0f / 255.0f, 0.14f, xAlpha);
        cb = lerpf(181.0f / 255.0f, 0.10f, xAlpha);
      }

      // Mouth stroke.
      float mouthAlpha = smoothstepf(aaStep, 0.0f, -dMouth);
      cr = lerpf(cr, 0.08f, mouthAlpha);
      cg = lerpf(cg, 0.08f, mouthAlpha);
      cb = lerpf(cb, 0.10f, mouthAlpha);

      // Pack final colour.
      row[fx] = packBGR565(
        (uint8_t)(cr * 255.0f + 0.5f),
        (uint8_t)(cg * 255.0f + 0.5f),
        (uint8_t)(cb * 255.0f + 0.5f)
      );
    }
  }
}

// ---------------------------------------------------------------------------
// blitFace() — renders + pushes the face to the display at (x, y, size).
//
// Renders once into the 128×128 faceBuf, then nearest-neighbour scale-blits
// to the output size row-by-row via DisplayEmu::pushPixelsAt().
// We avoid a second large heap allocation by doing the scale mapping into a
// per-row scratch (rowOut[], module-level, max FACE_LARGE_SIZE pixels).
// ---------------------------------------------------------------------------
static void blitFace(int x, int y, int size) {
  // Clamp output size so rowOut[] is not overrun.
  if (size > FACE_LARGE_SIZE) size = FACE_LARGE_SIZE;
  if (size <= 0)              return;

  // -----------------------------------------------------------------------
  // Timed SDF render pass — same profiling pattern as DOOM tick latency
  // check in emu_doom.cpp.  Fires once per draw(), not per loop() call.
  // -----------------------------------------------------------------------
  unsigned long t0 = micros();
  renderFace();
  unsigned long renderUs = micros() - t0;

  static bool warnedSlow = false;
  if (!warnedSlow && renderUs > 5000) {
    LOG_WARN("[BmoFace] WARNING: SDF render took %lu us (>5ms threshold)", renderUs);
    warnedSlow = true;
  }

  // -----------------------------------------------------------------------
  // Scale-blit: map output pixel (ox, oy) → source pixel (sx, sy) using
  // nearest-neighbour sampling.  Push each row as a self-contained
  // DisplayEmu::pushPixelsAt() transaction.
  // -----------------------------------------------------------------------
  float invSize = (float)FACE_FB_W / (float)size;

  for (int oy = 0; oy < size; ++oy) {
    int sy = (int)(((float)oy + 0.5f) * invSize);
    if (sy >= FACE_FB_H) sy = FACE_FB_H - 1;
    const uint16_t* srcRow = &faceBuf[sy * FACE_FB_W];

    for (int ox = 0; ox < size; ++ox) {
      int sx = (int)(((float)ox + 0.5f) * invSize);
      if (sx >= FACE_FB_W) sx = FACE_FB_W - 1;
      rowOut[ox] = srcRow[sx];
    }

    DisplayEmu::pushPixelsAt(x, y + oy, size, 1, rowOut);
  }
}

} // anonymous namespace

// ===========================================================================
// Public API — BmoFace namespace
// ===========================================================================

namespace BmoFace {

void begin() {
  randomSeed(micros()); // seed blink RNG from free-running hardware timer

  // Initialise live parameters to IDLE so the first draw() is valid.
  s_current  = EXPR_TARGETS[(int)IDLE];
  s_expr     = IDLE;
  s_useXEyes = false;
  s_dirty    = true;

  s_lastBlinkMs = millis();
  s_nextBlinkMs = (unsigned long)random(2000, 5500);
}

void setExpression(BmoExpression expr) {
  if (s_expr == expr) return;

  s_expr     = expr;
  s_useXEyes = (expr == ERROR);

  // ERROR cuts immediately — no smooth interpolation.
  if (expr == ERROR) {
    s_current = EXPR_TARGETS[(int)ERROR];
  }

  // Cancel any in-progress blink; reschedule if the new expression blinks.
  s_blinking      = false;
  s_blinkProgress = 0.0f;

  if (expressionBlinks(expr)) {
    s_lastBlinkMs = millis();
    s_nextBlinkMs = (unsigned long)random(2000, 5500);
  }

  s_dirty = true;
}

void update() {
  if (s_expr == HIDDEN) return;

  unsigned long now = millis();

  // Rate-limit: skip if called more than once every ~33ms (~30 fps cap).
  if (now - s_lastUpdateMs < UPDATE_PERIOD_MS) return;

  float dt = (float)(now - s_lastUpdateMs) * 0.001f; // seconds
  s_lastUpdateMs = now;

  // -----------------------------------------------------------------------
  // Exponential-decay easing toward the current expression's target params.
  // Formula: current += (target - current) * EASE_RATE * dt
  // ERROR already snapped to target in setExpression(); skip easing for it.
  // -----------------------------------------------------------------------
  if (s_expr != ERROR) {
    const FaceParams& tgt = EXPR_TARGETS[(int)s_expr];
    float k = EASE_RATE * dt;
    if (k > 1.0f) k = 1.0f; // prevent overshoot on very long dt spikes

    // Macro: ease one field and set dirty if it moved meaningfully.
    #define EASE_FIELD(F) \
      { float _d = tgt.F - s_current.F; \
        if (fabsf(_d) > 0.0005f) { s_current.F += _d * k; s_dirty = true; } }

    EASE_FIELD(eyeOpenness)
    EASE_FIELD(eyeWidth)
    EASE_FIELD(eyeOffsetX)
    EASE_FIELD(eyeOffsetY)
    EASE_FIELD(mouthWidth)
    EASE_FIELD(mouthCurve)
    EASE_FIELD(mouthOpenness)
    EASE_FIELD(mouthOffsetY)

    #undef EASE_FIELD
  }

  // -----------------------------------------------------------------------
  // Auto-blink: trigger a blink after a random 2–5.5 s interval.
  // Only active for IDLE, HAPPY, CHARGING (see expressionBlinks()).
  // -----------------------------------------------------------------------
  if (expressionBlinks(s_expr) && !s_blinking) {
    if ((now - s_lastBlinkMs) >= s_nextBlinkMs) {
      s_blinking      = true;
      s_blinkStartMs  = now;
    }
  }

  if (s_blinking) {
    unsigned long elapsed = now - s_blinkStartMs;
    if (elapsed < BLINK_HALF_MS) {
      // Closing phase: progress 0 → 1
      s_blinkProgress = (float)elapsed / (float)BLINK_HALF_MS;
    } else if (elapsed < BLINK_HALF_MS * 2UL) {
      // Opening phase: progress 1 → 0
      s_blinkProgress = 1.0f - (float)(elapsed - BLINK_HALF_MS) / (float)BLINK_HALF_MS;
    } else {
      // Blink complete; schedule the next one.
      s_blinking      = false;
      s_blinkProgress = 0.0f;
      s_lastBlinkMs   = now;
      s_nextBlinkMs   = (unsigned long)random(2000, 5500);
    }
    s_dirty = true;
  }
}

void draw(int x, int y, int size) {
  if (s_expr == HIDDEN) return;
  blitFace(x, y, size);
  s_dirty = false;
}

void draw() {
  // Zero-arg overload — blits a large centered face for battery/error events.
  draw(FACE_LARGE_X, FACE_LARGE_Y, FACE_LARGE_SIZE);
}

bool isDirty() {
  return s_dirty;
}

} // namespace BmoFace
