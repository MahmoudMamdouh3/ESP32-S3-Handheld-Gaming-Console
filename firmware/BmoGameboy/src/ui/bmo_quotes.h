// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace BmoQuotes {

inline constexpr const char* kQuotes[] = {
    "Who wants to play video games?!",
    "Yay! BMO is so happy to see you!",
    "Sometimes life is scary, but we have games!",
    "Yes, Finn. It goes in my butt.",
    "BMO chop! If this were a real attack, you'd be dead!",
    "When bad things happen, I know you want to believe they're a joke.",
    "I am a little living boy!",
    "BMO is camera ready! *wink*",
    "Oh, you are making BMO blush!",
    "Whew! What a fun dance party!",
};

inline constexpr uint32_t kQuoteCount = sizeof(kQuotes) / sizeof(kQuotes[0]);
inline constexpr uint32_t kIdleQuoteCount = 7U;
inline constexpr uint32_t kWinkQuoteIndex = 7U;
inline constexpr uint32_t kBlushQuoteIndex = 8U;
inline constexpr uint32_t kDanceEndQuoteIndex = 9U;

inline const char* quoteForIndex(uint32_t index) {
  return (index < kQuoteCount) ? kQuotes[index] : nullptr;
}

} // namespace BmoQuotes
