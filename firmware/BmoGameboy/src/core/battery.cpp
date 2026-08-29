#include "battery.h"
#include "config.h"
#include "bmo_face.h"
#include <Arduino.h>

// Calibration values for the voltage divider (to be finalized on real hardware)
// Assumes a 100k / 100k divider on the 3.7V-4.2V LiPo connecting to 3.3V ADC.
#define BATTERY_R1 100000.0f
#define BATTERY_R2 100000.0f
#define ADC_MAX 4095.0f
#define VREF 3.3f // Typical ESP32-S3 VRef

namespace {
    float lastVoltage = 3.7f;
    unsigned long lastReadMs = 0;
}

namespace Battery {
#if FEATURE_BATTERY_MONITOR
void begin() {
#ifdef BATTERY_ADC_PIN
    analogReadResolution(12);
    pinMode(BATTERY_ADC_PIN, INPUT);
    lastVoltage = getVoltage();
#endif
}

float getVoltage() {
#ifdef BATTERY_ADC_PIN
    int raw = analogRead(BATTERY_ADC_PIN);
    float adcVoltage = (raw / ADC_MAX) * VREF;
    float batVoltage = adcVoltage * ((BATTERY_R1 + BATTERY_R2) / BATTERY_R2);
    return batVoltage;
#else
    return 4.2f;
#endif
}

int getPercentage() {
    float v = lastVoltage;
    if (v >= 4.15f) return 100;
    if (v <= 3.3f) return 0;
    int pct = (int)(((v - 3.3f) / (4.15f - 3.3f)) * 100.0f);
    return (pct < 0) ? 0 : ((pct > 100) ? 100 : pct);
}

void safeShutdown() {
    Serial.println("BATTERY CRITICAL! Halting to prevent deep discharge...");
    Serial.flush();
    BmoFace::setExpression(BmoFace::SHUTDOWN);
    BmoFace::draw(); // Force draw immediately before sleep
    esp_deep_sleep_start();
}

void update() {
    unsigned long now = millis();
    if (now - lastReadMs >= 5000) {
        float v = getVoltage();
        lastVoltage = (lastVoltage * 0.8f) + (v * 0.2f);
        
        int pct = getPercentage();
        if (pct < 20 && lastVoltage >= 3.2f) {
            BmoFace::setExpression(BmoFace::LOW_BATTERY);
        }
        
        if (lastVoltage < 3.2f) {
            safeShutdown();
        }
        lastReadMs = now;
    }
}
#else
void begin() {}
float getVoltage() { return 4.2f; }
int getPercentage() { return 100; }
void safeShutdown() {}
void update() {}
#endif
}
