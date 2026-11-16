// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

// -----------------------------------------------------------------------------
// board_timing.h - Panel dimensions and SPI bus clock definitions.
// -----------------------------------------------------------------------------

#define TFT_WIDTH 240
#define TFT_HEIGHT 320

#ifndef TFT_SPI_HZ
#define TFT_SPI_HZ 80000000UL
#endif
#define TFT_SPI_FALLBACK_HZ 40000000UL // Diagnostics comparison only; default remains 80 MHz.

#if TFT_SPI_HZ > 62500000UL
#pragma message("TFT_SPI_HZ exceeds the ST7789VW 62.5 MHz published serial-write limit; board timing is unmeasured")
#endif

#define SD_SPI_HZ 25000000UL
