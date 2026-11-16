// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3,unroll-loops")
#endif

#include "bmo_face.h"
#include "bmo_face_internal.h"
#include "bmo_face_sdf.h"
#include "display_emu.h"
#include <math.h>
#include <stdint.h>

namespace BmoFaceRenderDetail {

using namespace BmoFaceInternal;
using BmoFaceSdf::clampf;
using BmoFaceSdf::coverage;
using BmoFaceSdf::lerpf;
using BmoFaceSdf::packBGR565;
using BmoFaceSdf::sdfDiamondStar;
using BmoFaceSdf::sdfRoundRect;

inline int clampInt(int v, int lo, int hi) {
  return (v < lo) ? lo : ((v > hi) ? hi : v);
}

inline uint8_t floatToByte(float v) {
  return static_cast<uint8_t>(lroundf(v * 255.0f));
}

inline int minInt(int a, int b) {
  return (a < b) ? a : b;
}

inline int maxInt(int a, int b) {
  return (a > b) ? a : b;
}

struct FaceFrameParams {
  float aaBand;
  float baseBgR;
  float baseBgG;
  float baseBgB;
  float eyeW;
  float eyeH;
  float eyeCy;
  float leftEyeCx;
  float rightEyeCx;
  float leftCheekCx;
  float rightCheekCx;
  float cheekCy;
  float cheekR;
  float mouthW;
  float mouthCrv;
  float mouthOpen;
  float mouthOffY;
  float blushIntensity;
  float gazeX;
  float gazeY;
};

struct FaceBounds {
  int bLeftEyeX0, bLeftEyeX1;
  int bRightEyeX0, bRightEyeX1;
  int bEyeY0, bEyeY1;
  int bCheekL_X0, bCheekL_X1;
  int bCheekR_X0, bCheekR_X1;
  int bCheekY0, bCheekY1;
  int bMouthX0, bMouthX1, bMouthY0, bMouthY1;
  int bBubbleX0, bBubbleX1, bBubbleY0, bBubbleY1;
  int minBoundY, maxBoundY;
};

void fillFaceBackground(uint16_t* dst, int stride, int destX, int destY, int destW, int destH, uint16_t bgColor) {
  const uint32_t bgDword = (static_cast<uint32_t>(bgColor) << 16) | static_cast<uint32_t>(bgColor);
  for (int r = 0; r < destH; ++r) {
    uint16_t* row = &dst[(destY + r) * stride + destX];
    int c = 0;
    if (c < destW && (reinterpret_cast<uintptr_t>(&row[c]) & 3u) != 0) {
      row[c++] = bgColor;
    }
    for (; c <= destW - 2; c += 2) {
      *reinterpret_cast<uint32_t*>(&row[c]) = bgDword;
    }
    for (; c < destW; ++c) {
      row[c] = bgColor;
    }
  }
}

FaceFrameParams snapshotParams(int destW, int destH) {
  FaceFrameParams p{};
  const bool isSleepingBg = (s_face.expr == BmoFace::SLEEPING);
  p.baseBgR = isSleepingBg ? (111.0f / 255.0f) : (138.0f / 255.0f);
  p.baseBgG = isSleepingBg ? (184.0f / 255.0f) : (213.0f / 255.0f);
  p.baseBgB = isSleepingBg ? (167.0f / 255.0f) : (195.0f / 255.0f);
  const float aaStepX = (2.0f / static_cast<float>(destW)) * 1.25f;
  const float aaStepY = (2.0f / static_cast<float>(destH)) * 1.25f;
  p.aaBand = (aaStepX + aaStepY) * 0.5f;

  const float eyeOpenness = s_face.spOpenness.val * (1.0f - s_face.blinkProgress);
  const float breath = s_face.breathAmp * sinf(s_face.breathPhase);
  p.eyeW = s_face.spEyeW.val * s_face.spSquashX.val;
  p.eyeH = s_face.spEyeH.val * clampf(eyeOpenness, 0.005f, 1.2f);
  const float eyeOffX = s_face.spEyeOffX.val;
  const float eyeOffY = s_face.spEyeOffY.val + s_face.spBounceY.val + breath;
  p.mouthW = s_face.spMouthW.val * s_face.spSquashX.val;
  p.mouthCrv = s_face.spMouthCrv.val;
  p.mouthOpen = s_face.spMouthOpen.val;
  p.mouthOffY = s_face.spMouthOffY.val + s_face.spBounceY.val * 0.8f + breath;
  p.blushIntensity = clampf(s_face.spBlush.val, 0.0f, 1.0f);
  p.gazeX = s_face.spGazeX.val;
  p.gazeY = s_face.spGazeY.val;
  p.leftEyeCx = -eyeOffX + p.gazeX * 0.06f;
  p.rightEyeCx = eyeOffX + p.gazeX * 0.06f;
  p.eyeCy = eyeOffY + p.gazeY * 0.06f;
  p.leftCheekCx = -eyeOffX * 1.52f;
  p.rightCheekCx = eyeOffX * 1.52f;
  p.cheekCy = eyeOffY - p.eyeH * 0.82f;
  p.cheekR = 0.13f;
  return p;
}

FaceBounds computeFeatureBounds(const FaceFrameParams& p, int destW, int destH) {
  auto nxToPx = [destW](float nx) -> int { return static_cast<int>(((nx + 1.0f) * 0.5f) * static_cast<float>(destW)); };
  auto nyToPy = [destH](float ny) -> int { return static_cast<int>(((1.0f - ny) * 0.5f) * static_cast<float>(destH)); };
  FaceBounds b{};
  b.bLeftEyeX0 = maxInt(0, nxToPx(p.leftEyeCx - p.eyeW * 1.6f - p.aaBand));
  b.bLeftEyeX1 = minInt(destW - 1, nxToPx(p.leftEyeCx + p.eyeW * 1.6f + p.aaBand));
  b.bRightEyeX0 = maxInt(0, nxToPx(p.rightEyeCx - p.eyeW * 1.6f - p.aaBand));
  b.bRightEyeX1 = minInt(destW - 1, nxToPx(p.rightEyeCx + p.eyeW * 1.6f + p.aaBand));
  b.bEyeY0 = maxInt(0, nyToPy(p.eyeCy + p.eyeH * 1.6f + p.aaBand));
  b.bEyeY1 = minInt(destH - 1, nyToPy(p.eyeCy - p.eyeH * 1.6f - p.aaBand));

  b.bCheekL_X0 = maxInt(0, nxToPx(p.leftCheekCx - p.cheekR * 1.4f));
  b.bCheekL_X1 = minInt(destW - 1, nxToPx(p.leftCheekCx + p.cheekR * 1.4f));
  b.bCheekR_X0 = maxInt(0, nxToPx(p.rightCheekCx - p.cheekR * 1.4f));
  b.bCheekR_X1 = minInt(destW - 1, nxToPx(p.rightCheekCx + p.cheekR * 1.4f));
  b.bCheekY0 = maxInt(0, nyToPy(p.cheekCy + p.cheekR * 1.4f));
  b.bCheekY1 = minInt(destH - 1, nyToPy(p.cheekCy - p.cheekR * 1.4f));

  b.bMouthX0 = maxInt(0, nxToPx(-p.mouthW * 1.3f - p.aaBand));
  b.bMouthX1 = minInt(destW - 1, nxToPx(p.mouthW * 1.3f + p.aaBand));
  b.bMouthY0 = maxInt(0, nyToPy(p.mouthOffY + fabsf(p.mouthCrv) * p.mouthW * p.mouthW + p.mouthOpen * 1.8f + p.aaBand));
  b.bMouthY1 = minInt(destH - 1, nyToPy(p.mouthOffY - p.mouthOpen * 1.8f - p.aaBand));

  b.bBubbleX0 = maxInt(0, nxToPx(-0.43f - p.aaBand));
  b.bBubbleX1 = minInt(destW - 1, nxToPx(0.43f + p.aaBand));
  b.bBubbleY0 = maxInt(0, nyToPy(0.708f + 0.095f + p.aaBand));
  b.bBubbleY1 = minInt(destH - 1, nyToPy(0.50f - p.aaBand));

  b.minBoundY = minInt(minInt(b.bEyeY0, b.bCheekY0), b.bMouthY0);
  b.maxBoundY = maxInt(maxInt(b.bEyeY1, b.bCheekY1), b.bMouthY1);
  if (s_face.speechActive) {
    b.minBoundY = minInt(b.minBoundY, b.bBubbleY0);
    b.maxBoundY = maxInt(b.maxBoundY, b.bBubbleY1);
  }
  return b;
}

void shadeSpeechBubble(float nx, float ny, float aaBand, float& cr, float& cg, float& cb) {
  const float dBox = sdfRoundRect(nx, ny - 0.708f, 0.406f, 0.0875f, 0.033f);
  float dTail = 10.0f;
  if (ny >= 0.52f && ny <= 0.625f) {
    dTail = fabsf(nx - 0.03f) - (ny - 0.52f) * 0.5f;
  }
  const float dBubble = fminf(dBox, dTail);
  const float bubbleAlpha = coverage(dBubble, aaBand);
  if (bubbleAlpha <= 0.005f) return;

  const float borderAlpha = coverage(fabsf(dBubble) - 0.010f, aaBand);
  if (borderAlpha > 0.35f) {
    cr = lerpf(cr, 16.0f / 255.0f, bubbleAlpha);
    cg = lerpf(cg, 30.0f / 255.0f, bubbleAlpha);
    cb = lerpf(cb, 43.0f / 255.0f, bubbleAlpha);
  } else {
    cr = lerpf(cr, 1.0f, bubbleAlpha);
    cg = lerpf(cg, 1.0f, bubbleAlpha);
    cb = lerpf(cb, 1.0f, bubbleAlpha);
  }
}

void renderParticlesOverlay(uint16_t* dst, int stride, int destX, int destY, int destW, int destH, float aaBand) {
  for (int i = 0; i < MAX_PARTICLES; ++i) {
    if (s_face.particles[i].type == PARTICLE_NONE) continue;
    const float pNx = s_face.particles[i].x;
    const float pNy = s_face.particles[i].y;
    const int pcx = static_cast<int>(((pNx + 1.0f) * 0.5f) * static_cast<float>(destW));
    const int pcy = static_cast<int>(((1.0f - pNy) * 0.5f) * static_cast<float>(destH));
    int pRadPx = static_cast<int>(s_face.particles[i].scale * static_cast<float>(destW) * 0.5f);
    if (pRadPx < 3) pRadPx = 3;

    const int pX0 = maxInt(0, pcx - pRadPx - 2);
    const int pX1 = minInt(destW - 1, pcx + pRadPx + 2);
    const int pY0 = maxInt(0, pcy - pRadPx - 2);
    const int pY1 = minInt(destH - 1, pcy + pRadPx + 2);
    const float alpha = clampf(s_face.particles[i].life / s_face.particles[i].maxLife, 0.0f, 1.0f);

    for (int y = pY0; y <= pY1; ++y) {
      const float ny = 1.0f - 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(destH);
      uint16_t* row = &dst[(destY + y) * stride + destX];
      for (int x = pX0; x <= pX1; ++x) {
        const float nx = -1.0f + 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(destW);
        float dPart = 10.0f;
        if (s_face.particles[i].type == PARTICLE_SPARKLE) {
          dPart = sdfDiamondStar(nx - pNx, ny - pNy, s_face.particles[i].scale);
        } else {
          const float dx = (nx - pNx) / (s_face.particles[i].scale + 1e-6f);
          const float dy = (ny - pNy) / (s_face.particles[i].scale + 1e-6f);
          dPart = sqrtf(dx * dx + dy * dy) - 0.7f;
        }
        const float pAlpha = coverage(dPart, aaBand) * alpha;
        if (pAlpha <= 0.01f) continue;

        const uint16_t origSwapped = row[x];
        const uint16_t origBgr = static_cast<uint16_t>((origSwapped >> 8) | (origSwapped << 8));
        float r = static_cast<float>(origBgr & 0x1F) / 31.0f;
        float g = static_cast<float>((origBgr >> 5) & 0x3F) / 63.0f;
        float b = static_cast<float>((origBgr >> 11) & 0x1F) / 31.0f;
        const bool sparkle = (s_face.particles[i].type == PARTICLE_SPARKLE);
        r = lerpf(r, sparkle ? 1.0f : 0.85f, pAlpha);
        g = lerpf(g, sparkle ? 0.88f : 0.96f, pAlpha);
        b = lerpf(b, sparkle ? 0.40f : 0.93f, pAlpha);
        row[x] = packBGR565(floatToByte(r), floatToByte(g), floatToByte(b));
      }
    }
  }
}

inline bool inBox(int px, int py, int x0, int x1, int y0, int y1) {
  return (px >= x0 && px <= x1 && py >= y0 && py <= y1);
}

inline void shadeCheekLayer(bool inLeftCheek, bool inRightCheek, float nx, float ny, const FaceFrameParams& p,
                            float& cr, float& cg, float& cb) {
  if (!inLeftCheek && !inRightCheek) return;
  const float chX = inLeftCheek ? (nx - p.leftCheekCx) : (nx - p.rightCheekCx);
  const float chY = ny - p.cheekCy;
  const float blushAlpha = p.blushIntensity * coverage(sqrtf(chX * chX + chY * chY), p.cheekR) * 0.75f;
  if (blushAlpha <= 0.005f) return;
  cr = lerpf(cr, 1.0f, blushAlpha);
  cg = lerpf(cg, 139.0f / 255.0f, blushAlpha);
  cb = lerpf(cb, 167.0f / 255.0f, blushAlpha);
}

inline void shadeEyeLayer(bool inLeftEye, bool inRightEye, float nx, float ny, const FaceFrameParams& p, float& cr,
                          float& cg, float& cb) {
  if (!inLeftEye && !inRightEye) return;
  BmoFaceSdf::EyeStyle curStyle = s_face.activeEyeStyle;
  if (curStyle == BmoFaceSdf::EYE_WINK_LEFT) {
    curStyle = inLeftEye ? BmoFaceSdf::EYE_CRESCENT_UP : BmoFaceSdf::EYE_CAPSULE;
  }
  const float eyeDx = inLeftEye ? (nx - p.leftEyeCx) : (nx - p.rightEyeCx);
  BmoFaceSdf::shadeEyePixel(eyeDx, ny - p.eyeCy, p.eyeW, p.eyeH, p.gazeX, p.gazeY, curStyle, p.aaBand, cr, cg, cb);
}

void shadeFacePixel(int px, int py, float nx, float ny, const FaceFrameParams& p, const FaceBounds& b,
                    uint16_t& outPixel) {
  const bool inLeftEye = inBox(px, py, b.bLeftEyeX0, b.bLeftEyeX1, b.bEyeY0, b.bEyeY1);
  const bool inRightEye = inBox(px, py, b.bRightEyeX0, b.bRightEyeX1, b.bEyeY0, b.bEyeY1);
  const bool inLeftCheek =
      (p.blushIntensity > 0.05f) && inBox(px, py, b.bCheekL_X0, b.bCheekL_X1, b.bCheekY0, b.bCheekY1);
  const bool inRightCheek =
      (p.blushIntensity > 0.05f) && inBox(px, py, b.bCheekR_X0, b.bCheekR_X1, b.bCheekY0, b.bCheekY1);
  const bool inMouth = inBox(px, py, b.bMouthX0, b.bMouthX1, b.bMouthY0, b.bMouthY1);
  const bool inBubble = s_face.speechActive && inBox(px, py, b.bBubbleX0, b.bBubbleX1, b.bBubbleY0, b.bBubbleY1);
  if (!inLeftEye && !inRightEye && !inLeftCheek && !inRightCheek && !inMouth && !inBubble) return;

  float cr = p.baseBgR;
  float cg = p.baseBgG;
  float cb = p.baseBgB;
  if (inBubble) shadeSpeechBubble(nx, ny, p.aaBand, cr, cg, cb);
  shadeCheekLayer(inLeftCheek, inRightCheek, nx, ny, p, cr, cg, cb);
  shadeEyeLayer(inLeftEye, inRightEye, nx, ny, p, cr, cg, cb);
  if (inMouth) {
    BmoFaceSdf::shadeMouthPixel(nx, ny - p.mouthOffY, p.mouthW, p.mouthCrv, p.mouthOpen, s_face.activeMouthStyle,
                                p.aaBand, cr, cg, cb);
  }
  outPixel = packBGR565(floatToByte(cr), floatToByte(cg), floatToByte(cb));
}

void renderMascotInternal(uint16_t* dst, int stride, int destX, int destY, int destW, int destH) {
  if (!dst || destW <= 0 || destH <= 0) return;
  const uint16_t bgColor = (s_face.expr == BmoFace::SLEEPING) ? COL_DUSK_BG : COL_MINT_BG;
  fillFaceBackground(dst, stride, destX, destY, destW, destH, bgColor);

  const FaceFrameParams params = snapshotParams(destW, destH);
  const FaceBounds bounds = computeFeatureBounds(params, destW, destH);
  for (int py = bounds.minBoundY; py <= bounds.maxBoundY; ++py) {
    const float ny = 1.0f - 2.0f * (static_cast<float>(py) + 0.5f) / static_cast<float>(destH);
    uint16_t* row = &dst[(destY + py) * stride + destX];
    for (int px = 0; px < destW; ++px) {
      const float nx = -1.0f + 2.0f * (static_cast<float>(px) + 0.5f) / static_cast<float>(destW);
      shadeFacePixel(px, py, nx, ny, params, bounds, row[px]);
    }
  }
  renderParticlesOverlay(dst, stride, destX, destY, destW, destH, params.aaBand);
}

} // namespace BmoFaceRenderDetail

