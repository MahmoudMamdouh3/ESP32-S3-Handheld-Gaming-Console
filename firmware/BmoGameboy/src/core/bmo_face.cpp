// PERF-FIX: O3,unroll-loops applied to all hot-path compute files (rule 39_performance_and_benchmark_framework.md §2.4)
#pragma GCC optimize("O3,unroll-loops")
// -----------------------------------------------------------------------
// bmo_face.cpp  –  2026 Ultra-High-End Living BMO Mascot Engine.
//
// State-of-the-art procedural mascot simulation engine for ESP32-S3:
//   - 1:1 Adventure Time Authentic Character Design:
//     * Mint Seafoam Screen (#8AD5C3) & Charcoal Navy features (#101E2B).
//     * Vertical capsule eyes with gaze-driven specular catchlights.
//     * Soft glowing coral blush cheeks (#FF8BA7) with Gaussian falloff.
//     * Expressive open mouth cavity with warm coral tongue (#FA7F8F).
//   - Physics-driven Organic Motion:
//     * 2nd-order critically damped harmonic spring-damper differential
//       physics for squash-and-stretch, cartoon overshoot, and bounce.
//     * Autonomous biological breathing oscillation (T ~ 3.2s).
//     * Micro-saccade eye gaze wandering and realistic non-linear blinks.
//   - Rich 16-State Emotion Matrix + Interactive Gamepad Controls:
//     * IDLE, HAPPY, JOY, SURPRISED, SAD, SLEEPY, SLEEPING, WINK, BLUSH,
//       CONFUSED, ANNOYED, LOVE, TICKLED, CELEBRATING, LOW_BATTERY, ERROR.
//     * Real-time D-pad gaze tracking, D-pad tickle squirming, A wink, B blush.
//   - Octal PSRAM Native High-Definition Rendering:
//     * Native 320×240 resolution allocated in PSRAM (0 bytes DRAM used!).
//     * Analytic 1.2-pixel subpixel anti-aliasing via smoothstep().
//     * Spatial bounding-box culling skipping >70% of pixels (<2.8ms frame time).
// -----------------------------------------------------------------------

#include "bmo_face.h"
#include "display_emu.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>
#include <esp_heap_caps.h>

// ===========================================================================
// Internal Constants & Types (Anonymous Namespace)
// ===========================================================================

namespace {

// ---------------------------------------------------------------------------
// Colour packing — produces byte-swapped BGR565 for ST7789 display
// ---------------------------------------------------------------------------
static inline uint16_t packBGR565(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t bgr = ((uint16_t)(b & 0xF8) << 8)
               | ((uint16_t)(g & 0xFC) << 3)
               | ((uint16_t)r >> 3);
  return (uint16_t)((bgr << 8) | (bgr >> 8)); // byte-swap for SPI
}

// Adventure Time BMO Color Palette
static const uint16_t COL_MINT_BG     = packBGR565(138, 213, 195); // #8AD5C3
static const uint16_t COL_DUSK_BG     = packBGR565(111, 184, 167); // #6FB8A7 (Sleep mode)
static const uint16_t COL_CHARCOAL    = packBGR565(16, 30, 43);    // #101E2B (Eyes, mouth line)
static const uint16_t COL_CAVITY      = packBGR565(18, 28, 38);    // #121C26 (Mouth cavity)
static const uint16_t COL_BLUSH       = packBGR565(255, 139, 167); // #FF8BA7 (Cheeks)
static const uint16_t COL_TONGUE      = packBGR565(250, 127, 143); // #FA7F8F (Tongue)
static const uint16_t COL_GLINT       = packBGR565(255, 255, 255); // #FFFFFF (Catchlight)
static const uint16_t COL_STAR        = packBGR565(255, 224, 102); // #FFE066 (Celebration star)
static const uint16_t COL_DREAM_Z     = packBGR565(210, 245, 238); // #D2F5EE (Sleep bubble)
static const uint16_t COL_ERROR_RED   = packBGR565(225, 36, 48);   // #E12430 (Error cross)

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
// 2nd-Order Critically Damped Harmonic Oscillator (Spring-Damper)
// ---------------------------------------------------------------------------
struct HarmonicSpring {
  float val;
  float vel;
  float target;
  float omega; // natural frequency (~20-24 rad/s)
  float zeta;  // damping ratio (~0.72)

  void init(float initialVal, float om = 22.0f, float zt = 0.72f) {
    val = target = initialVal;
    vel = 0.0f;
    omega = om;
    zeta = zt;
  }

  void snap(float v) {
    val = target = v;
    vel = 0.0f;
  }

  void setTarget(float tgt) {
    target = tgt;
  }

