#pragma once
#include "FreeRTOS.h"

struct TestSemaphore {
  bool held = false;
};

using SemaphoreHandle_t = TestSemaphore*;

namespace HostRuntimeFake {
inline TestSemaphore g_mutex{};
}

inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() {
  return &HostRuntimeFake::g_mutex;
}

inline BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t mutex, TickType_t) {
  if (!mutex) return pdFALSE;
  mutex->held = true;
  return pdTRUE;
}

inline BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t mutex) {
  if (!mutex) return pdFALSE;
  mutex->held = false;
  return pdTRUE;
}
