// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace BmoDisplayDriver {

void initPanelHardware(uint32_t spiClockHz);
void applySpiClock(uint32_t spiClockHz);
void fillPanelBlack();
void pushWindowLocked(int x, int y, int w, int h, const uint16_t* data, size_t byteCount);
void beginRawWindowLocked(int x, int y, int w, int h);
void endRawWindowLocked();
void writeRawBytes(const uint8_t* data, size_t len);

} // namespace BmoDisplayDriver
