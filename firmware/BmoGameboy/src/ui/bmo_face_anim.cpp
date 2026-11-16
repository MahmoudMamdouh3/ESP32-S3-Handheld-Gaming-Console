// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "bmo_face.h"
#include "bmo_face_internal.h"
#include "bmo_quotes.h"
#include "../hal/config.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

namespace BmoFaceInternal {
BmoFaceRuntime s_face{};
}

namespace BmoFaceAnimDetail {

using namespace BmoFaceInternal;
using BmoFaceSdf::clampf;
using BmoFaceSdf::EXPR_TABLE;

void spawnParticle(ParticleType type, float x, float y, float vx, float vy, float scale, float maxLife) {
  for (int i = 0; i < MAX_PARTICLES; ++i) {
    if (s_face.particles[i].type != PARTICLE_NONE) continue;
    s_face.particles[i].type = type;
    s_face.particles[i].x = x;
    s_face.particles[i].y = y;
    s_face.particles[i].vx = vx;
    s_face.particles[i].vy = vy;
    s_face.particles[i].scale = scale;
    s_face.particles[i].life = maxLife;
    s_face.particles[i].maxLife = maxLife;
    s_face.particles[i].seed = static_cast<float>(random(0, 1000)) * 0.001f;
    return;
  }
}

void updateParticles(float dt) {
  for (int i = 0; i < MAX_PARTICLES; ++i) {
    if (s_face.particles[i].type == PARTICLE_NONE) continue;
    s_face.particles[i].life -= dt;
    if (s_face.particles[i].life <= 0.0f) {
      s_face.particles[i].type = PARTICLE_NONE;
      continue;
    }
    s_face.particles[i].x += s_face.particles[i].vx * dt;
    s_face.particles[i].y += s_face.particles[i].vy * dt;
    if (s_face.particles[i].type == PARTICLE_DREAM_Z) {
      s_face.particles[i].x += sinf(s_face.particles[i].life * 4.0f + s_face.particles[i].seed * 6.28f) * 0.05f * dt;
    }
    s_face.dirty = true;
  }
}

void updateSaccades(unsigned long nowMs) {
  if (s_face.userGazeActive) {
    if (nowMs > s_face.userGazeEndMs) {
      s_face.userGazeActive = false;
      s_face.spGazeX.setTarget(0.0f);
      s_face.spGazeY.setTarget(0.0f);
    }
    return;
  }
  if (s_face.expr == BmoFace::IDLE || s_face.expr == BmoFace::HAPPY || s_face.expr == BmoFace::CHARGING) {
    if ((nowMs - s_face.lastSaccadeMs) >= s_face.nextSaccadeMs) {
      s_face.spGazeX.setTarget(static_cast<float>(random(-35, 36)) * 0.01f);
      s_face.spGazeY.setTarget(static_cast<float>(random(-20, 21)) * 0.01f);
      s_face.lastSaccadeMs = nowMs;
      s_face.nextSaccadeMs = static_cast<unsigned long>(random(2200, 4800));
    }
  }
}

void updateBlink(unsigned long nowMs) {
  if (!s_face.blinking && (s_face.expr == BmoFace::IDLE || s_face.expr == BmoFace::HAPPY ||
                           s_face.expr == BmoFace::CHARGING || s_face.expr == BmoFace::JOY)) {
    if ((nowMs - s_face.lastBlinkMs) >= s_face.nextBlinkMs) {
      s_face.blinking = true;
      s_face.blinkStartMs = nowMs;
      s_face.blinkPhase = (random(0, 5) == 0) ? 1 : 0;
    }
  }
  if (!s_face.blinking) return;

  const unsigned long elapsed = nowMs - s_face.blinkStartMs;
  constexpr unsigned long BLINK_CLOSE_MS = 45;
  constexpr unsigned long BLINK_HOLD_MS = 20;
  constexpr unsigned long BLINK_OPEN_MS = 75;
  if (elapsed < BLINK_CLOSE_MS) {
    s_face.blinkProgress = static_cast<float>(elapsed) / static_cast<float>(BLINK_CLOSE_MS);
  } else if (elapsed < BLINK_CLOSE_MS + BLINK_HOLD_MS) {
    s_face.blinkProgress = 1.0f;
  } else if (elapsed < BLINK_CLOSE_MS + BLINK_HOLD_MS + BLINK_OPEN_MS) {
    const float t = static_cast<float>(elapsed - BLINK_CLOSE_MS - BLINK_HOLD_MS) / static_cast<float>(BLINK_OPEN_MS);
    s_face.blinkProgress = 1.0f - t;
  } else if (s_face.blinkPhase == 1) {
    s_face.blinkPhase = 2;
    s_face.blinkStartMs = nowMs;
    s_face.blinkProgress = 0.0f;
  } else {
    s_face.blinking = false;
    s_face.blinkProgress = 0.0f;
    s_face.lastBlinkMs = nowMs;
    s_face.nextBlinkMs = static_cast<unsigned long>(random(2500, 5500));
  }
  s_face.dirty = true;
}

void updateSpeech(unsigned long nowMs) {
  if (!s_face.speechActive) return;
  if (nowMs > s_face.speechExpireMs) {
    s_face.speechActive = false;
    s_face.speechText[0] = '\0';
    s_face.speechVisibleChars = 0;
    s_face.dirty = true;
    return;
  }
  const int fullLen = static_cast<int>(strlen(s_face.speechText));
  if (s_face.speechVisibleChars < fullLen && (nowMs - s_face.speechLastCharMs >= 25)) {
    s_face.speechVisibleChars++;
    s_face.speechLastCharMs = nowMs;
    s_face.dirty = true;
  }
}

void updateDance(unsigned long nowMs, float dt) {
  if (!s_face.danceActive) return;
  if (s_face.danceEndMs != 0 && nowMs > s_face.danceEndMs) {
    BmoFace::stopDance();
    return;
  }
  const float danceOmega = s_face.danceBpm * 2.0f * 3.14159265f / 60.0f;
  s_face.dancePhase += dt * danceOmega;
  if (s_face.dancePhase > 628.31853f) s_face.dancePhase -= 628.31853f;

  s_face.spBounceY.val = 0.09f * sinf(s_face.dancePhase);
  s_face.spGazeX.val = 0.45f * sinf(s_face.dancePhase * 0.5f);
  s_face.spSquashX.val = 1.0f + 0.12f * cosf(s_face.dancePhase);

  static float s_lastDanceSine = 0.0f;
  const float curDanceSine = sinf(s_face.dancePhase);
  if (curDanceSine > 0.95f && s_lastDanceSine <= 0.95f) {
    const float sparkX = (random(0, 2) == 0) ? -0.45f : 0.45f;
    spawnParticle(PARTICLE_SPARKLE, sparkX, 0.20f, 0.06f, 0.15f, 0.06f, 0.7f);
  }
  s_lastDanceSine = curDanceSine;
  s_face.dirty = true;
}

bool updateSprings(float dt) {
  bool moved = false;
  moved |= s_face.spOpenness.update(dt);
  moved |= s_face.spEyeW.update(dt);
  moved |= s_face.spEyeH.update(dt);
  moved |= s_face.spEyeOffX.update(dt);
  moved |= s_face.spEyeOffY.update(dt);
  moved |= s_face.spMouthW.update(dt);
  moved |= s_face.spMouthCrv.update(dt);
  moved |= s_face.spMouthOpen.update(dt);
  moved |= s_face.spMouthOffY.update(dt);
  moved |= s_face.spBlush.update(dt);
  moved |= s_face.spBounceY.update(dt);
  moved |= s_face.spSquashX.update(dt);
  moved |= s_face.spGazeX.update(dt);
  moved |= s_face.spGazeY.update(dt);
  s_face.spSquashX.setTarget(1.0f - s_face.spBounceY.val * 0.45f);
  return moved;
}

} // namespace BmoFaceAnimDetail

