#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ST7789 panel pixel format used by DisplayEmu (BGR565 byte-swapped for SPI DMA)
static inline uint16_t bmo_rgb888_to_panel565(uint8_t r, uint8_t g, uint8_t b) {
  const uint16_t bgr = (uint16_t)(((b & 0xF8U) << 8) | ((g & 0xFCU) << 3) | (r >> 3));
  return (uint16_t)((bgr >> 8) | (bgr << 8));
}

// Convert host-endian RGB565 (R:15-11, G:10-5, B:4-0) to ST7789 byte-swapped BGR565
static inline uint16_t bmo_rgb565_to_panel565(uint16_t rgb565) {
  const uint16_t bgr = (uint16_t)(((rgb565 & 0x001FU) << 11) | (rgb565 & 0x07E0U) | ((rgb565 & 0xF800U) >> 11));
  return (uint16_t)((bgr >> 8) | (bgr << 8));
}

// Convert big-endian RGB565 (as emitted by Handy MIKIE_PIXEL_FORMAT_16BPP_565_BE)
static inline uint16_t bmo_rgb565be_to_panel565(uint16_t rgb565_be) {
  const uint16_t rgb565 = (uint16_t)((rgb565_be >> 8) | (rgb565_be << 8));
  return bmo_rgb565_to_panel565(rgb565);
}

#ifdef __cplusplus
}
#endif
