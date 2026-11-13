#pragma once
#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

// The browser build is single threaded. Task creation reports failure so the
// firmware takes its own synchronous code path (the same fallback it uses when
// the display worker task cannot be created on hardware).
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char* name, uint32_t stack,
                                   void* arg, UBaseType_t prio, TaskHandle_t* handle, BaseType_t core);
BaseType_t xTaskCreate(TaskFunction_t fn, const char* name, uint32_t stack,
                       void* arg, UBaseType_t prio, TaskHandle_t* handle);
void vTaskDelay(TickType_t ticks);
void vTaskDelete(TaskHandle_t task);
TickType_t xTaskGetTickCount(void);
#define taskYIELD() do {} while (0)

#ifdef __cplusplus
}
#endif