namespace BmoFace {

void begin() {
  using namespace BmoFaceInternal;
  using namespace BmoFaceSdf;
  randomSeed(micros());

  if (!s_face.psramFaceBuf) {
    s_face.psramFaceBuf =
        static_cast<uint16_t*>(heap_caps_malloc(FACE_FB_W * FACE_FB_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
    if (!s_face.psramFaceBuf) {
      LOG_ERROR_STR("[BmoFace] FATAL: Failed to allocate mascot canvas in Octal PSRAM!");
    } else {
      LOG_INFO("[BmoFace] Animated Mascot 320x240 canvas allocated in Octal PSRAM (%d bytes)",
               FACE_FB_W * FACE_FB_H * static_cast<int>(sizeof(uint16_t)));
    }
  }

  const ExprTarget& t = EXPR_TABLE[static_cast<int>(IDLE)];
  s_face.spOpenness.init(t.eyeOpenness, 22.0f, 0.72f);
  s_face.spEyeW.init(t.eyeWidth, 22.0f, 0.72f);
  s_face.spEyeH.init(t.eyeHeight, 22.0f, 0.72f);
  s_face.spEyeOffX.init(t.eyeOffsetX, 22.0f, 0.72f);
  s_face.spEyeOffY.init(t.eyeOffsetY, 22.0f, 0.72f);
  s_face.spMouthW.init(t.mouthWidth, 22.0f, 0.72f);
  s_face.spMouthCrv.init(t.mouthCurve, 20.0f, 0.70f);
  s_face.spMouthOpen.init(t.mouthOpenness, 24.0f, 0.68f);
  s_face.spMouthOffY.init(t.mouthOffsetY, 22.0f, 0.72f);
  s_face.spBlush.init(t.blushIntensity, 16.0f, 0.75f);
  s_face.spBounceY.init(0.0f, 24.0f, 0.65f);
  s_face.spSquashX.init(1.0f, 24.0f, 0.65f);
  s_face.spGazeX.init(0.0f, 18.0f, 0.75f);
  s_face.spGazeY.init(0.0f, 18.0f, 0.75f);

  s_face.activeEyeStyle = t.eyeStyle;
  s_face.activeMouthStyle = t.mouthStyle;
  s_face.expr = IDLE;
  s_face.dirty = true;
  s_face.lastBlinkMs = millis();
  s_face.nextBlinkMs = static_cast<unsigned long>(random(2500, 5000));
  s_face.lastSaccadeMs = millis();
  s_face.nextSaccadeMs = static_cast<unsigned long>(random(2000, 4500));
  s_face.lastUpdateUs = micros();

  for (int i = 0; i < MAX_PARTICLES; ++i) {
    s_face.particles[i].type = PARTICLE_NONE;
  }
}

void setExpression(BmoExpression expr) {
  using namespace BmoFaceInternal;
  using namespace BmoFaceSdf;
  if (s_face.expr == expr) return;

  s_face.expr = expr;
  const ExprTarget& t = EXPR_TABLE[static_cast<int>(expr)];
  if (expr == ERROR) {
    s_face.spOpenness.snap(t.eyeOpenness);
    s_face.spEyeW.snap(t.eyeWidth);
    s_face.spEyeH.snap(t.eyeHeight);
    s_face.spEyeOffX.snap(t.eyeOffsetX);
    s_face.spEyeOffY.snap(t.eyeOffsetY);
    s_face.spMouthW.snap(t.mouthWidth);
    s_face.spMouthCrv.snap(t.mouthCurve);
    s_face.spMouthOpen.snap(t.mouthOpenness);
    s_face.spMouthOffY.snap(t.mouthOffsetY);
    s_face.spBlush.snap(t.blushIntensity);
    s_face.spBounceY.snap(0.0f);
    s_face.spSquashX.snap(1.0f);
  } else {
    s_face.spOpenness.setTarget(t.eyeOpenness);
    s_face.spEyeW.setTarget(t.eyeWidth);
    s_face.spEyeH.setTarget(t.eyeHeight);
    s_face.spEyeOffX.setTarget(t.eyeOffsetX);
    s_face.spEyeOffY.setTarget(t.eyeOffsetY);
    s_face.spMouthW.setTarget(t.mouthWidth);
    s_face.spMouthCrv.setTarget(t.mouthCurve);
    s_face.spMouthOpen.setTarget(t.mouthOpenness);
    s_face.spMouthOffY.setTarget(t.mouthOffsetY);
    s_face.spBlush.setTarget(t.blushIntensity);
    if (expr == JOY || expr == CELEBRATING || expr == HAPPY) {
      s_face.spBounceY.vel = 1.2f;
      s_face.spSquashX.val = 0.88f;
    } else if (expr == SURPRISED) {
      s_face.spBounceY.vel = 0.8f;
      s_face.spSquashX.val = 0.92f;
    }
  }
  s_face.activeEyeStyle = t.eyeStyle;
  s_face.activeMouthStyle = t.mouthStyle;
  s_face.blinking = false;
  s_face.blinkProgress = 0.0f;
  s_face.dirty = true;
}

BmoExpression getExpression() {
  return BmoFaceInternal::s_face.expr;
}

void setGaze(float gx, float gy) {
  auto& f = BmoFaceInternal::s_face;
  f.spGazeX.setTarget(BmoFaceSdf::clampf(gx, -1.0f, 1.0f));
  f.spGazeY.setTarget(BmoFaceSdf::clampf(gy, -1.0f, 1.0f));
  f.userGazeActive = true;
  f.userGazeEndMs = millis() + 1400;
  f.dirty = true;
}

void tickle(float intensity) {
  setExpression(TICKLED);
  auto& f = BmoFaceInternal::s_face;
  f.spBounceY.vel = 1.4f * intensity;
  f.spSquashX.val = 0.82f;
  f.spGazeX.vel = ((random(0, 2) == 0) ? -2.5f : 2.5f) * intensity;
  f.dirty = true;
}

void triggerWink() {
  setExpression(WINK);
  BmoFaceAnimDetail::spawnParticle(BmoFaceInternal::PARTICLE_SPARKLE, 0.42f, 0.22f, 0.08f, 0.12f, 0.06f, 0.8f);
  BmoFaceAnimDetail::spawnParticle(BmoFaceInternal::PARTICLE_SPARKLE, 0.48f, 0.14f, 0.12f, 0.04f, 0.05f, 0.9f);
  BmoFaceAnimDetail::spawnParticle(BmoFaceInternal::PARTICLE_SPARKLE, 0.38f, 0.30f, 0.04f, 0.16f, 0.04f, 0.7f);
  BmoFaceInternal::s_face.dirty = true;
}

void triggerBlush() {
  setExpression(BLUSH);
  auto& f = BmoFaceInternal::s_face;
  f.spGazeX.setTarget(-0.55f);
  f.spGazeY.setTarget(-0.40f);
  f.dirty = true;
}

void triggerJoy() {
  setExpression(JOY);
  BmoFaceInternal::s_face.dirty = true;
}

void update() {
  auto& f = BmoFaceInternal::s_face;
  if (f.expr == HIDDEN) return;

  const unsigned long nowUs = micros();
  float dt = static_cast<float>(nowUs - f.lastUpdateUs) * 0.000001f;
  f.lastUpdateUs = nowUs;
  if (dt <= 0.0f || dt > 0.1f) dt = 0.01666f;

  const unsigned long nowMs = millis();
  f.breathPhase += dt * f.breathSpeed;
  if (f.breathPhase > 6.2831853f) f.breathPhase -= 6.2831853f;

  BmoFaceAnimDetail::updateSaccades(nowMs);
  BmoFaceAnimDetail::updateBlink(nowMs);
  if (f.expr == SLEEPING) {
    static unsigned long lastZSpawn = 0;
    if (nowMs - lastZSpawn > 1800) {
      BmoFaceAnimDetail::spawnParticle(BmoFaceInternal::PARTICLE_DREAM_Z, 0.35f, -0.15f, 0.04f, 0.18f, 0.08f, 2.8f);
      lastZSpawn = nowMs;
    }
  }
  BmoFaceAnimDetail::updateSpeech(nowMs);
  BmoFaceAnimDetail::updateDance(nowMs, dt);
  const bool moved = BmoFaceAnimDetail::updateSprings(dt);
  BmoFaceAnimDetail::updateParticles(dt);
  if (moved) f.dirty = true;
}

void say(const char* quote, unsigned long durationMs) {
  if (!quote) return;
  auto& f = BmoFaceInternal::s_face;
  strncpy(f.speechText, quote, sizeof(f.speechText) - 1);
  f.speechText[sizeof(f.speechText) - 1] = '\0';
  f.speechExpireMs = millis() + durationMs;
  f.speechVisibleChars = 1;
  f.speechLastCharMs = millis();
  f.speechActive = true;
  f.dirty = true;
}

const char* getCurrentQuote() {
  return BmoFaceInternal::s_face.speechText;
}

int getVisibleChars() {
  return BmoFaceInternal::s_face.speechVisibleChars;
}

bool hasSpeech() {
  return BmoFaceInternal::s_face.speechActive;
}

void clearSpeech() {
  auto& f = BmoFaceInternal::s_face;
  f.speechActive = false;
  f.speechText[0] = '\0';
  f.speechVisibleChars = 0;
  f.dirty = true;
}

void triggerDance(float bpm) {
  auto& f = BmoFaceInternal::s_face;
  f.danceActive = true;
  f.danceBpm = (bpm > 40.0f && bpm < 240.0f) ? bpm : 125.0f;
  f.dancePhase = 0.0f;
  f.danceEndMs = millis() + 10000;
  setExpression(JOY);
  say("Dance party with BMO! Unce unce! ~", 4000);
  f.dirty = true;
}

void stopDance() {
  auto& f = BmoFaceInternal::s_face;
  f.danceActive = false;
  f.spBounceY.setTarget(0.0f);
  f.spSquashX.setTarget(1.0f);
  f.spGazeX.setTarget(0.0f);
  setExpression(HAPPY);
  f.dirty = true;
}

bool isDancing() {
  return BmoFaceInternal::s_face.danceActive;
}

int getBuddyLevel() {
  return BmoFaceInternal::s_face.buddyLevel;
}

int getHappiness() {
  return BmoFaceInternal::s_face.happiness;
}

void petCompanion() {
  auto& f = BmoFaceInternal::s_face;
  f.totalPets++;
  f.happiness = (f.happiness + 5 < 100) ? (f.happiness + 5) : 100;
  f.buddyLevel = (f.totalPets / 10) + 1;
  tickle(1.2f);
  say("Hehehe! Stop it, that tickles!", 2500);
}

bool isDirty() {
  return BmoFaceInternal::s_face.dirty;
}

} // namespace BmoFace
