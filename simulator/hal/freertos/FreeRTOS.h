#pragma once
#include <stdint.h>
#include <stddef.h>

typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;
typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef void (*TaskFunction_t)(void*);

#define pdFALSE 0
#define pdTRUE  1
#define pdPASS  1
#define pdFAIL  0
#define portMAX_DELAY 0xFFFFFFFFu
#define portTICK_PERIOD_MS 1
#define configMAX_PRIORITIES 25
#define configTICK_RATE_HZ 1000
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define tskNO_AFFINITY 0x7FFFFFFF
#define portENTER_CRITICAL(m) do {} while (0)
#define portEXIT_CRITICAL(m)  do {} while (0)
