#pragma once
#include "FreeRTOS.h"

inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) {
  return 1024U;
}
inline void vTaskDelay(TickType_t) {}
inline void taskYIELD() {}
