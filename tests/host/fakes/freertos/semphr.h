#pragma once

#include "FreeRTOS.h"

struct TestSemaphore;
typedef TestSemaphore* SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex();
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t mutex, TickType_t waitTicks);
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t mutex);
