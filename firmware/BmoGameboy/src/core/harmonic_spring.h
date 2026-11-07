#pragma once

#include <cmath>

namespace BmoFaceInternal {

struct HarmonicSpring {
  float val;
  float vel;
  float target;
  float omega;
  float zeta;

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
    if (!std::isfinite(dt) || dt <= 0.0f) return false;
    if (dt > 0.05f) dt = 0.05f;
    if (!std::isfinite(val) || !std::isfinite(vel) || !std::isfinite(target) ||
        !std::isfinite(omega) || !std::isfinite(zeta) || omega <= 0.0f || zeta < 0.0f) {
      return false;
    }
    float diff = val - target;
    if (fabsf(diff) < 0.0002f && fabsf(vel) < 0.0005f) {
      if (val != target) {
        val = target;
        vel = 0.0f;
        return true;
      }
      return false;
    }
    const float f = 1.0f + 2.0f * dt * zeta * omega;
    const float omegaSquared = omega * omega;
    const float dtOmegaSquared = dt * omegaSquared;
    const float dtSquaredOmegaSquared = dt * dtOmegaSquared;
    const float inverseDeterminant = 1.0f / (f + dtSquaredOmegaSquared);
    const float nextValue = (f * val + dt * vel + dtSquaredOmegaSquared * target) * inverseDeterminant;
    const float nextVelocity = (vel + dtOmegaSquared * (target - val)) * inverseDeterminant;
    val = nextValue;
    vel = nextVelocity;
    return true;
  }
};

} // namespace BmoFaceInternal
