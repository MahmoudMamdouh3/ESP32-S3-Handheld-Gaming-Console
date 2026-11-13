#pragma once

#include <stdint.h>
#include <stddef.h>

namespace BmoInput {

static constexpr int BUTTON_COUNT = 12;
static constexpr uint32_t DEFAULT_DEBOUNCE_MS = 20U;
static constexpr uint32_t DEFAULT_REPEAT_INITIAL_MS = 350U;
static constexpr uint32_t DEFAULT_REPEAT_INTERVAL_MS = 100U;

struct InputFrame {
  uint32_t timestampMs = 0;
  uint16_t rawMask = 0;
  uint16_t pressedMask = 0;
  uint16_t justPressedMask = 0;
  uint16_t justReleasedMask = 0;
  uint16_t repeatMask = 0;

  constexpr bool isDown(int idx) const {
    return (idx >= 0 && idx < BUTTON_COUNT) && ((pressedMask & (1U << idx)) != 0);
  }
  constexpr bool edgePressed(int idx) const {
    return (idx >= 0 && idx < BUTTON_COUNT) && ((justPressedMask & (1U << idx)) != 0);
  }
  constexpr bool edgeReleased(int idx) const {
    return (idx >= 0 && idx < BUTTON_COUNT) && ((justReleasedMask & (1U << idx)) != 0);
  }
  constexpr bool triggeredOrRepeat(int idx) const {
    return (idx >= 0 && idx < BUTTON_COUNT) && (((justPressedMask | repeatMask) & (1U << idx)) != 0);
  }
};

/**
 * Per-button millisecond debouncer + menu key-repeat controller.
 * Keeps emulator gameplay polling low-latency while rejecting switch bounce
 * chatter in UI navigation and providing ergonomic held-D-pad repeat.
 */
class Debouncer {
public:
  explicit Debouncer(uint32_t debounceMs = DEFAULT_DEBOUNCE_MS, uint32_t repeatInitialMs = DEFAULT_REPEAT_INITIAL_MS,
                     uint32_t repeatIntervalMs = DEFAULT_REPEAT_INTERVAL_MS)
      : debounceMs_(debounceMs), repeatInitialMs_(repeatInitialMs), repeatIntervalMs_(repeatIntervalMs) {
    reset(0);
  }

  void reset(uint16_t initialMask = 0, uint32_t nowMs = 0) {
    lastStableMask_ = initialMask;
    lastRawMask_ = initialMask;
    for (int i = 0; i < BUTTON_COUNT; ++i) {
      rawChangedAtMs_[i] = nowMs;
      pressedSinceMs_[i] = nowMs;
      lastRepeatAtMs_[i] = nowMs;
      repeatArmed_[i] = false;
    }
    suppressedUntilMs_ = 0;
  }

  void suppressUntil(uint32_t untilMs) {
    suppressedUntilMs_ = untilMs;
    for (int i = 0; i < BUTTON_COUNT; ++i) {
      repeatArmed_[i] = false;
    }
  }

  InputFrame update(uint16_t rawMask, uint32_t nowMs, bool enableRepeat) {
    InputFrame frame{};
    frame.timestampMs = nowMs;
    frame.rawMask = rawMask;

    uint16_t newStable = lastStableMask_;
    uint16_t justPressed = 0;
    uint16_t justReleased = 0;
    uint16_t repeats = 0;

    for (int i = 0; i < BUTTON_COUNT; ++i) {
      const uint16_t bit = static_cast<uint16_t>(1U << i);
      const bool rawNow = (rawMask & bit) != 0;
      const bool rawPrev = (lastRawMask_ & bit) != 0;
      if (rawNow != rawPrev) {
        rawChangedAtMs_[i] = nowMs;
      }

      const bool stableNow = (newStable & bit) != 0;
      const uint32_t rawStableElapsed = static_cast<uint32_t>(nowMs - rawChangedAtMs_[i]);
      if (rawNow != stableNow && (debounceMs_ == 0 || rawStableElapsed >= debounceMs_)) {
        if (rawNow) {
          newStable = static_cast<uint16_t>(newStable | bit);
          justPressed = static_cast<uint16_t>(justPressed | bit);
          pressedSinceMs_[i] = nowMs;
          lastRepeatAtMs_[i] = nowMs;
          repeatArmed_[i] = false;
        } else {
          newStable = static_cast<uint16_t>(newStable & ~bit);
          justReleased = static_cast<uint16_t>(justReleased | bit);
          repeatArmed_[i] = false;
        }
      } else if (enableRepeat && stableNow && static_cast<int32_t>(nowMs - suppressedUntilMs_) >= 0) {
        const uint32_t heldMs = static_cast<uint32_t>(nowMs - pressedSinceMs_[i]);
        if (!repeatArmed_[i]) {
          if (heldMs >= repeatInitialMs_) {
            repeatArmed_[i] = true;
            lastRepeatAtMs_[i] = nowMs;
            repeats = static_cast<uint16_t>(repeats | bit);
          }
        } else if (repeatIntervalMs_ > 0 && static_cast<uint32_t>(nowMs - lastRepeatAtMs_[i]) >= repeatIntervalMs_) {
          lastRepeatAtMs_[i] = nowMs;
          repeats = static_cast<uint16_t>(repeats | bit);
        }
      }
    }

    lastRawMask_ = rawMask;
    lastStableMask_ = newStable;

    if (static_cast<int32_t>(nowMs - suppressedUntilMs_) < 0) {
      justPressed = 0;
      repeats = 0;
    }

    frame.pressedMask = newStable;
    frame.justPressedMask = justPressed;
    frame.justReleasedMask = justReleased;
    frame.repeatMask = repeats;
    return frame;
  }

private:
  uint32_t debounceMs_;
  uint32_t repeatInitialMs_;
  uint32_t repeatIntervalMs_;
  uint16_t lastStableMask_ = 0;
  uint16_t lastRawMask_ = 0;
  uint32_t rawChangedAtMs_[BUTTON_COUNT] = {};
  uint32_t pressedSinceMs_[BUTTON_COUNT] = {};
  uint32_t lastRepeatAtMs_[BUTTON_COUNT] = {};
  bool repeatArmed_[BUTTON_COUNT] = {};
  uint32_t suppressedUntilMs_ = 0;
};

} // namespace BmoInput
