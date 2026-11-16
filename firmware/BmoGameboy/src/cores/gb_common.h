// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" {
uint8_t audio_read(uint16_t addr);
void audio_write(uint16_t addr, uint8_t val);
}

namespace GbCommon {

inline constexpr size_t kCartRamSize = 128U * 1024U;

extern uint16_t g_gbRowBuffer[240];

/** Compute validated Game Boy battery-backed SRAM size from ROM header bytes. */
size_t batteryRamSize(uint8_t romType, uint8_t romSizeCode);

/** Allocate aligned Game Boy emulator state struct, preferring internal DRAM then PSRAM. */
void* allocGbStateAligned(size_t stateSize, const char* tag);

/** Allocate and zero-clear 128 KB cartridge RAM in Octal PSRAM. */
uint8_t* allocCartRam(const char* tag, uint32_t heapCaps = MALLOC_CAP_SPIRAM);

/** Wait for display DMA, free GB state/cartridge RAM, and clear ROM bindings. */
void freeGbRuntime(void** gbPtr, uint8_t** cartRamPtr, const uint8_t** romPtr, size_t* romLen);

/**
 * Expand one 160-pixel Game Boy scanline into 240 pixels (3:2 NN: A B C D -> A A B C C D)
 * using three aligned 32-bit stores per 4-pixel group, then copy 1 or 2 rows to async PSRAM.
 */
template <uint8_t PixelMask = 0xFFU>
IRAM_ATTR inline void scaleAndBlitGbLine(const uint8_t* pixels, const uint16_t* palette, uint16_t* asyncFrame,
                                         uint_fast8_t line) {
  uint32_t* out32 = reinterpret_cast<uint32_t*>(g_gbRowBuffer);
  for (int g = 0; g < 40; ++g) {
    const int base = g * 4;
    const uint16_t pA = palette[pixels[base] & PixelMask];
    const uint16_t pB = palette[pixels[base + 1] & PixelMask];
    const uint16_t pC = palette[pixels[base + 2] & PixelMask];
    const uint16_t pD = palette[pixels[base + 3] & PixelMask];

    out32[0] = static_cast<uint32_t>(pA) | (static_cast<uint32_t>(pA) << 16);
    out32[1] = static_cast<uint32_t>(pB) | (static_cast<uint32_t>(pC) << 16);
    out32[2] = static_cast<uint32_t>(pC) | (static_cast<uint32_t>(pD) << 16);
    out32 += 3;
  }

  if (asyncFrame && line < 144U) {
    const int rowsToDraw = (line & 1U) ? 1 : 2;
    const int y0 = (static_cast<int>(line >> 1) * 3) + ((line & 1U) ? 2 : 0);
    uint16_t* dstRow = &asyncFrame[y0 * 240];
    memcpy(dstRow, g_gbRowBuffer, 240U * sizeof(uint16_t));
    if (rowsToDraw == 2) {
      memcpy(dstRow + 240, g_gbRowBuffer, 240U * sizeof(uint16_t));
    }
  }
}

} // namespace GbCommon