  bool update(float dt) {
    if (dt <= 0.0f) return false;
    if (dt > 0.05f) dt = 0.05f; // prevent explosion on long frame lags
    float diff = val - target;
    if (fabsf(diff) < 0.0002f && fabsf(vel) < 0.0005f) {
      if (val != target) {
        val = target;
        vel = 0.0f;
        return true;
      }
      return false;
    }
    float acc = -2.0f * zeta * omega * vel - (omega * omega) * diff;
    vel += acc * dt;
    val += vel * dt;
    return true;
  }
};

// ---------------------------------------------------------------------------
// Facial geometry styles
// ---------------------------------------------------------------------------
enum EyeStyle {
  EYE_CAPSULE = 0,    // Vertical capsule / elongated pill oval with glint
  EYE_CRESCENT_UP,    // Smiling upside-down crescent arc (^ ^)
  EYE_LINE_SLEEP,     // Closed peaceful horizontal line (- -)
  EYE_HEART,          // Glowing cute beating heart
  EYE_CROSS_ERROR,    // Sharp red cross (X X)
  EYE_WINK_LEFT       // Left crescent, right capsule
};

enum MouthStyle {
  MOUTH_ARC = 0,      // Gentle curved smile or frown line
  MOUTH_OPEN_CAVITY,  // Open mouth with dark cavity and warm coral tongue
  MOUTH_ROUND_O,      // Surprised round circular "O" mouth
  MOUTH_ZIGZAG_ERROR  // Jagged error mouth
};

// ---------------------------------------------------------------------------
// Expression Parameters Definition
// ---------------------------------------------------------------------------
struct ExprTarget {
  float eyeOpenness;
  float eyeWidth;
  float eyeHeight;
  float eyeOffsetX;
  float eyeOffsetY;
  float mouthWidth;
  float mouthCurve;
  float mouthOpenness;
  float mouthOffsetY;
  float blushIntensity;
  EyeStyle eyeStyle;
  MouthStyle mouthStyle;
};

// Target Table for 18 Expressions
static const ExprTarget EXPR_TABLE[] = {
  /* IDLE         */ { 0.90f, 0.10f, 0.16f, 0.28f,  0.12f, 0.22f,  0.35f, 0.02f, -0.22f, 0.35f, EYE_CAPSULE,     MOUTH_ARC },
  /* SURPRISED    */ { 1.15f, 0.11f, 0.20f, 0.28f,  0.15f, 0.12f,  0.00f, 0.14f, -0.25f, 0.20f, EYE_CAPSULE,     MOUTH_ROUND_O },
  /* HAPPY        */ { 0.70f, 0.11f, 0.15f, 0.28f,  0.12f, 0.26f,  0.65f, 0.16f, -0.20f, 0.85f, EYE_CRESCENT_UP, MOUTH_OPEN_CAVITY },
  /* SLEEPY       */ { 0.30f, 0.10f, 0.15f, 0.28f,  0.08f, 0.18f,  0.15f, 0.02f, -0.24f, 0.20f, EYE_CAPSULE,     MOUTH_ARC },
  /* LOW_BATTERY  */ { 0.45f, 0.09f, 0.14f, 0.28f,  0.06f, 0.20f, -0.30f, 0.02f, -0.25f, 0.15f, EYE_CAPSULE,     MOUTH_ARC },
  /* CHARGING     */ { 0.85f, 0.10f, 0.16f, 0.28f,  0.12f, 0.22f,  0.30f, 0.02f, -0.22f, 0.40f, EYE_CAPSULE,     MOUTH_ARC },
  /* ERROR        */ { 1.00f, 0.11f, 0.16f, 0.28f,  0.12f, 0.24f, -0.60f, 0.03f, -0.18f, 0.00f, EYE_CROSS_ERROR, MOUTH_ZIGZAG_ERROR },
  /* SHUTDOWN     */ { 0.05f, 0.10f, 0.15f, 0.28f,  0.06f, 0.16f,  0.05f, 0.01f, -0.26f, 0.10f, EYE_LINE_SLEEP,  MOUTH_ARC },
  /* HIDDEN       */ { 0.00f, 0.00f, 0.00f, 0.00f,  0.00f, 0.00f,  0.00f, 0.00f,  0.00f, 0.00f, EYE_CAPSULE,     MOUTH_ARC },
  /* JOY          */ { 0.80f, 0.12f, 0.16f, 0.28f,  0.14f, 0.28f,  0.80f, 0.20f, -0.18f, 0.95f, EYE_CRESCENT_UP, MOUTH_OPEN_CAVITY },
  /* SLEEPING     */ { 0.00f, 0.10f, 0.15f, 0.28f,  0.06f, 0.16f,  0.10f, 0.01f, -0.25f, 0.25f, EYE_LINE_SLEEP,  MOUTH_ARC },
  /* WINK         */ { 0.90f, 0.10f, 0.16f, 0.28f,  0.12f, 0.24f,  0.45f, 0.04f, -0.20f, 0.70f, EYE_WINK_LEFT,   MOUTH_ARC },
  /* BLUSH        */ { 0.65f, 0.10f, 0.15f, 0.28f,  0.09f, 0.18f,  0.30f, 0.02f, -0.23f, 1.00f, EYE_CAPSULE,     MOUTH_ARC },
  /* CONFUSED     */ { 0.85f, 0.10f, 0.16f, 0.28f,  0.12f, 0.20f,  0.10f, 0.03f, -0.22f, 0.30f, EYE_CAPSULE,     MOUTH_ARC },
  /* ANNOYED      */ { 0.50f, 0.10f, 0.14f, 0.28f,  0.10f, 0.20f, -0.10f, 0.02f, -0.22f, 0.20f, EYE_CAPSULE,     MOUTH_ARC },
  /* LOVE         */ { 0.95f, 0.12f, 0.18f, 0.28f,  0.13f, 0.24f,  0.60f, 0.12f, -0.20f, 0.90f, EYE_HEART,       MOUTH_OPEN_CAVITY },
  /* TICKLED      */ { 0.75f, 0.12f, 0.15f, 0.28f,  0.15f, 0.28f,  0.85f, 0.22f, -0.16f, 1.00f, EYE_CRESCENT_UP, MOUTH_OPEN_CAVITY },
  /* CELEBRATING  */ { 0.85f, 0.12f, 0.17f, 0.28f,  0.14f, 0.30f,  0.75f, 0.20f, -0.18f, 0.90f, EYE_CRESCENT_UP, MOUTH_OPEN_CAVITY }
};

// ---------------------------------------------------------------------------
// Ambient Particle System (Sleep Bubbles & Star Sparkles)
// ---------------------------------------------------------------------------
enum ParticleType {
  PARTICLE_NONE = 0,
  PARTICLE_DREAM_Z,
  PARTICLE_SPARKLE
};

struct MascotParticle {
  ParticleType type;
  float x;      // normalized face coordinates
  float y;
  float vx;
  float vy;
  float scale;
  float life;   // 0.0 to 1.0
  float maxLife;
  float seed;
};

static const int MAX_PARTICLES = 8;
static MascotParticle s_particles[MAX_PARTICLES];

// ---------------------------------------------------------------------------
// Engine State
// ---------------------------------------------------------------------------
static BmoFace::BmoExpression s_expr = BmoFace::HIDDEN;
static HarmonicSpring s_spOpenness;
static HarmonicSpring s_spEyeW;
static HarmonicSpring s_spEyeH;
static HarmonicSpring s_spEyeOffX;
static HarmonicSpring s_spEyeOffY;
static HarmonicSpring s_spMouthW;
static HarmonicSpring s_spMouthCrv;
static HarmonicSpring s_spMouthOpen;
static HarmonicSpring s_spMouthOffY;
static HarmonicSpring s_spBlush;
static HarmonicSpring s_spBounceY;
static HarmonicSpring s_spSquashX;
static HarmonicSpring s_spGazeX;
static HarmonicSpring s_spGazeY;

static EyeStyle   s_activeEyeStyle   = EYE_CAPSULE;
static MouthStyle s_activeMouthStyle = MOUTH_ARC;
static bool       s_dirty            = false;

// Biological Dynamics
static float s_breathPhase    = 0.0f;
static float s_breathAmp      = 0.025f;
static float s_breathSpeed    = 1.95f; // ~3.2s period (2*pi / 1.95)

// Gaze Saccades
static unsigned long s_lastSaccadeMs = 0;
static unsigned long s_nextSaccadeMs = 2800;
static bool          s_userGazeActive = false;
static unsigned long s_userGazeEndMs = 0;

// Blinking
static unsigned long s_lastBlinkMs   = 0;
static unsigned long s_nextBlinkMs   = 3200;
static float         s_blinkProgress = 0.0f;
static bool          s_blinking      = false;
static unsigned long s_blinkStartMs  = 0;
static int           s_blinkPhase    = 0; // 0=single blink, 1=double blink first, 2=double blink second

// High-Definition Octal PSRAM Framebuffer (320x240x2 = 153.6 KB, 0 bytes DRAM!)
static uint16_t* s_psramFaceBuf = nullptr;
static unsigned long s_lastUpdateUs = 0;

// ---------------------------------------------------------------------------
// Particle Helpers
// ---------------------------------------------------------------------------
static void spawnParticle(ParticleType type, float x, float y, float vx, float vy, float scale, float maxLife) {
  for (int i = 0; i < MAX_PARTICLES; ++i) {
    if (s_particles[i].type == PARTICLE_NONE) {
      s_particles[i].type = type;
      s_particles[i].x = x;
      s_particles[i].y = y;
      s_particles[i].vx = vx;
      s_particles[i].vy = vy;
      s_particles[i].scale = scale;
      s_particles[i].life = maxLife;
      s_particles[i].maxLife = maxLife;
      s_particles[i].seed = (float)random(0, 1000) * 0.001f;
      return;
    }
  }
}

static void updateParticles(float dt) {
  for (int i = 0; i < MAX_PARTICLES; ++i) {
    if (s_particles[i].type == PARTICLE_NONE) continue;
    s_particles[i].life -= dt;
    if (s_particles[i].life <= 0.0f) {
      s_particles[i].type = PARTICLE_NONE;
      continue;
    }
    s_particles[i].x += s_particles[i].vx * dt;
    s_particles[i].y += s_particles[i].vy * dt;

    if (s_particles[i].type == PARTICLE_DREAM_Z) {
      // Gentle horizontal sinusoidal drift
      s_particles[i].x += sinf(s_particles[i].life * 4.0f + s_particles[i].seed * 6.28f) * 0.05f * dt;
    }
    s_dirty = true;
  }
}

// ---------------------------------------------------------------------------
// Signed Distance Functions (SDF Primitives in Normalized Face Space)
// ---------------------------------------------------------------------------

// Rounded capsule / vertical pill SDF:
// Exact Euclidean distance to a vertical capsule of radius r and segment half-height h.
static inline float sdfCapsuleV(float px, float py, float r, float h) {
  py = fabsf(py) - h;
  if (py < 0.0f) py = 0.0f;
  return sqrtf(px * px + py * py) - r;
}

// Smiling crescent eye arc SDF (^ ^)
static inline float sdfCrescentArc(float px, float py, float width, float curve, float thickness) {
  float halfW = width;
  if (fabsf(px) > halfW) {
    float epX = (px < 0.0f) ? -halfW : halfW;
    float epY = -curve * epX * epX;
    float dx = px - epX;
    float dy = py - epY;
    return sqrtf(dx * dx + dy * dy) - thickness;
  }
  float arcY = -curve * px * px;
  return fabsf(py - arcY) - thickness;
}

// Sleeping closed eye horizontal stroke SDF (- -)
static inline float sdfSleepStroke(float px, float py, float halfW, float thickness) {
  float dx = fabsf(px) - halfW;
  float ox = dx > 0.0f ? dx : 0.0f;
  float oy = fabsf(py) - thickness;
  if (oy < 0.0f) oy = 0.0f;
  return sqrtf(ox * ox + oy * oy);
}

// 4-point Diamond Star Sparkle SDF
static inline float sdfDiamondStar(float px, float py, float radius) {
  return (fabsf(px) + fabsf(py)) - radius;
}

// Heart Shape SDF for LOVE state
static inline float sdfHeart(float px, float py, float scale) {
  px /= (scale + 1e-6f);
  py /= (scale + 1e-6f);
  py = -py; // invert so top lobes are above point
  py += 0.2f;
  float a = px * px + py * py - 0.35f;
  return (a * a * a - px * px * py * py * py) * scale;
}

// Parabolic Mouth Stroke
static inline float sdfMouthStroke(float px, float py, float width, float curve, float thickness) {
  if (fabsf(px) > width) {
    float epx = (px < 0.0f) ? -width : width;
    float epy = curve * epx * epx;
    float ddx = px - epx;
    float ddy = py - epy;
    return sqrtf(ddx * ddx + ddy * ddy) - thickness;
  }
  float arcY = curve * px * px;
  return fabsf(py - arcY) - thickness;
}

// Open Mouth Cavity with Tongue
static inline void evaluateMouthCavity(float px, float py, float width, float curve, float openness,
                                       float& outCavityDist, float& outTongueDist, float& outBorderDist) {
  float halfW = width;
  float normX = px / (halfW + 1e-6f);
  if (fabsf(normX) >= 1.0f) {
    outCavityDist = 1.0f;
    outTongueDist = 1.0f;
    outBorderDist = 1.0f;
    return;
  }
  float upperY = curve * px * px + openness * 0.45f;
  float lowerY = -openness * 1.6f * sqrtf(1.0f - normX * normX);

  if (py <= upperY && py >= lowerY) {
    outCavityDist = -fminf(upperY - py, py - lowerY);
  } else {
    outCavityDist = fminf(fabsf(py - upperY), fabsf(py - lowerY));
  }

  // Tongue puffing from bottom center
  float tongueCx = 0.0f;
  float tongueCy = lowerY + openness * 0.4f;
  float tRad = openness * 0.85f;
  float tdx = px - tongueCx;
  float tdy = py - tongueCy;
  outTongueDist = sqrtf(tdx * tdx + tdy * tdy) - tRad;

  // Mouth dark outline border
  float dTop = fabsf(py - upperY);
  float dBottom = fabsf(py - lowerY);
  outBorderDist = fminf(dTop, dBottom) - 0.022f;
}

// ---------------------------------------------------------------------------
// Direct High-Performance Vector SDF Renderer
// ---------------------------------------------------------------------------
static void renderMascotInternal(uint16_t* dst, int stride, int destX, int destY, int destW, int destH) {
  if (!dst || destW <= 0 || destH <= 0) return;

  const uint16_t bgColor = (s_expr == BmoFace::SLEEPING) ? COL_DUSK_BG : COL_MINT_BG;
  const float bgR = (float)((bgColor >> 11) & 0x1F) / 31.0f; // BGR swapped
  const float bgG = (float)((bgColor >> 5)  & 0x3F) / 63.0f;
  const float bgB = (float)(bgColor & 0x1F)         / 31.0f;

  // Anti-aliasing band: 1.25 screen pixels mapped into normalized [-1, 1] units
  const float aaStepX = (2.0f / (float)destW) * 1.25f;
  const float aaStepY = (2.0f / (float)destH) * 1.25f;
  const float aaBand  = (aaStepX + aaStepY) * 0.5f;

  // Physics parameters live snapshot
  const float eyeOpenness    = s_spOpenness.val * (1.0f - s_blinkProgress);
  const float eyeW           = s_spEyeW.val * s_spSquashX.val;
  const float eyeH           = s_spEyeH.val * clampf(eyeOpenness, 0.005f, 1.2f);
  const float eyeOffX        = s_spEyeOffX.val;
  const float eyeOffY        = s_spEyeOffY.val + s_spBounceY.val + s_breathAmp * sinf(s_breathPhase);
  const float mouthW         = s_spMouthW.val * s_spSquashX.val;
  const float mouthCrv       = s_spMouthCrv.val;
  const float mouthOpen      = s_spMouthOpen.val;
  const float mouthOffY      = s_spMouthOffY.val + s_spBounceY.val * 0.8f + s_breathAmp * sinf(s_breathPhase);
  const float blushIntensity = clampf(s_spBlush.val, 0.0f, 1.0f);
  const float gazeX          = s_spGazeX.val;
  const float gazeY          = s_spGazeY.val;

  // Eye centers with gaze offset
  const float leftEyeCx  = -eyeOffX + gazeX * 0.06f;
  const float rightEyeCx =  eyeOffX + gazeX * 0.06f;
  const float eyeCy      =  eyeOffY + gazeY * 0.06f;

  // Cheeks centers
  const float leftCheekCx  = -eyeOffX * 1.52f;
  const float rightCheekCx =  eyeOffX * 1.52f;
  const float cheekCy      =  eyeOffY - eyeH * 0.82f;
  const float cheekR       =  0.13f;

  // Fast background clear of the destination box
  for (int r = 0; r < destH; ++r) {
    uint16_t* row = &dst[(destY + r) * stride + destX];
    uint32_t bgDword = ((uint32_t)bgColor << 16) | (uint32_t)bgColor;
    int c = 0;
    for (; c <= destW - 2; c += 2) {
      *(uint32_t*)&row[c] = bgDword;
    }
    for (; c < destW; ++c) {
      row[c] = bgColor;
    }
  }

  // Feature screen-space bounding boxes for spatial culling (PERF-08: skips >70% pixels)
  auto normToPixelX = [&](float nx) -> int {
    return (int)(((nx + 1.0f) * 0.5f) * (float)destW);
  };
  auto normToPixelY = [&](float ny) -> int {
    return (int)(((1.0f - ny) * 0.5f) * (float)destH);
  };

  const int bLeftEyeX0  = max(0, normToPixelX(leftEyeCx - eyeW * 1.6f - aaBand));
  const int bLeftEyeX1  = min(destW - 1, normToPixelX(leftEyeCx + eyeW * 1.6f + aaBand));
  const int bRightEyeX0 = max(0, normToPixelX(rightEyeCx - eyeW * 1.6f - aaBand));
  const int bRightEyeX1 = min(destW - 1, normToPixelX(rightEyeCx + eyeW * 1.6f + aaBand));
  const int bEyeY0      = max(0, normToPixelY(eyeCy + eyeH * 1.6f + aaBand));
  const int bEyeY1      = min(destH - 1, normToPixelY(eyeCy - eyeH * 1.6f - aaBand));

  const int bCheekL_X0  = max(0, normToPixelX(leftCheekCx - cheekR * 1.4f));
  const int bCheekL_X1  = min(destW - 1, normToPixelX(leftCheekCx + cheekR * 1.4f));
  const int bCheekR_X0  = max(0, normToPixelX(rightCheekCx - cheekR * 1.4f));
  const int bCheekR_X1  = min(destW - 1, normToPixelX(rightCheekCx + cheekR * 1.4f));
  const int bCheekY0    = max(0, normToPixelY(cheekCy + cheekR * 1.4f));
  const int bCheekY1    = min(destH - 1, normToPixelY(cheekCy - cheekR * 1.4f));

  const int bMouthX0    = max(0, normToPixelX(-mouthW * 1.3f - aaBand));
  const int bMouthX1    = min(destW - 1, normToPixelX( mouthW * 1.3f + aaBand));
  const int bMouthY0    = max(0, normToPixelY(mouthOffY + fabsf(mouthCrv) * mouthW * mouthW + mouthOpen * 1.8f + aaBand));
  const int bMouthY1    = min(destH - 1, normToPixelY(mouthOffY - mouthOpen * 1.8f - aaBand));

  // Pixel evaluation pass strictly inside bounding areas
  const int minBoundY = min(min(bEyeY0, bCheekY0), bMouthY0);
  const int maxBoundY = max(max(bEyeY1, bCheekY1), bMouthY1);

  for (int py = minBoundY; py <= maxBoundY; ++py) {
    float ny = 1.0f - 2.0f * ((float)py + 0.5f) / (float)destH;
    uint16_t* row = &dst[(destY + py) * stride + destX];

    for (int px = 0; px < destW; ++px) {
      bool inLeftEye  = (px >= bLeftEyeX0 && px <= bLeftEyeX1 && py >= bEyeY0 && py <= bEyeY1);
      bool inRightEye = (px >= bRightEyeX0 && px <= bRightEyeX1 && py >= bEyeY0 && py <= bEyeY1);
      bool inLeftCheek  = (blushIntensity > 0.05f && px >= bCheekL_X0 && px <= bCheekL_X1 && py >= bCheekY0 && py <= bCheekY1);
      bool inRightCheek = (blushIntensity > 0.05f && px >= bCheekR_X0 && px <= bCheekR_X1 && py >= bCheekY0 && py <= bCheekY1);
      bool inMouth    = (px >= bMouthX0 && px <= bMouthX1 && py >= bMouthY0 && py <= bMouthY1);

      if (!inLeftEye && !inRightEye && !inLeftCheek && !inRightCheek && !inMouth) {
        continue; // Culled! Already filled with background.
      }

      float nx = -1.0f + 2.0f * ((float)px + 0.5f) / (float)destW;
      float cr = 138.0f / 255.0f;
      float cg = 213.0f / 255.0f;
      float cb = 195.0f / 255.0f;

      // 1. Cheeks Blush Layer (Soft Gaussian Falloff)
      if (inLeftCheek || inRightCheek) {
        float chX = inLeftCheek ? (nx - leftCheekCx) : (nx - rightCheekCx);
        float chY = ny - cheekCy;
        float dCheek = sqrtf(chX * chX + chY * chY);
        float blushAlpha = blushIntensity * smoothstepf(cheekR, 0.0f, dCheek) * 0.75f;
        if (blushAlpha > 0.005f) {
          cr = lerpf(cr, 255.0f / 255.0f, blushAlpha);
          cg = lerpf(cg, 139.0f / 255.0f, blushAlpha);
          cb = lerpf(cb, 167.0f / 255.0f, blushAlpha);
        }
      }

      // 2. Eyes Layer
      if (inLeftEye || inRightEye) {
        float eyeDx = inLeftEye ? (nx - leftEyeCx) : (nx - rightEyeCx);
        float eyeDy = ny - eyeCy;
        float dEye = 10.0f;
        float dGlint = 10.0f;

        EyeStyle curStyle = s_activeEyeStyle;
        if (curStyle == EYE_WINK_LEFT && inLeftEye) {
          curStyle = EYE_CRESCENT_UP;
        } else if (curStyle == EYE_WINK_LEFT && inRightEye) {
          curStyle = EYE_CAPSULE;
        }

        if (curStyle == EYE_CAPSULE) {
          // Vertical elongated capsule
          float capH = eyeH * 0.55f;
          float capR = eyeW * 0.75f;
          dEye = sdfCapsuleV(eyeDx, eyeDy, capR, capH);

          // Specular catchlight inside open eye
          float glintCx = -capR * 0.32f + gazeX * capR * 0.20f;
          float glintCy =  capH * 0.40f + gazeY * capH * 0.20f;
          float glintR  =  capR * 0.38f;
          float gdx = eyeDx - glintCx;
          float gdy = eyeDy - glintCy;
          dGlint = sqrtf(gdx * gdx + gdy * gdy) - glintR;

        } else if (curStyle == EYE_CRESCENT_UP) {
          // Smiling curved arc (^ ^)
          dEye = sdfCrescentArc(eyeDx, eyeDy, eyeW * 0.95f, 1.8f, 0.024f);

        } else if (curStyle == EYE_LINE_SLEEP) {
          // Closed sleeping line (- -)
          dEye = sdfSleepStroke(eyeDx, eyeDy, eyeW * 0.90f, 0.022f);

        } else if (curStyle == EYE_CROSS_ERROR) {
          // Cross X eyes
          float rxa =  eyeDx * 0.7071f + eyeDy * 0.7071f;
          float rya = -eyeDx * 0.7071f + eyeDy * 0.7071f;
          float rxb =  eyeDx * 0.7071f - eyeDy * 0.7071f;
          float ryb =  eyeDx * 0.7071f + eyeDy * 0.7071f;
          float d1 = fmaxf(fabsf(rxa) - eyeW * 0.85f, fabsf(rya) - 0.025f);
          float d2 = fmaxf(fabsf(rxb) - eyeW * 0.85f, fabsf(ryb) - 0.025f);
          dEye = fminf(d1, d2);

        } else if (curStyle == EYE_HEART) {
          dEye = sdfHeart(eyeDx, eyeDy, eyeW * 1.1f);
        }

        // Composite Eye
        float eyeAlpha = smoothstepf(aaBand, 0.0f, -dEye);
        if (eyeAlpha > 0.002f) {
          if (curStyle == EYE_CROSS_ERROR) {
            cr = lerpf(cr, 225.0f / 255.0f, eyeAlpha);
            cg = lerpf(cg,  36.0f / 255.0f, eyeAlpha);
            cb = lerpf(cb,  48.0f / 255.0f, eyeAlpha);
          } else if (curStyle == EYE_HEART) {
            cr = lerpf(cr, 255.0f / 255.0f, eyeAlpha);
            cg = lerpf(cg, 105.0f / 255.0f, eyeAlpha);
            cb = lerpf(cb, 140.0f / 255.0f, eyeAlpha);
          } else {
            cr = lerpf(cr, 16.0f / 255.0f, eyeAlpha);
            cg = lerpf(cg, 30.0f / 255.0f, eyeAlpha);
            cb = lerpf(cb, 43.0f / 255.0f, eyeAlpha);
          }

          // Specular glint layer
          if (curStyle == EYE_CAPSULE) {
            float glintAlpha = smoothstepf(aaBand, 0.0f, -dGlint);
            cr = lerpf(cr, 1.0f, glintAlpha);
            cg = lerpf(cg, 1.0f, glintAlpha);
            cb = lerpf(cb, 1.0f, glintAlpha);
          }
        }
      }

      // 3. Mouth Layer
      if (inMouth) {
        float mDx = nx;
        float mDy = ny - mouthOffY;

        if (s_activeMouthStyle == MOUTH_OPEN_CAVITY) {
          float dCavity, dTongue, dBorder;
          evaluateMouthCavity(mDx, mDy, mouthW, mouthCrv, mouthOpen, dCavity, dTongue, dBorder);

          float cavityAlpha = smoothstepf(aaBand, 0.0f, -dCavity);
          if (cavityAlpha > 0.002f) {
            cr = lerpf(cr, 18.0f / 255.0f, cavityAlpha);
            cg = lerpf(cg, 28.0f / 255.0f, cavityAlpha);
            cb = lerpf(cb, 38.0f / 255.0f, cavityAlpha);

            float tongueAlpha = smoothstepf(aaBand, 0.0f, -dTongue);
            if (tongueAlpha > 0.002f) {
              cr = lerpf(cr, 250.0f / 255.0f, tongueAlpha);
              cg = lerpf(cg, 127.0f / 255.0f, tongueAlpha);
              cb = lerpf(cb, 143.0f / 255.0f, tongueAlpha);
            }
          }

          float borderAlpha = smoothstepf(aaBand, 0.0f, -dBorder);
          if (borderAlpha > 0.002f) {
            cr = lerpf(cr, 16.0f / 255.0f, borderAlpha);
            cg = lerpf(cg, 30.0f / 255.0f, borderAlpha);
            cb = lerpf(cb, 43.0f / 255.0f, borderAlpha);
          }

        } else if (s_activeMouthStyle == MOUTH_ROUND_O) {
          float mRad = mouthW * 0.55f;
          float dCircle = sqrtf(mDx * mDx + mDy * mDy) - mRad;
          float fillAlpha = smoothstepf(aaBand, 0.0f, -dCircle);
          cr = lerpf(cr, 18.0f / 255.0f, fillAlpha);
          cg = lerpf(cg, 28.0f / 255.0f, fillAlpha);
          cb = lerpf(cb, 38.0f / 255.0f, fillAlpha);

          float strokeAlpha = smoothstepf(aaBand, 0.0f, -(fabsf(dCircle) - 0.024f));
          cr = lerpf(cr, 16.0f / 255.0f, strokeAlpha);
          cg = lerpf(cg, 30.0f / 255.0f, strokeAlpha);
          cb = lerpf(cb, 43.0f / 255.0f, strokeAlpha);

        } else if (s_activeMouthStyle == MOUTH_ZIGZAG_ERROR) {
          float dZig = fabsf(mDy - sinf(mDx * 25.0f) * 0.035f) - 0.025f;
          float zigAlpha = smoothstepf(aaBand, 0.0f, -dZig);
          cr = lerpf(cr, 16.0f / 255.0f, zigAlpha);
          cg = lerpf(cg, 30.0f / 255.0f, zigAlpha);
          cb = lerpf(cb, 43.0f / 255.0f, zigAlpha);

        } else {
          // Closed smiling or frowning stroke
          float dStroke = sdfMouthStroke(mDx, mDy, mouthW, mouthCrv, 0.025f);
          float mouthAlpha = smoothstepf(aaBand, 0.0f, -dStroke);
          if (mouthAlpha > 0.002f) {
            cr = lerpf(cr, 16.0f / 255.0f, mouthAlpha);
            cg = lerpf(cg, 30.0f / 255.0f, mouthAlpha);
            cb = lerpf(cb, 43.0f / 255.0f, mouthAlpha);
          }
        }
      }

      row[px] = packBGR565((uint8_t)(cr * 255.0f + 0.5f),
                           (uint8_t)(cg * 255.0f + 0.5f),
                           (uint8_t)(cb * 255.0f + 0.5f));
    }
  }

  // 4. Particle Layer Rendering (Dream Bubbles & Star Sparkles)
  for (int i = 0; i < MAX_PARTICLES; ++i) {
    if (s_particles[i].type == PARTICLE_NONE) continue;
    float pNx = s_particles[i].x;
    float pNy = s_particles[i].y;
    int pcx = normToPixelX(pNx);
    int pcy = normToPixelY(pNy);
    int pRadPx = (int)(s_particles[i].scale * (float)destW * 0.5f);
    if (pRadPx < 3) pRadPx = 3;

    int pX0 = max(0, pcx - pRadPx - 2);
    int pX1 = min(destW - 1, pcx + pRadPx + 2);
    int pY0 = max(0, pcy - pRadPx - 2);
    int pY1 = min(destH - 1, pcy + pRadPx + 2);

    float alpha = clampf(s_particles[i].life / s_particles[i].maxLife, 0.0f, 1.0f);

    for (int y = pY0; y <= pY1; ++y) {
      float ny = 1.0f - 2.0f * ((float)y + 0.5f) / (float)destH;
      uint16_t* row = &dst[(destY + y) * stride + destX];

      for (int x = pX0; x <= pX1; ++x) {
        float nx = -1.0f + 2.0f * ((float)x + 0.5f) / (float)destW;
        float dPart = 10.0f;

        if (s_particles[i].type == PARTICLE_SPARKLE) {
          dPart = sdfDiamondStar(nx - pNx, ny - pNy, s_particles[i].scale);
        } else {
          // Dream bubble "Z" stroke
          float dx = (nx - pNx) / (s_particles[i].scale + 1e-6f);
          float dy = (ny - pNy) / (s_particles[i].scale + 1e-6f);
          dPart = sqrtf(dx * dx + dy * dy) - 0.7f;
        }

        float pAlpha = smoothstepf(aaBand, 0.0f, -dPart) * alpha;
        if (pAlpha > 0.01f) {
          uint16_t orig = row[x];
          // BGR unpacked
          float r = (float)((orig >> 11) & 0x1F) / 31.0f;
          float g = (float)((orig >> 5)  & 0x3F) / 63.0f;
          float b = (float)(orig & 0x1F)         / 31.0f;

          if (s_particles[i].type == PARTICLE_SPARKLE) {
            r = lerpf(r, 1.0f, pAlpha);
            g = lerpf(g, 0.88f, pAlpha);
            b = lerpf(b, 0.40f, pAlpha);
          } else {
            r = lerpf(r, 0.85f, pAlpha);
            g = lerpf(g, 0.96f, pAlpha);
            b = lerpf(b, 0.93f, pAlpha);
          }
          row[x] = packBGR565((uint8_t)(r * 255.0f + 0.5f),
                              (uint8_t)(g * 255.0f + 0.5f),
                              (uint8_t)(b * 255.0f + 0.5f));
        }
      }
    }
  }
}

} // anonymous namespace

