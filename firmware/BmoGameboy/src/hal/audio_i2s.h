// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace AudioI2S {
bool begin();
// Push a stereo pair of 16-bit samples to the I2S DMA ring buffer
void pushSample(int16_t left, int16_t right);
} // namespace AudioI2S
