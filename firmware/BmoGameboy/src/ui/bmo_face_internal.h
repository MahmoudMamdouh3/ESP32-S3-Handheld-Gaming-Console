// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include "bmo_face.h"
#include "bmo_face_sdf.h"
#include "harmonic_spring.h"

namespace BmoFaceInternal {

// Adventure Time BMO Color Palette (byte-swapped BGR565 for ST7789 SPI)
static const uint16_t COL_MINT_BG = BmoFaceSdf::packBGR565(138, 213, 195); // #8AD5C3
static const uint16_t COL_DUSK_BG = BmoFaceSdf::packBGR565(111, 184, 167); // #6FB8A7 (Sleep mode)
static const uint16_t COL_CHARCOAL = BmoFaceSdf::packBGR565(16, 30, 43);   // #101E2B (Eyes, mouth line)
static const uint16_t COL_CAVITY = BmoFaceSdf::packBGR565(18, 28, 38);     // #121C26 (Mouth cavity)
static const uint16_t COL_BLUSH = BmoFaceSdf::packBGR565(255, 139, 167);   // #FF8BA7 (Cheeks)
static const uint16_t COL_TONGUE = BmoFaceSdf::packBGR565(250, 127, 143);  // #FA7F8F (Tongue)
static const uint16_t COL_GLINT = BmoFaceSdf::packBGR565(255, 255, 255);   // #FFFFFF (Catchlight)
static const uint16_t COL_STAR = BmoFaceSdf::packBGR565(255, 224, 102);    // #FFE066 (Celebration star)
static const uint16_t COL_DREAM_Z = BmoFaceSdf::packBGR565(210, 245, 238); // #D2F5EE (Sleep bubble)
static const uint16_t COL_ERROR_RED = BmoFaceSdf::packBGR565(225, 36, 48); // #E12430 (Error cross)

enum ParticleType { PARTICLE_NONE = 0, PARTICLE_DREAM_Z, PARTICLE_SPARKLE };

struct MascotParticle {
  ParticleType type;
  float x;
  float y;
  float vx;
  float vy;
  float scale;
  float life;
  float maxLife;
  float seed;
};

static const int MAX_PARTICLES = 8;

struct BmoFaceRuntime {
  MascotParticle particles[MAX_PARTICLES];
  BmoFace::BmoExpression expr = BmoFace::HIDDEN;
  HarmonicSpring spOpenness;
  HarmonicSpring spEyeW;
  HarmonicSpring spEyeH;
  HarmonicSpring spEyeOffX;
  HarmonicSpring spEyeOffY;
  HarmonicSpring spMouthW;
  HarmonicSpring spMouthCrv;
  HarmonicSpring spMouthOpen;
  HarmonicSpring spMouthOffY;
  HarmonicSpring spBlush;
  HarmonicSpring spBounceY;
  HarmonicSpring spSquashX;
  HarmonicSpring spGazeX;
  HarmonicSpring spGazeY;

  BmoFaceSdf::EyeStyle activeEyeStyle = BmoFaceSdf::EYE_CAPSULE;
  BmoFaceSdf::MouthStyle activeMouthStyle = BmoFaceSdf::MOUTH_ARC;
  bool dirty = false;

  float breathPhase = 0.0f;
  float breathAmp = 0.025f;
  float breathSpeed = 1.95f;

  unsigned long lastSaccadeMs = 0;
  unsigned long nextSaccadeMs = 2800;
  bool userGazeActive = false;
  unsigned long userGazeEndMs = 0;

  unsigned long lastBlinkMs = 0;
  unsigned long nextBlinkMs = 3200;
  float blinkProgress = 0.0f;
  bool blinking = false;
  unsigned long blinkStartMs = 0;
  int blinkPhase = 0;

  uint16_t* psramFaceBuf = nullptr;
  unsigned long lastUpdateUs = 0;

  char speechText[64] = "";
  unsigned long speechExpireMs = 0;
  int speechVisibleChars = 0;
  unsigned long speechLastCharMs = 0;
  bool speechActive = false;

  bool danceActive = false;
  float danceBpm = 125.0f;
  float dancePhase = 0.0f;
  unsigned long danceEndMs = 0;

  int buddyLevel = 1;
  int happiness = 95;
  int totalPets = 0;
};

extern BmoFaceRuntime s_face;

} // namespace BmoFaceInternal
