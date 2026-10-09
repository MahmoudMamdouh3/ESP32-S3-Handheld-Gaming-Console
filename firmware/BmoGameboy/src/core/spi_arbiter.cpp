#include "spi_arbiter.h"

namespace {
  static SemaphoreHandle_t s_spiMutex = nullptr;
}

void SpiArbiter::init() {
  if (!s_spiMutex) {
    s_spiMutex = xSemaphoreCreateRecursiveMutex();
  }
}

bool SpiArbiter::lock(TickType_t waitTicks) {
  if (!s_spiMutex) return true;
  return xSemaphoreTakeRecursive(s_spiMutex, waitTicks) == pdTRUE;
}

void SpiArbiter::unlock() {
  if (s_spiMutex) {
    xSemaphoreGiveRecursive(s_spiMutex);
  }
}