namespace BmoFace {

void renderFullScreen(uint16_t* dst, int width, int height) {
  BmoFaceRenderDetail::renderMascotInternal(dst, width, 0, 0, width, height);
  BmoFaceInternal::s_face.dirty = false;
}

void renderToBuffer(uint16_t* dst, int stride, int x, int y, int w, int h) {
  BmoFaceRenderDetail::renderMascotInternal(dst, stride, x, y, w, h);
  BmoFaceInternal::s_face.dirty = false;
}

void draw(int x, int y, int size) {
  auto& f = BmoFaceInternal::s_face;
  if (f.expr == HIDDEN) return;
  if (!f.psramFaceBuf) begin();
  if (!f.psramFaceBuf) return;

  if (size == FACE_FB_W && x == 0 && y == 0) {
    draw();
    return;
  }
  BmoFaceRenderDetail::renderMascotInternal(f.psramFaceBuf, size, 0, 0, size, size);
  DisplayEmu::pushPixelsAt(x, y, size, size, f.psramFaceBuf);
  f.dirty = false;
}

void draw() {
  auto& f = BmoFaceInternal::s_face;
  if (f.expr == HIDDEN) return;
  if (!f.psramFaceBuf) begin();
  if (!f.psramFaceBuf) return;

  BmoFaceRenderDetail::renderMascotInternal(f.psramFaceBuf, FACE_FB_W, 0, 0, FACE_FB_W, FACE_FB_H);
  DisplayEmu::pushPixelsAt(0, 0, FACE_FB_W, FACE_FB_H, f.psramFaceBuf);
  f.dirty = false;
}

uint16_t getScreenBgColor() {
  return (BmoFaceInternal::s_face.expr == SLEEPING) ? BmoFaceInternal::COL_DUSK_BG : BmoFaceInternal::COL_MINT_BG;
}

} // namespace BmoFace
