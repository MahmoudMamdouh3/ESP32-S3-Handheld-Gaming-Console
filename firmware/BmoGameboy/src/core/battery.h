#pragma once
#include <stdint.h>

namespace Battery {
    void begin();
    
    // Read the battery voltage (0.0 to ~4.2V). Returns 0.0 when the board has no fitted battery monitor.
    float getVoltage();

    // Read battery voltage in millivolts. Returns 0 when the board has no fitted battery monitor.
    int getVoltageMv();
    
    // Read battery percentage (0 to 100). Returns 0 when the board has no fitted battery monitor.
    int getPercentage();
    
    // Main processing loop: updates sliding average and triggers sleep if too low
    void update();
    
    // Forces ESP32 into deep sleep immediately to protect the LiPo
    void safeShutdown();
}
