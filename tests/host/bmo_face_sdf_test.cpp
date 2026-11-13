#include <math.h>
#include <stdint.h>
#include <vector>

#include "src/ui/bmo_face_sdf.h"
#include "test_check.h"

static uint32_t fnv1a16(const uint16_t* data, size_t count) {
  uint32_t h = 2166136261U;
  for (size_t i = 0; i < count; ++i) {
    h ^= static_cast<uint8_t>(data[i] & 0xFFU);
    h *= 16777619U;
    h ^= static_cast<uint8_t>((data[i] >> 8) & 0xFFU);
    h *= 16777619U;
  }
  return h;
}

int main() {
  // Coverage contract: dist <= 0 => 1.0, dist >= aaBand => 0.0, monotone in between.
  BMO_CHECK(fabsf(BmoFaceSdf::coverage(-0.05f, 0.02f) - 1.0f) < 1e-6f);
  BMO_CHECK(fabsf(BmoFaceSdf::coverage(0.0f, 0.02f) - 1.0f) < 1e-6f);
  BMO_CHECK(fabsf(BmoFaceSdf::coverage(0.02f, 0.02f) - 0.0f) < 1e-6f);
  BMO_CHECK(fabsf(BmoFaceSdf::coverage(0.05f, 0.02f) - 0.0f) < 1e-6f);
  const float mid = BmoFaceSdf::coverage(0.01f, 0.02f);
  BMO_CHECK(fabsf(mid - 0.5f) < 1e-4f);
  BMO_CHECK(BmoFaceSdf::coverage(0.005f, 0.02f) > mid);
  BMO_CHECK(BmoFaceSdf::coverage(0.015f, 0.02f) < mid);
  BMO_CHECK(BmoFaceSdf::coverage(-0.01f, 0.0f) == 1.0f);
  BMO_CHECK(BmoFaceSdf::coverage(0.01f, 0.0f) == 0.0f);

  // Verify SDF polarity at origin for capsule eyes & rounded rect.
  BMO_CHECK(BmoFaceSdf::sdfCapsuleV(0.0f, 0.0f, 0.08f, 0.08f) < 0.0f);
  BMO_CHECK(BmoFaceSdf::sdfCapsuleV(0.5f, 0.0f, 0.08f, 0.08f) > 0.0f);

  // Render all 18 expression snapshots deterministically at 64x48 and verify
  // visible non-background geometry differs between IDLE, ERROR, SLEEPING, LOVE.
  constexpr int W = 64;
  constexpr int H = 48;
  std::vector<uint16_t> fb(W * H);
  uint32_t hashes[18] = {};
  for (int i = 0; i < 18; ++i) {
    const bool sleepBg = (i == 10);
    BmoFaceSdf::renderTargetSnapshot(BmoFaceSdf::EXPR_TABLE[i], sleepBg, fb.data(), W, H);
    hashes[i] = fnv1a16(fb.data(), fb.size());
    BMO_CHECK(hashes[i] != 0U);
  }
  BMO_CHECK(hashes[0] != hashes[6]);   // IDLE vs ERROR
  BMO_CHECK(hashes[0] != hashes[10]);  // IDLE vs SLEEPING
  BMO_CHECK(hashes[0] != hashes[15]);  // IDLE vs LOVE
  BMO_CHECK(hashes[1] != hashes[2]);   // SURPRISED vs HAPPY
  return 0;
}