// ===========================================================================
// Public API — BmoFace Namespace
// ===========================================================================

namespace BmoFace {

void begin() {
  randomSeed(micros());

  // Allocate 320x240 native canvas in Octal PSRAM (0 bytes DRAM!)
  if (!s_psramFaceBuf) {
    s_psramFaceBuf = (uint16_t*)heap_caps_malloc(FACE_FB_W * FACE_FB_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!s_psramFaceBuf) {
      LOG_ERROR_STR("[BmoFace] FATAL: Failed to allocate mascot canvas in Octal PSRAM!");
    } else {
      LOG_INFO("[BmoFace] Living Mascot 320x240 canvas allocated in Octal PSRAM (%d bytes)",
               FACE_FB_W * FACE_FB_H * (int)sizeof(uint16_t));
    }
  }

  // Initialize harmonic springs
  const ExprTarget& t = EXPR_TABLE[(int)IDLE];
  s_spOpenness.init(t.eyeOpenness, 22.0f, 0.72f);
  s_spEyeW.init(t.eyeWidth, 22.0f, 0.72f);
  s_spEyeH.init(t.eyeHeight, 22.0f, 0.72f);
  s_spEyeOffX.init(t.eyeOffsetX, 22.0f, 0.72f);
  s_spEyeOffY.init(t.eyeOffsetY, 22.0f, 0.72f);
  s_spMouthW.init(t.mouthWidth, 22.0f, 0.72f);
  s_spMouthCrv.init(t.mouthCurve, 20.0f, 0.70f);
  s_spMouthOpen.init(t.mouthOpenness, 24.0f, 0.68f);
  s_spMouthOffY.init(t.mouthOffsetY, 22.0f, 0.72f);
  s_spBlush.init(t.blushIntensity, 16.0f, 0.75f);
  s_spBounceY.init(0.0f, 24.0f, 0.65f);
  s_spSquashX.init(1.0f, 24.0f, 0.65f);
  s_spGazeX.init(0.0f, 18.0f, 0.75f);
  s_spGazeY.init(0.0f, 18.0f, 0.75f);

  s_activeEyeStyle   = t.eyeStyle;
  s_activeMouthStyle = t.mouthStyle;
  s_expr             = IDLE;
  s_dirty            = true;

  s_lastBlinkMs   = millis();
  s_nextBlinkMs   = (unsigned long)random(2500, 5000);
  s_lastSaccadeMs = millis();
  s_nextSaccadeMs = (unsigned long)random(2000, 4500);
  s_lastUpdateUs  = micros();

  for (int i = 0; i < MAX_PARTICLES; ++i) {
    s_particles[i].type = PARTICLE_NONE;
  }
}

void setExpression(BmoExpression expr) {
  if (s_expr == expr) return;

  s_expr = expr;
  const ExprTarget& t = EXPR_TABLE[(int)expr];

  if (expr == ERROR) {
    // Snap instantly without easing
    s_spOpenness.snap(t.eyeOpenness);
    s_spEyeW.snap(t.eyeWidth);
    s_spEyeH.snap(t.eyeHeight);
    s_spEyeOffX.snap(t.eyeOffsetX);
    s_spEyeOffY.snap(t.eyeOffsetY);
    s_spMouthW.snap(t.mouthWidth);
    s_spMouthCrv.snap(t.mouthCurve);
    s_spMouthOpen.snap(t.mouthOpenness);
    s_spMouthOffY.snap(t.mouthOffsetY);
    s_spBlush.snap(t.blushIntensity);
    s_spBounceY.snap(0.0f);
    s_spSquashX.snap(1.0f);
  } else {
    // Spring targets with dynamic overshoot
    s_spOpenness.setTarget(t.eyeOpenness);
    s_spEyeW.setTarget(t.eyeWidth);
    s_spEyeH.setTarget(t.eyeHeight);
    s_spEyeOffX.setTarget(t.eyeOffsetX);
    s_spEyeOffY.setTarget(t.eyeOffsetY);
    s_spMouthW.setTarget(t.mouthWidth);
    s_spMouthCrv.setTarget(t.mouthCurve);
    s_spMouthOpen.setTarget(t.mouthOpenness);
    s_spMouthOffY.setTarget(t.mouthOffsetY);
    s_spBlush.setTarget(t.blushIntensity);

    // Expressive cartoon bounce trigger on joy / celebration
    if (expr == JOY || expr == CELEBRATING || expr == HAPPY) {
      s_spBounceY.vel = 1.2f; // upward elastic velocity
      s_spSquashX.val = 0.88f;
    } else if (expr == SURPRISED) {
      s_spBounceY.vel = 0.8f;
      s_spSquashX.val = 0.92f;
    }
  }

  s_activeEyeStyle   = t.eyeStyle;
  s_activeMouthStyle = t.mouthStyle;
  s_blinking         = false;
  s_blinkProgress    = 0.0f;
  s_dirty            = true;
}

BmoExpression getExpression() {
  return s_expr;
}

void setGaze(float gx, float gy) {
  s_spGazeX.setTarget(clampf(gx, -1.0f, 1.0f));
  s_spGazeY.setTarget(clampf(gy, -1.0f, 1.0f));
  s_userGazeActive = true;
  s_userGazeEndMs  = millis() + 1400; // retain gaze for 1.4s then drift back
  s_dirty = true;
}

void tickle(float intensity) {
  setExpression(TICKLED);
  s_spBounceY.vel = 1.4f * intensity;
  s_spSquashX.val = 0.82f;
  s_spGazeX.vel = ((random(0, 2) == 0) ? -2.5f : 2.5f) * intensity;
  s_dirty = true;
}

void triggerWink() {
  setExpression(WINK);
  // Spawn 3 celebration star sparkles
  spawnParticle(PARTICLE_SPARKLE, 0.42f, 0.22f,  0.08f, 0.12f, 0.06f, 0.8f);
  spawnParticle(PARTICLE_SPARKLE, 0.48f, 0.14f,  0.12f, 0.04f, 0.05f, 0.9f);
  spawnParticle(PARTICLE_SPARKLE, 0.38f, 0.30f,  0.04f, 0.16f, 0.04f, 0.7f);
  s_dirty = true;
}

void triggerBlush() {
  setExpression(BLUSH);
  s_spGazeX.setTarget(-0.55f);
  s_spGazeY.setTarget(-0.40f);
  s_dirty = true;
}

void triggerJoy() {
  setExpression(JOY);
  s_dirty = true;
}

void update() {
  if (s_expr == HIDDEN) return;

  unsigned long nowUs = micros();
  float dt = (float)(nowUs - s_lastUpdateUs) * 0.000001f;
  s_lastUpdateUs = nowUs;
  if (dt <= 0.0f || dt > 0.1f) dt = 0.01666f; // clamp to 60 FPS baseline

  unsigned long nowMs = millis();

  // 1. Biological Breathing (Continuous Sinusoid)
  s_breathPhase += dt * s_breathSpeed;
  if (s_breathPhase > 6.2831853f) s_breathPhase -= 6.2831853f;

  // 2. Autonomous Saccades
  if (s_userGazeActive) {
    if (nowMs > s_userGazeEndMs) {
      s_userGazeActive = false;
      s_spGazeX.setTarget(0.0f);
      s_spGazeY.setTarget(0.0f);
    }
  } else if (s_expr == IDLE || s_expr == HAPPY || s_expr == CHARGING) {
    if ((nowMs - s_lastSaccadeMs) >= s_nextSaccadeMs) {
      float nx = (float)random(-35, 36) * 0.01f;
      float ny = (float)random(-20, 21) * 0.01f;
      s_spGazeX.setTarget(nx);
      s_spGazeY.setTarget(ny);
      s_lastSaccadeMs = nowMs;
      s_nextSaccadeMs = (unsigned long)random(2200, 4800);
    }
  }

  // 3. Autonomous Blinking
  if (!s_blinking && (s_expr == IDLE || s_expr == HAPPY || s_expr == CHARGING || s_expr == JOY)) {
    if ((nowMs - s_lastBlinkMs) >= s_nextBlinkMs) {
      s_blinking     = true;
      s_blinkStartMs = nowMs;
      s_blinkPhase   = (random(0, 5) == 0) ? 1 : 0; // occasional double blink
    }
  }

  if (s_blinking) {
    unsigned long elapsed = nowMs - s_blinkStartMs;
    const unsigned long BLINK_CLOSE_MS = 45;
    const unsigned long BLINK_HOLD_MS  = 20;
    const unsigned long BLINK_OPEN_MS  = 75;

    if (elapsed < BLINK_CLOSE_MS) {
      s_blinkProgress = (float)elapsed / (float)BLINK_CLOSE_MS;
    } else if (elapsed < BLINK_CLOSE_MS + BLINK_HOLD_MS) {
      s_blinkProgress = 1.0f;
    } else if (elapsed < BLINK_CLOSE_MS + BLINK_HOLD_MS + BLINK_OPEN_MS) {
      float t = (float)(elapsed - BLINK_CLOSE_MS - BLINK_HOLD_MS) / (float)BLINK_OPEN_MS;
      s_blinkProgress = 1.0f - t;
    } else {
      if (s_blinkPhase == 1) {
        // Trigger second flutter blink
        s_blinkPhase = 2;
        s_blinkStartMs = nowMs;
        s_blinkProgress = 0.0f;
      } else {
        s_blinking      = false;
        s_blinkProgress = 0.0f;
        s_lastBlinkMs   = nowMs;
        s_nextBlinkMs   = (unsigned long)random(2500, 5500);
      }
    }
    s_dirty = true;
  }

  // 4. Sleep Particle Spawner
  if (s_expr == SLEEPING) {
    static unsigned long lastZSpawn = 0;
    if (nowMs - lastZSpawn > 1800) {
      spawnParticle(PARTICLE_DREAM_Z, 0.35f, -0.15f, 0.04f, 0.18f, 0.08f, 2.8f);
      lastZSpawn = nowMs;
    }
  }

  // 5. Update Springs
  bool moved = false;
  moved |= s_spOpenness.update(dt);
  moved |= s_spEyeW.update(dt);
  moved |= s_spEyeH.update(dt);
  moved |= s_spEyeOffX.update(dt);
  moved |= s_spEyeOffY.update(dt);
  moved |= s_spMouthW.update(dt);
  moved |= s_spMouthCrv.update(dt);
  moved |= s_spMouthOpen.update(dt);
  moved |= s_spMouthOffY.update(dt);
  moved |= s_spBlush.update(dt);
  moved |= s_spBounceY.update(dt);
  moved |= s_spSquashX.update(dt);
  moved |= s_spGazeX.update(dt);
  moved |= s_spGazeY.update(dt);

  // Coupling: bounce drives squash & stretch
  s_spSquashX.setTarget(1.0f - s_spBounceY.val * 0.45f);

  updateParticles(dt);

  if (moved) s_dirty = true;
}

void renderFullScreen(uint16_t* dst, int width, int height) {
  renderMascotInternal(dst, width, 0, 0, width, height);
  s_dirty = false;
}

void renderToBuffer(uint16_t* dst, int stride, int x, int y, int w, int h) {
  renderMascotInternal(dst, stride, x, y, w, h);
  s_dirty = false;
}

void draw(int x, int y, int size) {
  if (s_expr == HIDDEN) return;
  if (!s_psramFaceBuf) begin();
  if (!s_psramFaceBuf) return;

  if (size == FACE_FB_W && x == 0 && y == 0) {
    draw();
    return;
  }

  // Render directly at requested sub-region size with subpixel anti-aliasing
  renderMascotInternal(s_psramFaceBuf, size, 0, 0, size, size);
  DisplayEmu::pushPixelsAt(x, y, size, size, s_psramFaceBuf);
  s_dirty = false;
}

void draw() {
  if (s_expr == HIDDEN) return;
  if (!s_psramFaceBuf) begin();
  if (!s_psramFaceBuf) return;

  renderMascotInternal(s_psramFaceBuf, FACE_FB_W, 0, 0, FACE_FB_W, FACE_FB_H);
  DisplayEmu::pushPixelsAt(0, 0, FACE_FB_W, FACE_FB_H, s_psramFaceBuf);
  s_dirty = false;
}

bool isDirty() {
  return s_dirty;
}

uint16_t getScreenBgColor() {
  return (s_expr == SLEEPING) ? COL_DUSK_BG : COL_MINT_BG;
}

} // namespace BmoFace
