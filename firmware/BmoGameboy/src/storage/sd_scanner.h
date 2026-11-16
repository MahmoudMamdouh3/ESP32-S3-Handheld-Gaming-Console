// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include "../hal/config.h"
#include <stddef.h>
#include <stdint.h>

#if FEATURE_SD_CARD
namespace fs {
class File;
}
using fs::File;
#endif

namespace BmoSdScanner {

#if FEATURE_SD_CARD
uint32_t computePrefixCrc32FromFile(File& file, size_t maxBytes, uint8_t* optPrefixOut, size_t* optPrefixLen,
                                    bool readGenesisMagic, uint8_t* optGenesisOut, size_t* optGenesisLen);
#endif
uint32_t computePrefixCrc32FromBuffer(const uint8_t* buf, size_t size);

} // namespace BmoSdScanner
