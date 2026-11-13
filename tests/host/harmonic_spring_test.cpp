#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "harmonic_spring.h"

int main() {
  for (float dt : {1.0f / 60.0f, 0.05f}) {
    BmoFaceInternal::HarmonicSpring spring;
    spring.init(0.0f, 22.0f, 0.72f);
    spring.setTarget(1.0f);
    float maximum = spring.val;
    const int steps = dt < 0.02f ? 60 : 20;
    for (int i = 0; i < steps; ++i) {
      spring.update(dt);
      if (!std::isfinite(spring.val) || !std::isfinite(spring.vel)) return 1;
      if (std::fabs(spring.val) > 5.0f || std::fabs(spring.vel) > 100.0f) return 2;
      if (spring.val > maximum) maximum = spring.val;
    }
    std::printf("dt=%.8f value=%.8f velocity=%.8f max=%.8f overshoot=%.8f\n", dt, spring.val, spring.vel, maximum,
                maximum - 1.0f);
  }

  BmoFaceInternal::HarmonicSpring spring;
  spring.init(2.0f, 22.0f, 0.72f);
  spring.setTarget(3.0f);
  if (spring.update(0.0f) || spring.update(-0.01f)) return 3;
  if (spring.update(NAN) || spring.update(INFINITY)) return 3;
  if (spring.val != 2.0f || spring.target != 3.0f || spring.vel != 0.0f) return 4;
  spring.update(0.05f);
  const float cappedValue = spring.val;
  const float cappedVelocity = spring.vel;

  BmoFaceInternal::HarmonicSpring overlong;
  overlong.init(2.0f, 22.0f, 0.72f);
  overlong.setTarget(3.0f);
  overlong.update(0.5f);
  if (overlong.val != cappedValue || overlong.vel != cappedVelocity) return 5;

  spring.snap(7.0f);
  if (spring.val != 7.0f || spring.target != 7.0f || spring.vel != 0.0f) return 6;
  if (spring.update(1.0f / 60.0f)) return 7;
  return 0;
}
