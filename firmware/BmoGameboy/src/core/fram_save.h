#pragma once
#include <stdint.h>
#include <stddef.h>

namespace FramSave {
    bool begin();
    
    // Read an entire block (e.g. 8KB) from the FM24C FRAM
    bool readSaveData(uint16_t offset, uint8_t* buffer, size_t length);
    
    // Write a block to the FM24C FRAM. This is non-blocking and handles page boundaries internally.
    bool writeSaveData(uint16_t offset, const uint8_t* buffer, size_t length);
}
