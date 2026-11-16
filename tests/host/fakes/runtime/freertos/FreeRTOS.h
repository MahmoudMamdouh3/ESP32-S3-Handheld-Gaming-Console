#pragma once
#include <stdint.h>

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef void* TaskHandle_t;

#ifndef pdTRUE
#define pdTRUE 1
#endif
#ifndef pdFALSE
#define pdFALSE 0
#endif
#ifndef portMAX_DELAY
#define portMAX_DELAY UINT32_MAX
#endif
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) static_cast<TickType_t>(ms)
#endif

typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux)                                                                                        \
  do {                                                                                                                 \
    (void)(mux);                                                                                                       \
  } while (0)
#define portEXIT_CRITICAL(mux)                                                                                         \
  do {                                                                                                                 \
    (void)(mux);                                                                                                       \
  } while (0)
