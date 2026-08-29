#pragma once
#include <stdint.h>

namespace Battery {
    void begin();
    
    // Read the battery voltage (0.0 to ~4.2V)
    float getVoltage();
    
    // Read battery percentage (0 to 100)
    int getPercentage();
    
    // Main processing loop: updates sliding average and triggers sleep if too low
    void update();
    
    // Forces ESP32 into deep sleep immediately to protect the LiPo
    void safeShutdown();
}
