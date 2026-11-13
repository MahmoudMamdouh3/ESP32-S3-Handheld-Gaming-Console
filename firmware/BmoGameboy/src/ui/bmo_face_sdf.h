#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

namespace BmoFaceSdf {

inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

inline float smoothstepf(float lo, float hi, float t) {
  const float span = hi - lo;
  if (fabsf(span) < 1e-7f) {
    return t >= hi ? 1.0f : 0.0f;
  }
  const float u = clampf((t - lo) / span, 0.0f, 1.0f);
  return u * u * (3.0f - 2.0f * u);
}

inline float lerpf(float a, float b, float t) {
  return a + (b - a) * t;
}

/**
 * Compute normalized sub-pixel coverage in [0, 1] from signed distance `dist`
 * and anti-aliasing feather width `aaBand` (`dist <= 0` -> inside -> 1.0,
 * `dist >= aaBand` -> outside -> 0.0).
 */
inline float coverage(float dist, float aaBand) {
  if (aaBand <= 0.0f) {
    return (dist <= 0.0f) ? 1.0f : 0.0f;
  }
  return smoothstepf(aaBand, 0.0f, dist);
}

enum EyeStyle : uint8_t { EYE_CAPSULE = 0, EYE_CRESCENT_UP, EYE_LINE_SLEEP, EYE_HEART, EYE_CROSS_ERROR, EYE_WINK_LEFT };

enum MouthStyle : uint8_t { MOUTH_ARC = 0, MOUTH_OPEN_CAVITY, MOUTH_ROUND_O, MOUTH_ZIGZAG_ERROR };

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

inline constexpr ExprTarget EXPR_TABLE[18] = {
    /* 0  IDLE         */ {0.90f, 0.10f, 0.16f, 0.28f, 0.12f, 0.22f, 0.35f, 0.02f, -0.22f, 0.35f, EYE_CAPSULE,
                           MOUTH_ARC},
    /* 1  SURPRISED    */
    {1.15f, 0.11f, 0.20f, 0.28f, 0.15f, 0.12f, 0.00f, 0.14f, -0.25f, 0.20f, EYE_CAPSULE, MOUTH_ROUND_O},
    /* 2  HAPPY        */
    {0.70f, 0.11f, 0.15f, 0.28f, 0.12f, 0.26f, 0.65f, 0.16f, -0.20f, 0.85f, EYE_CRESCENT_UP, MOUTH_OPEN_CAVITY},
    /* 3  SLEEPY       */
    {0.30f, 0.10f, 0.15f, 0.28f, 0.08f, 0.18f, 0.15f, 0.02f, -0.24f, 0.20f, EYE_CAPSULE, MOUTH_ARC},
    /* 4  LOW_BATTERY  */
    {0.45f, 0.09f, 0.14f, 0.28f, 0.06f, 0.20f, -0.30f, 0.02f, -0.25f, 0.15f, EYE_CAPSULE, MOUTH_ARC},
    /* 5  CHARGING     */
    {0.85f, 0.10f, 0.16f, 0.28f, 0.12f, 0.22f, 0.30f, 0.02f, -0.22f, 0.40f, EYE_CAPSULE, MOUTH_ARC},
    /* 6  ERROR        */
    {1.00f, 0.11f, 0.16f, 0.28f, 0.12f, 0.24f, -0.60f, 0.03f, -0.18f, 0.00f, EYE_CROSS_ERROR, MOUTH_ZIGZAG_ERROR},
    /* 7  SHUTDOWN     */
    {0.05f, 0.10f, 0.15f, 0.28f, 0.06f, 0.16f, 0.05f, 0.01f, -0.26f, 0.10f, EYE_LINE_SLEEP, MOUTH_ARC},
    /* 8  HIDDEN       */
    {0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, EYE_CAPSULE, MOUTH_ARC},
    /* 9  JOY          */
    {0.80f, 0.12f, 0.16f, 0.28f, 0.14f, 0.28f, 0.80f, 0.20f, -0.18f, 0.95f, EYE_CRESCENT_UP, MOUTH_OPEN_CAVITY},
    /* 10 SLEEPING     */
    {0.00f, 0.10f, 0.15f, 0.28f, 0.06f, 0.16f, 0.10f, 0.01f, -0.25f, 0.25f, EYE_LINE_SLEEP, MOUTH_ARC},
    /* 11 WINK         */
    {0.90f, 0.10f, 0.16f, 0.28f, 0.12f, 0.24f, 0.45f, 0.04f, -0.20f, 0.70f, EYE_WINK_LEFT, MOUTH_ARC},
    /* 12 BLUSH        */
    {0.65f, 0.10f, 0.15f, 0.28f, 0.09f, 0.18f, 0.30f, 0.02f, -0.23f, 1.00f, EYE_CAPSULE, MOUTH_ARC},
    /* 13 CONFUSED     */
    {0.85f, 0.10f, 0.16f, 0.28f, 0.12f, 0.20f, 0.10f, 0.03f, -0.22f, 0.30f, EYE_CAPSULE, MOUTH_ARC},
    /* 14 ANNOYED      */
    {0.50f, 0.10f, 0.14f, 0.28f, 0.10f, 0.20f, -0.10f, 0.02f, -0.22f, 0.20f, EYE_CAPSULE, MOUTH_ARC},
    /* 15 LOVE         */
    {0.95f, 0.12f, 0.18f, 0.28f, 0.13f, 0.24f, 0.60f, 0.12f, -0.20f, 0.90f, EYE_HEART, MOUTH_OPEN_CAVITY},
    /* 16 TICKLED      */
    {0.75f, 0.12f, 0.15f, 0.28f, 0.15f, 0.28f, 0.85f, 0.22f, -0.16f, 1.00f, EYE_CRESCENT_UP, MOUTH_OPEN_CAVITY},
    /* 17 CELEBRATING  */
    {0.85f, 0.12f, 0.17f, 0.28f, 0.14f, 0.30f, 0.75f, 0.20f, -0.18f, 0.90f, EYE_CRESCENT_UP, MOUTH_OPEN_CAVITY}};

inline float sdfCapsuleV(float px, float py, float r, float h) {
  py = fabsf(py) - h;
  if (py < 0.0f) py = 0.0f;
  return sqrtf(px * px + py * py) - r;
}

inline float sdfCrescentArc(float px, float py, float width, float curve, float thickness) {
  const float halfW = width;
  if (fabsf(px) > halfW) {
    const float epX = (px < 0.0f) ? -halfW : halfW;
    const float epY = -curve * epX * epX;
    const float dx = px - epX;
    const float dy = py - epY;
    return sqrtf(dx * dx + dy * dy) - thickness;
  }
  const float arcY = -curve * px * px;
  return fabsf(py - arcY) - thickness;
}

inline float sdfSleepStroke(float px, float py, float halfW, float thickness) {
  const float dx = fabsf(px) - halfW;
  const float ox = dx > 0.0f ? dx : 0.0f;
  float oy = fabsf(py) - thickness;
  if (oy < 0.0f) oy = 0.0f;
  return sqrtf(ox * ox + oy * oy);
}

inline float sdfDiamondStar(float px, float py, float radius) {
  return (fabsf(px) + fabsf(py)) - radius;
}

inline float sdfRoundRect(float px, float py, float hw, float hh, float r) {
  const float dx = fabsf(px) - hw + r;
  const float dy = fabsf(py) - hh + r;
  const float ox = dx > 0.0f ? dx : 0.0f;
  const float oy = dy > 0.0f ? dy : 0.0f;
  const float inside = (dx < 0.0f && dy < 0.0f) ? (dx > dy ? dx : dy) : 0.0f;
  return sqrtf(ox * ox + oy * oy) + inside - r;
}

inline float sdfHeart(float px, float py, float scale) {
  px /= (scale + 1e-6f);
  py /= (scale + 1e-6f);
  py = -py + 0.2f;
  const float a = px * px + py * py - 0.35f;
  return (a * a * a - px * px * py * py * py) * scale;
}

inline float sdfMouthStroke(float px, float py, float width, float curve, float thickness) {
  if (fabsf(px) > width) {
    const float epx = (px < 0.0f) ? -width : width;
    const float epy = curve * epx * epx;
    const float ddx = px - epx;
    const float ddy = py - epy;
    return sqrtf(ddx * ddx + ddy * ddy) - thickness;
  }
  const float arcY = curve * px * px;
  return fabsf(py - arcY) - thickness;
}

inline void evaluateMouthCavity(float px, float py, float width, float curve, float openness, float& outCavityDist,
                                float& outTongueDist, float& outBorderDist) {
  const float normX = px / (width + 1e-6f);
  if (fabsf(normX) >= 1.0f) {
    outCavityDist = 1.0f;
    outTongueDist = 1.0f;
    outBorderDist = 1.0f;
    return;
  }
  const float upperY = curve * px * px + openness * 0.45f;
  const float lowerY = -openness * 1.6f * sqrtf(1.0f - normX * normX);

  if (py <= upperY && py >= lowerY) {
    outCavityDist = -fminf(upperY - py, py - lowerY);
  } else {
    outCavityDist = fminf(fabsf(py - upperY), fabsf(py - lowerY));
  }

  const float tongueCy = lowerY + openness * 0.4f;
  const float tRad = openness * 0.85f;
  const float tdy = py - tongueCy;
  outTongueDist = sqrtf(px * px + tdy * tdy) - tRad;

  const float dTop = fabsf(py - upperY);
  const float dBottom = fabsf(py - lowerY);
  outBorderDist = fminf(dTop, dBottom) - 0.022f;
}

inline uint16_t packBGR565(uint8_t r, uint8_t g, uint8_t b) {
  const uint16_t bgr =
      static_cast<uint16_t>(((uint16_t)(b & 0xF8U) << 8) | ((uint16_t)(g & 0xFCU) << 3) | ((uint16_t)r >> 3));
  return static_cast<uint16_t>((bgr << 8) | (bgr >> 8));
}

/**
 * Pure C++ golden/preview renderer for an ExprTarget snapshot (no globals, no PSRAM).
 * Evaluates all pixels in `dst[destW x destH]` for deterministic host comparison.
 */
inline void renderTargetSnapshot(const ExprTarget& target, bool sleepingBg, uint16_t* dst, int destW, int destH) {
  if (!dst || destW <= 0 || destH <= 0) return;
  const float baseBgR = sleepingBg ? (111.0f / 255.0f) : (138.0f / 255.0f);
  const float baseBgG = sleepingBg ? (184.0f / 255.0f) : (213.0f / 255.0f);
  const float baseBgB = sleepingBg ? (167.0f / 255.0f) : (195.0f / 255.0f);
  const float aaBand = ((2.0f / (float)destW) * 1.25f + (2.0f / (float)destH) * 1.25f) * 0.5f;

  const float eyeH = target.eyeHeight * clampf(target.eyeOpenness, 0.005f, 1.2f);
  const float eyeW = target.eyeWidth;
  const float leftEyeCx = -target.eyeOffsetX;
  const float rightEyeCx = target.eyeOffsetX;
  const float eyeCy = target.eyeOffsetY;
  const float blush = clampf(target.blushIntensity, 0.0f, 1.0f);
  const float leftCheekCx = -target.eyeOffsetX * 1.52f;
  const float rightCheekCx = target.eyeOffsetX * 1.52f;
  const float cheekCy = eyeCy - eyeH * 0.82f;
  const float cheekR = 0.13f;

  for (int py = 0; py < destH; ++py) {
    const float ny = 1.0f - 2.0f * ((float)py + 0.5f) / (float)destH;
    uint16_t* row = &dst[py * destW];
    for (int px = 0; px < destW; ++px) {
      const float nx = -1.0f + 2.0f * ((float)px + 0.5f) / (float)destW;
      float cr = baseBgR;
      float cg = baseBgG;
      float cb = baseBgB;

      if (blush > 0.05f) {
        for (int side = 0; side < 2; ++side) {
          const float cx = (side == 0) ? leftCheekCx : rightCheekCx;
          const float dx = nx - cx;
          const float dy = ny - cheekCy;
          const float dCheek = sqrtf(dx * dx + dy * dy);
          const float a = blush * smoothstepf(cheekR, 0.0f, dCheek) * 0.75f;
          if (a > 0.005f) {
            cr = lerpf(cr, 1.0f, a);
            cg = lerpf(cg, 139.0f / 255.0f, a);
            cb = lerpf(cb, 167.0f / 255.0f, a);
          }
        }
      }

      if (target.eyeWidth > 0.0f && target.eyeHeight > 0.0f) {
        for (int side = 0; side < 2; ++side) {
          const bool isLeft = (side == 0);
          const float edx = nx - (isLeft ? leftEyeCx : rightEyeCx);
          const float edy = ny - eyeCy;
          EyeStyle st = target.eyeStyle;
          if (st == EYE_WINK_LEFT) st = isLeft ? EYE_CRESCENT_UP : EYE_CAPSULE;

          float dEye = 10.0f;
          float dGlint = 10.0f;
          if (st == EYE_CAPSULE) {
            const float capH = eyeH * 0.55f;
            const float capR = eyeW * 0.75f;
            dEye = sdfCapsuleV(edx, edy, capR, capH);
            const float gx = edx + capR * 0.32f;
            const float gy = edy - capH * 0.40f;
            dGlint = sqrtf(gx * gx + gy * gy) - capR * 0.38f;
          } else if (st == EYE_CRESCENT_UP) {
            dEye = sdfCrescentArc(edx, edy, eyeW * 0.95f, 1.8f, 0.024f);
          } else if (st == EYE_LINE_SLEEP) {
            dEye = sdfSleepStroke(edx, edy, eyeW * 0.90f, 0.022f);
          } else if (st == EYE_CROSS_ERROR) {
            const float rxa = edx * 0.7071f + edy * 0.7071f;
            const float rya = -edx * 0.7071f + edy * 0.7071f;
            const float rxb = edx * 0.7071f - edy * 0.7071f;
            const float ryb = edx * 0.7071f + edy * 0.7071f;
            const float d1 = fmaxf(fabsf(rxa) - eyeW * 0.85f, fabsf(rya) - 0.025f);
            const float d2 = fmaxf(fabsf(rxb) - eyeW * 0.85f, fabsf(ryb) - 0.025f);
            dEye = fminf(d1, d2);
          } else if (st == EYE_HEART) {
            dEye = sdfHeart(edx, edy, eyeW * 1.1f);
          }

          const float eyeAlpha = coverage(dEye, aaBand);
          if (eyeAlpha > 0.002f) {
            if (st == EYE_CROSS_ERROR) {
              cr = lerpf(cr, 225.0f / 255.0f, eyeAlpha);
              cg = lerpf(cg, 36.0f / 255.0f, eyeAlpha);
              cb = lerpf(cb, 48.0f / 255.0f, eyeAlpha);
            } else if (st == EYE_HEART) {
              cr = lerpf(cr, 1.0f, eyeAlpha);
              cg = lerpf(cg, 105.0f / 255.0f, eyeAlpha);
              cb = lerpf(cb, 140.0f / 255.0f, eyeAlpha);
            } else {
              cr = lerpf(cr, 16.0f / 255.0f, eyeAlpha);
              cg = lerpf(cg, 30.0f / 255.0f, eyeAlpha);
              cb = lerpf(cb, 43.0f / 255.0f, eyeAlpha);
            }
            if (st == EYE_CAPSULE) {
              const float glintAlpha = coverage(dGlint, aaBand);
              cr = lerpf(cr, 1.0f, glintAlpha);
              cg = lerpf(cg, 1.0f, glintAlpha);
              cb = lerpf(cb, 1.0f, glintAlpha);
            }
          }
        }
      }

      if (target.mouthWidth > 0.0f) {
        const float mdx = nx;
        const float mdy = ny - target.mouthOffsetY;
        if (target.mouthStyle == MOUTH_OPEN_CAVITY) {
          float dc, dt, db;
          evaluateMouthCavity(mdx, mdy, target.mouthWidth, target.mouthCurve, target.mouthOpenness, dc, dt, db);
          const float ca = coverage(dc, aaBand);
          if (ca > 0.002f) {
            cr = lerpf(cr, 18.0f / 255.0f, ca);
            cg = lerpf(cg, 28.0f / 255.0f, ca);
            cb = lerpf(cb, 38.0f / 255.0f, ca);
            const float ta = coverage(dt, aaBand);
            if (ta > 0.002f) {
              cr = lerpf(cr, 250.0f / 255.0f, ta);
              cg = lerpf(cg, 127.0f / 255.0f, ta);
              cb = lerpf(cb, 143.0f / 255.0f, ta);
            }
          }
          const float ba = coverage(db, aaBand);
          if (ba > 0.002f) {
            cr = lerpf(cr, 16.0f / 255.0f, ba);
            cg = lerpf(cg, 30.0f / 255.0f, ba);
            cb = lerpf(cb, 43.0f / 255.0f, ba);
          }
        } else if (target.mouthStyle == MOUTH_ROUND_O) {
          const float dCircle = sqrtf(mdx * mdx + mdy * mdy) - target.mouthWidth * 0.55f;
          const float fa = coverage(dCircle, aaBand);
          cr = lerpf(cr, 18.0f / 255.0f, fa);
          cg = lerpf(cg, 28.0f / 255.0f, fa);
          cb = lerpf(cb, 38.0f / 255.0f, fa);
          const float sa = coverage(fabsf(dCircle) - 0.024f, aaBand);
          cr = lerpf(cr, 16.0f / 255.0f, sa);
          cg = lerpf(cg, 30.0f / 255.0f, sa);
          cb = lerpf(cb, 43.0f / 255.0f, sa);
        } else if (target.mouthStyle == MOUTH_ZIGZAG_ERROR) {
          const float dZig = fabsf(mdy - sinf(mdx * 25.0f) * 0.035f) - 0.025f;
          const float za = coverage(dZig, aaBand);
          cr = lerpf(cr, 16.0f / 255.0f, za);
          cg = lerpf(cg, 30.0f / 255.0f, za);
          cb = lerpf(cb, 43.0f / 255.0f, za);
        } else {
          const float ma = coverage(sdfMouthStroke(mdx, mdy, target.mouthWidth, target.mouthCurve, 0.025f), aaBand);
          if (ma > 0.002f) {
            cr = lerpf(cr, 16.0f / 255.0f, ma);
            cg = lerpf(cg, 30.0f / 255.0f, ma);
            cb = lerpf(cb, 43.0f / 255.0f, ma);
          }
        }
      }

      row[px] = packBGR565(static_cast<uint8_t>(cr * 255.0f + 0.5f), static_cast<uint8_t>(cg * 255.0f + 0.5f),
                           static_cast<uint8_t>(cb * 255.0f + 0.5f));
    }
  }
}

} // namespace BmoFaceSdf
