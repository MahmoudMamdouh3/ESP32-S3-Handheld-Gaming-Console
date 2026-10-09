#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdbool.h>

// -----------------------------------------------------------------------------
// SpiArbiter: Thread-Safe Bus Arbitration for Shared FSPI Bus
// 
// ST7789 display (TFT_CS GPIO10) and MicroSD (SD_CS GPIO13) share MOSI (GPIO11)
// and SCK (GPIO12). Per Rule 28 and HARDWARE-01, concurrent SPI access is strictly
// prohibited.
//
// SpiArbiter provides mutual exclusion across Core 0 (display worker task)
// and Core 1 (main loop / SD card operations).
// -----------------------------------------------------------------------------

namespace SpiArbiter {
  // Initializes the FreeRTOS SPI bus mutex. Idempotent.
  void init();

  // Acquires exclusive lock on the shared SPI bus.
  // Blocks until the bus is available or timeout expires.
  bool lock(TickType_t waitTicks = portMAX_DELAY);

  // Releases exclusive lock on the shared SPI bus.
  void unlock();
}
