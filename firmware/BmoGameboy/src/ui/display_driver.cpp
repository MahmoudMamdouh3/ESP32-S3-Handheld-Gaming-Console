// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3,unroll-loops")
#endif

#include "display_driver.h"

#include "display_internal.h"
#include "display_pipeline.h"
#include "../hal/config.h"
#include "../hal/spi_arbiter.h"
#include <SPI.h>
#include <esp_heap_caps.h>
#include <string.h>
#if defined(ESP_PLATFORM)
#include <esp_memory_utils.h>
#endif

namespace BmoDisplayInternal {

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST); // NOLINT(bugprone-throwing-static-initialization)

} // namespace BmoDisplayInternal

namespace BmoDisplayDriver {

namespace {

constexpr uint8_t MADCTL_MY = 0x80;
constexpr uint8_t MADCTL_MV = 0x20;
constexpr uint8_t MADCTL_BGR = 0x08;
constexpr size_t kDmaBounceStripBytes = 320U * 16U * sizeof(uint16_t); // 10,240 B DMA-capable internal DRAM strip
uint8_t* s_dmaBounceStrip = nullptr;

void ensureDmaBounceStrip() {
  if (!s_dmaBounceStrip) {
    s_dmaBounceStrip =
        static_cast<uint8_t*>(heap_caps_malloc(kDmaBounceStripBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  }
}

bool needsDmaBounce(const void* ptr) {
#if defined(ESP_PLATFORM)
  return ptr != nullptr && esp_ptr_external_ram(ptr);
#else
  (void)ptr;
  return false;
#endif
}

} // namespace

void initPanelHardware(uint32_t spiClockHz) {
  using namespace BmoDisplayInternal;
  ensureDmaBounceStrip();
  SpiArbiter::ScopedLock spiLock;
  tft.init(TFT_WIDTH, TFT_HEIGHT);
  tft.setSPISpeed(spiClockHz);
  tft.setRotation(3);

  const uint8_t madctl = MADCTL_MY | MADCTL_MV | MADCTL_BGR;
  tft.sendCommand(0x36, &madctl, 1);

  tft.fillScreen(ST77XX_BLACK);
}

void applySpiClock(uint32_t spiClockHz) {
  SpiArbiter::ScopedLock spiLock;
  BmoDisplayInternal::tft.setSPISpeed(spiClockHz);
}

void fillPanelBlack() {
  BmoDisplayInternal::invalidateMenuRenderCache();
  SpiArbiter::ScopedLock spiLock;
  BmoDisplayInternal::tft.fillScreen(ST77XX_BLACK);
}

void pushWindowLocked(int x, int y, int w, int h, const uint16_t* data, size_t byteCount) {
  if (!data || byteCount == 0) return;
  SpiArbiter::ScopedLock spiLock;
  BmoDisplayInternal::tft.startWrite();
  BmoDisplayInternal::tft.setAddrWindow(x, y, w, h);
  const uint8_t* src = reinterpret_cast<const uint8_t*>(data);
  if (s_dmaBounceStrip && needsDmaBounce(src)) {
    size_t offset = 0;
    while (offset < byteCount) {
      const size_t remaining = byteCount - offset;
      const size_t chunk = (remaining > kDmaBounceStripBytes) ? kDmaBounceStripBytes : remaining;
      memcpy(s_dmaBounceStrip, src + offset, chunk);
      SPI.writeBytes(s_dmaBounceStrip, chunk);
      offset += chunk;
    }
  } else {
    SPI.writeBytes(src, byteCount);
  }
  BmoDisplayInternal::tft.endWrite();
}

void beginRawWindowLocked(int x, int y, int w, int h) {
  SpiArbiter::lock();
  BmoDisplayInternal::tft.startWrite();
  BmoDisplayInternal::tft.setAddrWindow(x, y, w, h);
}

void endRawWindowLocked() {
  BmoDisplayInternal::tft.endWrite();
  SpiArbiter::unlock();
}

void writeRawBytes(const uint8_t* data, size_t len) {
  SPI.writeBytes(data, len);
}

} // namespace BmoDisplayDriver

void DisplayEmu::clearScreen() {
  waitForDisplay();
  BmoDisplayDriver::fillPanelBlack();
}

void DisplayEmu::pushPixels(int yOffset, const uint16_t* rowBuffer, int rowsToDraw) {
  waitForDisplay();
  BmoDisplayDriver::pushWindowLocked(BmoDisplayPipeline::OFFSET_X, BmoDisplayPipeline::OFFSET_Y + yOffset, 240,
                                     rowsToDraw, rowBuffer, static_cast<size_t>(240 * rowsToDraw) * sizeof(uint16_t));
}

void DisplayEmu::pushPixelsRaw(int yOffset, const uint16_t* rowBuffer, int rowsToDraw) {
  BmoDisplayInternal::tft.setAddrWindow(BmoDisplayPipeline::OFFSET_X, BmoDisplayPipeline::OFFSET_Y + yOffset, 240,
                                        rowsToDraw);
  SPI.writeBytes(reinterpret_cast<const uint8_t*>(rowBuffer), static_cast<size_t>(240 * rowsToDraw) * sizeof(uint16_t));
}

void DisplayEmu::pushPixelsAt(int x, int y, int w, int h, const uint16_t* buf) {
  waitForDisplay();
  BmoDisplayDriver::pushWindowLocked(x, y, w, h, buf, static_cast<size_t>(w) * static_cast<size_t>(h) * 2U);
}

void DisplayEmu::startDirectWindow(int x, int y, int w, int h) {
  waitForDisplay();
  BmoDisplayDriver::beginRawWindowLocked(x, y, w, h);
}

void DisplayEmu::writeWindowBytes(const uint8_t* data, size_t len) {
  SPI.writeBytes(data, len);
}

void DisplayEmu::endDirectWindow() {
  BmoDisplayDriver::endRawWindowLocked();
}
