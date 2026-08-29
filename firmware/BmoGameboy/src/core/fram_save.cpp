#include "fram_save.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>

#define FRAM_I2C_ADDRESS 0x50

namespace FramSave {

bool begin() {
    // Initialize I2C bus with the pins defined in config.h
    if (!Wire.begin(I2C_SDA, I2C_SCL, 400000)) {
        Serial.println("Failed to initialize I2C bus for FRAM.");
        return false;
    }
    
    // Ping the FRAM module to see if it responds
    Wire.beginTransmission(FRAM_I2C_ADDRESS);
    if (Wire.endTransmission() != 0) {
        Serial.println("FRAM module not detected at 0x50!");
        return false;
    }
    
    Serial.println("FRAM module initialized successfully.");
    return true;
}

bool readSaveData(uint16_t offset, uint8_t* buffer, size_t length) {
    if (!buffer || length == 0) return false;
    
    // Basic chunked read for EEPROM/FRAM structure (usually limited to 32 bytes per Wire transaction)
    size_t bytesRead = 0;
    while (bytesRead < length) {
        uint16_t currentOffset = offset + bytesRead;
        size_t chunkSize = min(length - bytesRead, (size_t)32);
        
        Wire.beginTransmission(FRAM_I2C_ADDRESS);
        Wire.write((uint8_t)(currentOffset >> 8));   // MSB
        Wire.write((uint8_t)(currentOffset & 0xFF)); // LSB
        Wire.endTransmission();
        
        Wire.requestFrom(FRAM_I2C_ADDRESS, chunkSize);
        for (size_t i = 0; i < chunkSize; i++) {
            if (Wire.available()) {
                buffer[bytesRead++] = Wire.read();
            }
        }
    }
    return true;
}

bool writeSaveData(uint16_t offset, const uint8_t* buffer, size_t length) {
    if (!buffer || length == 0) return false;
    
    // Write in 32 byte pages
    size_t bytesWritten = 0;
    while (bytesWritten < length) {
        uint16_t currentOffset = offset + bytesWritten;
        size_t chunkSize = min(length - bytesWritten, (size_t)30); // Leave 2 bytes for address
        
        Wire.beginTransmission(FRAM_I2C_ADDRESS);
        Wire.write((uint8_t)(currentOffset >> 8));   // MSB
        Wire.write((uint8_t)(currentOffset & 0xFF)); // LSB
        for (size_t i = 0; i < chunkSize; i++) {
            Wire.write(buffer[bytesWritten++]);
        }
        Wire.endTransmission();
        
        // FRAM has 0 write delay, unlike EEPROM where we'd need delay(5) here!
    }
    return true;
}

}
