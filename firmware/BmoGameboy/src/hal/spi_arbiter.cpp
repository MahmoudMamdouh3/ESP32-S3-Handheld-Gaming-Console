#include "spi_arbiter.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdlib.h>

namespace {
static SemaphoreHandle_t s_spiMutex = nullptr;
}

void SpiArbiter::init() {
  if (!s_spiMutex) {
    s_spiMutex = xSemaphoreCreateRecursiveMutex();
    if (!s_spiMutex) abort();
  }
}

void SpiArbiter::lock() {
  if (!s_spiMutex || xSemaphoreTakeRecursive(s_spiMutex, portMAX_DELAY) != pdTRUE) {
    abort();
  }
}

void SpiArbiter::unlock() {
  if (!s_spiMutex || xSemaphoreGiveRecursive(s_spiMutex) != pdTRUE) abort();
}
