#pragma once
#include <stdint.h>

namespace HostRuntimeFake {
inline uint64_t g_micros = 1000000ULL;
}

inline int64_t esp_timer_get_time(void) {
  return static_cast<int64_t>(HostRuntimeFake::g_micros);
}
