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
  static constexpr uint32_t kExpectedHashes[18] = {
      0x9B93E427U, // 0  IDLE
      0xE98FF89BU, // 1  SURPRISED
      0xC9C91BFDU, // 2  HAPPY
      0x2743385EU, // 3  SLEEPY
      0x33419B66U, // 4  LOW_BATTERY
      0x7AA4ED49U, // 5  CHARGING
      0xD43261D1U, // 6  ERROR
      0x3DE20099U, // 7  SHUTDOWN
      0x3D45A5C5U, // 8  HIDDEN
      0x3C780A19U, // 9  JOY
      0xD30A96C5U, // 10 SLEEPING
      0xC11903D2U, // 11 WINK
      0x43FFDCD9U, // 12 BLUSH
      0x1B0070C9U, // 13 CONFUSED
      0x253968D0U, // 14 ANNOYED
      0x45CCCBB5U, // 15 LOVE
      0xEB3102C1U, // 16 TICKLED
      0x9B18CD05U, // 17 CELEBRATING
  };
  constexpr int W = 64;
  constexpr int H = 48;
  std::vector<uint16_t> fb(W * H);
  uint32_t hashes[18] = {};
  for (int i = 0; i < 18; ++i) {
    const bool sleepBg = (i == 10);
    BmoFaceSdf::renderTargetSnapshot(BmoFaceSdf::EXPR_TABLE[i], sleepBg, fb.data(), W, H);
    hashes[i] = fnv1a16(fb.data(), fb.size());
    BMO_CHECK(hashes[i] != 0U);
    if (i != 6) {
      // Expressions 0..5 & 7..17 use purely algebraic SDF operations (bit-exact IEEE-754 across libm implementations).
      BMO_CHECK(hashes[i] == kExpectedHashes[i]);
    } else {
      // ERROR expression uses sinf() for the zigzag mouth, so check structural cross/mouth presence cleanly.
      int errorRedPixels = 0;
      const uint16_t bgPixel = fb[0];
      int nonBgPixels = 0;
      for (uint16_t px : fb) {
        if (px != bgPixel) nonBgPixels++;
        const uint16_t bgr = static_cast<uint16_t>((px << 8) | (px >> 8));
        const uint8_t r = static_cast<uint8_t>((bgr & 0x1FU) << 3);
        const uint8_t b = static_cast<uint8_t>(((bgr >> 11) & 0x1FU) << 3);
        if (r > 180U && b < 80U) errorRedPixels++;
      }
      BMO_CHECK(errorRedPixels >= 16);
      BMO_CHECK(nonBgPixels >= 80);
    }
  }
  BMO_CHECK(hashes[0] != hashes[6]);  // IDLE vs ERROR
  BMO_CHECK(hashes[0] != hashes[10]); // IDLE vs SLEEPING
  BMO_CHECK(hashes[0] != hashes[15]); // IDLE vs LOVE
  BMO_CHECK(hashes[1] != hashes[2]);  // SURPRISED vs HAPPY
  return 0;
}
