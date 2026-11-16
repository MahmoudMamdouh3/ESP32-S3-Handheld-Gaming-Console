// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
// Hot Core 0 SPI pixel conversion and scanline streaming worker; Arduino ESP32 builds at -Os.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3,unroll-loops")
#endif

#include "display_pipeline.h"

#include "display_driver.h"
#include "display_internal.h"
#include "doom_palette.h"
#include "../hal/config.h"
#include "../hal/profiler.h"
#include "../hal/spi_arbiter.h"
#include <SPI.h>
#include <cstring>
#include <new>

namespace BmoDisplayInternal {
PSRAMCanvas* menuCanvas = nullptr;
} // namespace BmoDisplayInternal

namespace {

using namespace BmoDisplayInternal;
using BmoDisplayPipeline::OFFSET_X;
using BmoDisplayPipeline::OFFSET_Y;

PSRAMCanvas* s_menuCanvasArr[2] = {nullptr, nullptr};
int s_activeDrawCanvas = 0;

constexpr TickType_t kDisplayIdlePollTicks = pdMS_TO_TICKS(250);
constexpr TickType_t kDisplayCmdTimeoutTicks = pdMS_TO_TICKS(1000);
constexpr TickType_t kDisplayWaitTimeoutTicks = pdMS_TO_TICKS(1000);

uint16_t __attribute__((aligned(4))) s_nesRowBuf[256];
#if BMO_ENABLE_DOOM
struct color {
  uint32_t b : 8;
  uint32_t g : 8;
  uint32_t r : 8;
  uint32_t a : 8;
};
extern "C" struct color colors[256];

uint16_t __attribute__((aligned(4))) s_doomLineBuf[320];
uint16_t s_doomPalette[256];
#endif

enum LastFrameFormat { FRAME_FMT_NONE = 0, FRAME_FMT_RAW16, FRAME_FMT_NES8, FRAME_FMT_DOOM8 };

uint8_t* s_asyncEmuBuf[2] = {nullptr, nullptr};
int s_activeEmuBuf = 0;
uint32_t s_activeSpiHz = TFT_SPI_HZ;
bool s_bezelEnabled = true;
const uint16_t* s_lastFramePtr = nullptr;
const uint8_t* s_lastRaw8Ptr = nullptr;
LastFrameFormat s_lastFrameFmt = FRAME_FMT_NONE;
int s_lastFrameW = 0;
int s_lastFrameH = 0;
uint16_t* s_streamCapBuf = nullptr;
int s_streamCapPixels = 0;

enum DisplayCmdType {
  CMD_NONE = 0,
  CMD_PUSH_CANVAS,
  CMD_PUSH_FULLSCREEN,
  CMD_STREAM_RAW16,
  CMD_STREAM_NES,
  CMD_STREAM_DOOM
};

struct DisplayTaskCmd {
  DisplayCmdType type;
  const void* buffer;
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

QueueHandle_t s_displayQueue = nullptr;
SemaphoreHandle_t s_displayDoneSem = nullptr;
TaskHandle_t s_displayTaskHandle = nullptr;
volatile bool s_displayBusy = false;
uint32_t s_displayQueueTimeouts = 0;

bool enqueueDisplayCmd(const DisplayTaskCmd& cmd) {
  if (xQueueSend(s_displayQueue, &cmd, kDisplayCmdTimeoutTicks) != pdTRUE) {
    s_displayQueueTimeouts++;
    LOG_WARN("DisplayEmu: queue send timed out after 1000 ms (%lu)", (unsigned long)s_displayQueueTimeouts);
    xSemaphoreGive(s_displayDoneSem);
    return false;
  }
  return true;
}

void streamNesPixelsLocked(const uint8_t* nesFb) {
  SpiArbiter::ScopedLock spiLock;
  tft.startWrite();
  tft.setAddrWindow(32, 0, 256, 240);
  uint32_t* out32 = reinterpret_cast<uint32_t*>(s_nesRowBuf);
  for (int y = 0; y < 240; ++y) {
    const uint8_t* inRow = &nesFb[y * 256];
    for (int x = 0; x < 256; x += 4) {
      const uint16_t p0 = DisplayEmu::NES_PALETTE[inRow[x] & 0x3F];
      const uint16_t p1 = DisplayEmu::NES_PALETTE[inRow[x + 1] & 0x3F];
      const uint16_t p2 = DisplayEmu::NES_PALETTE[inRow[x + 2] & 0x3F];
      const uint16_t p3 = DisplayEmu::NES_PALETTE[inRow[x + 3] & 0x3F];
      out32[x >> 1] = static_cast<uint32_t>(p0) | (static_cast<uint32_t>(p1) << 16);
      out32[(x >> 1) + 1] = static_cast<uint32_t>(p2) | (static_cast<uint32_t>(p3) << 16);
    }
    SPI.writeBytes(reinterpret_cast<const uint8_t*>(s_nesRowBuf), 256 * 2);
  }
  tft.endWrite();
}

#if BMO_ENABLE_DOOM
void streamDoomPixelsLocked(const uint8_t* cmap) {
  Ui::rebuildDoomPalette(colors, s_doomPalette, 256U);
  SpiArbiter::ScopedLock spiLock;
  tft.startWrite();
  tft.setAddrWindow(0, 20, 320, 200);
  uint32_t* out32 = reinterpret_cast<uint32_t*>(s_doomLineBuf);
  for (int y = 0; y < 200; ++y) {
    const uint8_t* inRow = &cmap[y * 320];
    for (int x = 0; x < 320; x += 4) {
      const uint16_t p0 = s_doomPalette[inRow[x]];
      const uint16_t p1 = s_doomPalette[inRow[x + 1]];
      const uint16_t p2 = s_doomPalette[inRow[x + 2]];
      const uint16_t p3 = s_doomPalette[inRow[x + 3]];
      out32[x >> 1] = static_cast<uint32_t>(p0) | (static_cast<uint32_t>(p1) << 16);
      out32[(x >> 1) + 1] = static_cast<uint32_t>(p2) | (static_cast<uint32_t>(p3) << 16);
    }
    SPI.writeBytes(reinterpret_cast<const uint8_t*>(s_doomLineBuf), 320 * 2);
  }
  tft.endWrite();
}
#endif

void executeDisplayCmd(const DisplayTaskCmd& cmd) {
  if (!cmd.buffer) return;
  if (cmd.type == CMD_PUSH_CANVAS || cmd.type == CMD_PUSH_FULLSCREEN) {
    BmoDisplayDriver::pushWindowLocked(0, 0, 320, 240, static_cast<const uint16_t*>(cmd.buffer), 320U * 240U * 2U);
  } else if (cmd.type == CMD_STREAM_RAW16) {
    BmoDisplayDriver::pushWindowLocked(cmd.x, cmd.y, cmd.w, cmd.h, static_cast<const uint16_t*>(cmd.buffer),
                                       static_cast<size_t>(cmd.w) * static_cast<size_t>(cmd.h) * 2U);
  } else if (cmd.type == CMD_STREAM_NES) {
    streamNesPixelsLocked(static_cast<const uint8_t*>(cmd.buffer));
#if BMO_ENABLE_DOOM
  } else if (cmd.type == CMD_STREAM_DOOM) {
    streamDoomPixelsLocked(static_cast<const uint8_t*>(cmd.buffer));
#endif
  }
}

void displayWorkerTask(void* pvParameters) {
  (void)pvParameters;
  DisplayTaskCmd cmd{};
  while (true) {
    if (xQueueReceive(s_displayQueue, &cmd, kDisplayIdlePollTicks) == pdTRUE) {
      s_displayBusy = true;
      BMO_PROFILE_START(ZONE_SPI_STREAM);
      executeDisplayCmd(cmd);
      BMO_PROFILE_END(ZONE_SPI_STREAM);
      s_displayBusy = false;
      xSemaphoreGive(s_displayDoneSem);
    }
  }
}

} // namespace

void BmoDisplayInternal::writeMenuCanvas() {
  if (internalHasActiveToast()) {
    drawFloatingToastHUD();
  }
  if (!s_menuCanvasArr[0] || !s_menuCanvasArr[1]) {
    if (menuCanvas) {
      BmoDisplayDriver::pushWindowLocked(0, 0, 320, 240, menuCanvas->getBuffer(), 320U * 240U * 2U);
    }
    return;
  }

  const uint16_t* drawnBuf = s_menuCanvasArr[s_activeDrawCanvas]->getBuffer();
  if (s_displayTaskHandle && s_displayQueue) {
    DisplayEmu::waitForDisplay();
    xSemaphoreTake(s_displayDoneSem, 0);
    const DisplayTaskCmd cmd = {CMD_PUSH_CANVAS, drawnBuf, 0, 0, 320, 240};
    if (enqueueDisplayCmd(cmd)) {
      s_activeDrawCanvas = 1 - s_activeDrawCanvas;
      menuCanvas = s_menuCanvasArr[s_activeDrawCanvas];
    }
  } else {
    BmoDisplayDriver::pushWindowLocked(0, 0, 320, 240, drawnBuf, 320U * 240U * 2U);
  }
}

const uint16_t DisplayEmu::CLASSIC_PALETTE[4] = {0xF30D, 0x710D, 0x0633, 0xC109};

const uint16_t DisplayEmu::NES_PALETTE[64] = {
    0x2C63, 0x5101, 0x9410, 0x1438, 0x0F58, 0x0868, 0x2068, 0xE050, 0xA031, 0x400A, 0x8002, 0x6102, 0x0902,
    0x0000, 0x0000, 0x0000, 0x75AD, 0xFB12, 0x1F42, 0x3F71, 0xD9A0, 0xEFB0, 0x84B1, 0x609A, 0x606B, 0x203C,
    0x800C, 0x6604, 0xF103, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x9F65, 0x9F94, 0xBFC3, 0x5FF3, 0x79FB, 0x0EFC,
    0xE4EC, 0xE0BD, 0xC08E, 0x265F, 0x1047, 0x7B4E, 0x694A, 0x0000, 0x0000, 0xFFFF, 0x3FA7, 0xDFBD, 0xDFDD,
    0xDFFD, 0x38FD, 0x96F6, 0x15FF, 0xCFFE, 0xCFDF, 0xD7BF, 0xDBBF, 0xFF07, 0xDFFE, 0x0000, 0x0000,
};

void DisplayEmu::begin(uint32_t initialSpiHz) {
  SpiArbiter::init();
  s_activeSpiHz = (initialSpiHz == TFT_SPI_FALLBACK_HZ) ? TFT_SPI_FALLBACK_HZ : TFT_SPI_HZ;

  if (!s_asyncEmuBuf[0]) {
    s_asyncEmuBuf[0] = static_cast<uint8_t*>(heap_caps_malloc(320 * 240 * 2, MALLOC_CAP_SPIRAM));
    s_asyncEmuBuf[1] = static_cast<uint8_t*>(heap_caps_malloc(320 * 240 * 2, MALLOC_CAP_SPIRAM));
    s_activeEmuBuf = 0;
  }
  if (!s_displayQueue) s_displayQueue = xQueueCreate(1, sizeof(DisplayTaskCmd));
  if (!s_displayDoneSem) {
    s_displayDoneSem = xSemaphoreCreateBinary();
    xSemaphoreGive(s_displayDoneSem);
  }
  if (!s_displayTaskHandle) {
    xTaskCreatePinnedToCore(displayWorkerTask, "BMO_Display", 4096, nullptr, configMAX_PRIORITIES - 2,
                            &s_displayTaskHandle, 0);
  }

  BmoDisplayDriver::initPanelHardware(s_activeSpiHz);
  initMenuUI();
}

void DisplayEmu::startFrame() {
  waitForDisplay();
  BmoDisplayDriver::beginRawWindowLocked(OFFSET_X, OFFSET_Y, 240, 216);
  s_streamCapBuf = reinterpret_cast<uint16_t*>(s_asyncEmuBuf[s_activeEmuBuf]);
  s_streamCapPixels = 0;
}

void DisplayEmu::endFrame() {
  BmoDisplayDriver::endRawWindowLocked();
  if (s_streamCapBuf && s_streamCapPixels == 240 * 216) {
    s_lastFrameW = 240;
    s_lastFrameH = 216;
    s_lastFrameFmt = FRAME_FMT_RAW16;
    s_lastFramePtr = s_streamCapBuf;
    s_lastRaw8Ptr = nullptr;
  }
  s_streamCapBuf = nullptr;
  s_streamCapPixels = 0;
}

void DisplayEmu::streamPixelRow(const uint16_t* buf, int pixelCount) {
  if (s_streamCapBuf && buf && pixelCount > 0 && s_streamCapPixels + pixelCount <= 240 * 216) {
    memcpy(&s_streamCapBuf[s_streamCapPixels], buf, static_cast<size_t>(pixelCount) * sizeof(uint16_t));
  }
  s_streamCapPixels += pixelCount;
  SPI.writeBytes(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(pixelCount) * 2U);
}

uint16_t* DisplayEmu::acquireAsyncFrameBuffer() {
  if (s_asyncEmuBuf[0] && s_asyncEmuBuf[1]) {
    return reinterpret_cast<uint16_t*>(s_asyncEmuBuf[s_activeEmuBuf]);
  }
  return reinterpret_cast<uint16_t*>(s_asyncEmuBuf[0]);
}

void DisplayEmu::submitAsyncFrameBuffer(uint16_t* fb, int x, int y, int w, int h) {
  if (!fb || w <= 0 || h <= 0) return;
  const size_t byteCount = static_cast<size_t>(w) * static_cast<size_t>(h) * 2U;
  if (byteCount > (320U * 240U * 2U)) return;

  s_lastFrameW = w;
  s_lastFrameH = h;
  s_lastFrameFmt = FRAME_FMT_RAW16;
  s_lastRaw8Ptr = nullptr;
  s_lastFramePtr = fb;

  if (s_displayTaskHandle && s_displayQueue && s_asyncEmuBuf[0] && s_asyncEmuBuf[1]) {
    waitForDisplay();
    xSemaphoreTake(s_displayDoneSem, 0);
    const DisplayTaskCmd cmd = {CMD_STREAM_RAW16,        fb,
                                static_cast<int16_t>(x), static_cast<int16_t>(y),
                                static_cast<int16_t>(w), static_cast<int16_t>(h)};
    if (enqueueDisplayCmd(cmd)) {
      if (fb == reinterpret_cast<uint16_t*>(s_asyncEmuBuf[0]))
        s_activeEmuBuf = 1;
      else if (fb == reinterpret_cast<uint16_t*>(s_asyncEmuBuf[1]))
        s_activeEmuBuf = 0;
      else
        s_activeEmuBuf = 1 - s_activeEmuBuf;
    }
  } else {
    waitForDisplay();
    BMO_PROFILE_START(ZONE_SPI_STREAM);
    BmoDisplayDriver::pushWindowLocked(x, y, w, h, fb, byteCount);
    BMO_PROFILE_END(ZONE_SPI_STREAM);
  }
}

void DisplayEmu::submitGBFrameAsync(uint16_t* gb_framebuffer) {
  submitAsyncFrameBuffer(gb_framebuffer, OFFSET_X, OFFSET_Y, 240, 216);
}

void DisplayEmu::streamRawFrameAsync(const uint16_t* fb, int x, int y, int w, int h) {
  if (!fb || w <= 0 || h <= 0) return;
  const size_t byteCount = static_cast<size_t>(w) * static_cast<size_t>(h) * 2U;
  if (byteCount > (320U * 240U * 2U)) return;

  s_lastFrameW = w;
  s_lastFrameH = h;
  s_lastFrameFmt = FRAME_FMT_RAW16;
  s_lastRaw8Ptr = nullptr;

  if (s_displayTaskHandle && s_displayQueue && s_asyncEmuBuf[0] && s_asyncEmuBuf[1]) {
    waitForDisplay();
    xSemaphoreTake(s_displayDoneSem, 0);
    uint8_t* dst = s_asyncEmuBuf[s_activeEmuBuf];
    memcpy(dst, fb, byteCount);
    s_lastFramePtr = reinterpret_cast<const uint16_t*>(dst);
    const DisplayTaskCmd cmd = {CMD_STREAM_RAW16,        dst,
                                static_cast<int16_t>(x), static_cast<int16_t>(y),
                                static_cast<int16_t>(w), static_cast<int16_t>(h)};
    if (enqueueDisplayCmd(cmd)) s_activeEmuBuf = 1 - s_activeEmuBuf;
  } else {
    s_lastFramePtr = fb;
    waitForDisplay();
    BmoDisplayDriver::pushWindowLocked(x, y, w, h, fb, byteCount);
  }
}

void DisplayEmu::streamNESFrame(const uint8_t* nes_framebuffer) {
  if (!nes_framebuffer) return;
  s_lastFrameW = 256;
  s_lastFrameH = 240;
  s_lastFrameFmt = FRAME_FMT_NES8;
  s_lastFramePtr = nullptr;

  if (s_displayTaskHandle && s_displayQueue && s_asyncEmuBuf[0] && s_asyncEmuBuf[1]) {
    waitForDisplay();
    xSemaphoreTake(s_displayDoneSem, 0);
    uint8_t* dst = s_asyncEmuBuf[s_activeEmuBuf];
    memcpy(dst, nes_framebuffer, 256 * 240);
    s_lastRaw8Ptr = dst;
    const DisplayTaskCmd cmd = {CMD_STREAM_NES, dst, 32, 0, 256, 240};
    if (enqueueDisplayCmd(cmd)) s_activeEmuBuf = 1 - s_activeEmuBuf;
    return;
  }
  s_lastRaw8Ptr = nes_framebuffer;
  waitForDisplay();
  streamNesPixelsLocked(nes_framebuffer);
}

#if BMO_ENABLE_DOOM
void DisplayEmu::streamDoomFrame(const uint8_t* cmap) {
  if (!cmap) return;
  s_lastFrameW = 320;
  s_lastFrameH = 200;
  s_lastFrameFmt = FRAME_FMT_DOOM8;
  s_lastFramePtr = nullptr;

  if (s_displayTaskHandle && s_displayQueue && s_asyncEmuBuf[0] && s_asyncEmuBuf[1]) {
    waitForDisplay();
    xSemaphoreTake(s_displayDoneSem, 0);
    uint8_t* dst = s_asyncEmuBuf[s_activeEmuBuf];
    memcpy(dst, cmap, 320 * 200);
    s_lastRaw8Ptr = dst;
    const DisplayTaskCmd cmd = {CMD_STREAM_DOOM, dst, 0, 20, 320, 200};
    if (enqueueDisplayCmd(cmd)) s_activeEmuBuf = 1 - s_activeEmuBuf;
    return;
  }
  s_lastRaw8Ptr = cmap;
  waitForDisplay();
  streamDoomPixelsLocked(cmap);
}
#else
void DisplayEmu::streamDoomFrame(const uint8_t* cmap) {
  (void)cmap;
}
#endif

void DisplayEmu::streamSMSFrame(const uint16_t* sms_framebuffer, bool isGameGear) {
  if (!sms_framebuffer) return;
  if (!isGameGear) {
    streamRawFrameAsync(sms_framebuffer, 32, 24, 256, 192);
    return;
  }
  s_lastFrameW = 160;
  s_lastFrameH = 144;
  s_lastFrameFmt = FRAME_FMT_RAW16;
  s_lastRaw8Ptr = nullptr;
  if (s_displayTaskHandle && s_displayQueue && s_asyncEmuBuf[0] && s_asyncEmuBuf[1]) {
    waitForDisplay();
    xSemaphoreTake(s_displayDoneSem, 0);
    uint16_t* dst = reinterpret_cast<uint16_t*>(s_asyncEmuBuf[s_activeEmuBuf]);
    for (int r = 0; r < 144; ++r)
      memcpy(&dst[r * 160], &sms_framebuffer[(24 + r) * 256 + 48], 160 * sizeof(uint16_t));
    s_lastFramePtr = dst;
    const DisplayTaskCmd cmd = {CMD_STREAM_RAW16, dst, 80, 48, 160, 144};
    if (enqueueDisplayCmd(cmd)) s_activeEmuBuf = 1 - s_activeEmuBuf;
    return;
  }
  waitForDisplay();
  SpiArbiter::ScopedLock spiLock;
  tft.startWrite();
  tft.setAddrWindow(80, 48, 160, 144);
  for (int r = 0; r < 144; ++r) {
    SPI.writeBytes(reinterpret_cast<const uint8_t*>(&sms_framebuffer[(24 + r) * 256 + 48]), 160 * 2);
  }
  tft.endWrite();
}

void DisplayEmu::streamPCEFrame(const uint16_t* pce_framebuffer) {
  streamRawFrameAsync(pce_framebuffer, 32, 0, 256, 240);
}
void DisplayEmu::streamAtariFrame(const uint16_t* atari_framebuffer) {
  streamRawFrameAsync(atari_framebuffer, 80, 24, 160, 192);
}
void DisplayEmu::streamPicoFrame(const uint16_t* pico_framebuffer) {
  streamRawFrameAsync(pico_framebuffer, 96, 56, 128, 128);
}
void DisplayEmu::streamGenesisFrame(const uint16_t* genesis_framebuffer, int width, int height) {
  (void)width;
  (void)height;
  streamRawFrameAsync(genesis_framebuffer, 0, 8, 320, 224);
}
void DisplayEmu::streamSNESFrame(const uint16_t* snes_framebuffer, int width, int height) {
  (void)width;
  (void)height;
  streamRawFrameAsync(snes_framebuffer, 32, 8, 256, 224);
}
void DisplayEmu::streamWSwanFrame(const uint16_t* wswan_framebuffer, int width, int height) {
  (void)width;
  (void)height;
  streamRawFrameAsync(wswan_framebuffer, 48, 48, 224, 144);
}
void DisplayEmu::streamNGPFrame(const uint16_t* ngp_framebuffer, int width, int height) {
  (void)width;
  (void)height;
  streamRawFrameAsync(ngp_framebuffer, 80, 44, 160, 152);
}
void DisplayEmu::streamLynxFrame(const uint16_t* lynx_framebuffer, int width, int height) {
  (void)width;
  (void)height;
  streamRawFrameAsync(lynx_framebuffer, 80, 69, 160, 102);
}
void DisplayEmu::streamColemFrame(const uint16_t* colem_framebuffer, int width, int height) {
  (void)width;
  (void)height;
  streamRawFrameAsync(colem_framebuffer, 32, 24, 256, 192);
}
void DisplayEmu::streamGBFrame(const uint16_t* gb_framebuffer) {
  streamRawFrameAsync(gb_framebuffer, OFFSET_X, OFFSET_Y, 240, 216);
}
void DisplayEmu::pushPixelsFullScreen(const uint16_t* buffer) {
  streamRawFrameAsync(buffer, 0, 0, 320, 240);
}

void DisplayEmu::initMenuUI() {
  if (s_menuCanvasArr[0]) return;
  s_menuCanvasArr[0] = new (std::nothrow) PSRAMCanvas(320, 240);
  s_menuCanvasArr[1] = new (std::nothrow) PSRAMCanvas(320, 240);
  if (!s_menuCanvasArr[0] || !s_menuCanvasArr[0]->getBuffer() || !s_menuCanvasArr[1] ||
      !s_menuCanvasArr[1]->getBuffer()) {
    delete s_menuCanvasArr[0];
    delete s_menuCanvasArr[1];
    s_menuCanvasArr[0] = nullptr;
    s_menuCanvasArr[1] = nullptr;
    menuCanvas = nullptr;
    return;
  }
  s_activeDrawCanvas = 0;
  menuCanvas = s_menuCanvasArr[0];
}

void DisplayEmu::cleanupMenuUI() {
  waitForDisplay();
}

void DisplayEmu::waitForDisplay() {
  if (s_displayTaskHandle && s_displayDoneSem) {
    if (xSemaphoreTake(s_displayDoneSem, kDisplayWaitTimeoutTicks) == pdTRUE) {
      xSemaphoreGive(s_displayDoneSem);
    } else {
      LOG_WARN("DisplayEmu: waitForDisplay timed out after 1000 ms");
    }
  }
}

bool DisplayEmu::isDisplayBusy() {
  return s_displayBusy;
}
void DisplayEmu::setBezelEnabled(bool enabled) {
  s_bezelEnabled = enabled;
}
bool DisplayEmu::isBezelEnabled() {
  return s_bezelEnabled;
}
bool DisplayEmu::toggleBezel() {
  s_bezelEnabled = !s_bezelEnabled;
  return s_bezelEnabled;
}

const uint16_t* DisplayEmu::getLastFramebuffer(int* outWidth, int* outHeight) {
  waitForDisplay();
  if (outWidth) *outWidth = s_lastFrameW;
  if (outHeight) *outHeight = s_lastFrameH;

  if (s_lastFrameFmt == FRAME_FMT_NES8 && s_lastRaw8Ptr && s_asyncEmuBuf[0] && s_asyncEmuBuf[1]) {
    uint16_t* convBuf =
        reinterpret_cast<uint16_t*>((s_lastRaw8Ptr == s_asyncEmuBuf[0]) ? s_asyncEmuBuf[1] : s_asyncEmuBuf[0]);
    for (int i = 0; i < 256 * 240; ++i)
      convBuf[i] = NES_PALETTE[s_lastRaw8Ptr[i] & 0x3F];
    s_lastFramePtr = convBuf;
    s_lastFrameFmt = FRAME_FMT_RAW16;
#if BMO_ENABLE_DOOM
  } else if (s_lastFrameFmt == FRAME_FMT_DOOM8 && s_lastRaw8Ptr && s_asyncEmuBuf[0] && s_asyncEmuBuf[1]) {
    uint16_t* convBuf =
        reinterpret_cast<uint16_t*>((s_lastRaw8Ptr == s_asyncEmuBuf[0]) ? s_asyncEmuBuf[1] : s_asyncEmuBuf[0]);
    for (int i = 0; i < 320 * 200; ++i)
      convBuf[i] = s_doomPalette[s_lastRaw8Ptr[i]];
    s_lastFramePtr = convBuf;
    s_lastFrameFmt = FRAME_FMT_RAW16;
#endif
  }
  return s_lastFramePtr;
}

void DisplayEmu::resetLastFramebuffer() {
  waitForDisplay();
  s_lastFramePtr = nullptr;
  s_lastRaw8Ptr = nullptr;
  s_lastFrameFmt = FRAME_FMT_NONE;
  s_lastFrameW = 0;
  s_lastFrameH = 0;
  s_streamCapBuf = nullptr;
  s_streamCapPixels = 0;
}

void DisplayEmu::drawConsoleBezel(int consoleIndex) {
  if (!s_bezelEnabled || !menuCanvas) {
    clearScreen();
    return;
  }
  const ConsoleCatalog::ConsoleSpec* spec = ConsoleCatalog::findByEmulatorIndex(consoleIndex);
  const int vx = spec ? spec->viewX : 40;
  const int vy = spec ? spec->viewY : 12;
  const int vw = spec ? spec->viewW : 240;
  const int vh = spec ? spec->viewH : 216;
  const char* label = spec ? spec->bezelLabel : "BMO HANDHELD";

  menuCanvas->fillScreen(UI_DEEP_TEAL);
  menuCanvas->drawRoundRect(2, 2, 316, 236, 8, UI_TEAL);
  if (vx >= 2 && vy >= 2) {
    menuCanvas->drawRect(vx - 2, vy - 2, vw + 4, vh + 4, UI_YELLOW);
    menuCanvas->drawRect(vx - 1, vy - 1, vw + 2, vh + 2, UI_BLACK);
  }
  menuCanvas->fillRect(vx, vy, vw, vh, UI_BLACK);
  if (vx >= 24) {
    menuCanvas->fillCircle(vx / 2, 32, 3, UI_CORAL);
    menuCanvas->drawCircle(vx / 2, 32, 4, UI_BLACK);
    const int rightWingX = vx + vw + (320 - (vx + vw)) / 2;
    for (int g = 0; g < 4; ++g)
      menuCanvas->drawFastHLine(rightWingX - 6, 180 + g * 5, 12, UI_TEAL);
  }
  if (vy >= 12) {
    menuCanvas->setFont();
    drawCentered(label, (vy - 8) / 2 + 1, UI_MINT);
  }
  writeMenuCanvas();
  waitForDisplay();
}

void DisplayEmu::setDiagnosticSpiClock(uint32_t spiClockHz) {
  if (spiClockHz != TFT_SPI_HZ && spiClockHz != TFT_SPI_FALLBACK_HZ) return;
  waitForDisplay();
  s_activeSpiHz = spiClockHz;
  BmoDisplayDriver::applySpiClock(spiClockHz);
}

uint32_t DisplayEmu::getDiagnosticSpiClock() {
  return s_activeSpiHz;
}

uint32_t DisplayEmu::getDisplayStackHighWaterMarkWords() {
  return s_displayTaskHandle ? static_cast<uint32_t>(uxTaskGetStackHighWaterMark(s_displayTaskHandle)) : 0U;
}
