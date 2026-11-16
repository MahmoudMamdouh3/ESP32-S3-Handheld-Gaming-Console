// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace BmoSdIndexIo {

uint32_t rootDirectoryStamp();
bool verifyIndexV3File(const char* path, size_t expectedSize, uint32_t expectedCrc);
void recoverIndexTransaction();

} // namespace BmoSdIndexIo
