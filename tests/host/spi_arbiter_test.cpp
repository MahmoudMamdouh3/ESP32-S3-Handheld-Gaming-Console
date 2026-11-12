#include "freertos/semphr.h"
#include "spi_arbiter.h"

struct TestSemaphore {
  bool held;
};

bool g_failCreate = false;
bool g_failTake = false;
int g_createCount = 0;
int g_takeCount = 0;
int g_giveCount = 0;
static TestSemaphore g_mutex = {false};

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() {
  ++g_createCount;
  return g_failCreate ? nullptr : &g_mutex;
}

BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t mutex, TickType_t waitTicks) {
  ++g_takeCount;
  if (!mutex || g_failTake || waitTicks != portMAX_DELAY) return pdFALSE;
  mutex->held = true;
  return pdTRUE;
}

BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t mutex) {
  ++g_giveCount;
  if (!mutex || !mutex->held) return pdFALSE;
  mutex->held = false;
  return pdTRUE;
}

int main(int argc, char** argv) {
  if (argc != 2) return 1;
  if (argv[1][0] == 'c') {
    g_failCreate = true;
    SpiArbiter::init();
    return 0;
  }
  if (argv[1][0] == 'b') {
    SpiArbiter::lock();
    return 0;
  }

  SpiArbiter::init();
  if (argv[1][0] == 't') {
    g_failTake = true;
    SpiArbiter::lock();
    return 0;
  }

  SpiArbiter::init();
  SpiArbiter::lock();
  if (g_createCount != 1 || g_takeCount != 1 || !g_mutex.held) return 2;
  SpiArbiter::unlock();
  if (g_giveCount != 1 || g_mutex.held) return 3;
  return 0;
}
