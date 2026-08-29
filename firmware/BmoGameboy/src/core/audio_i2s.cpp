#include "audio_i2s.h"
#include "config.h"

#if FEATURE_AUDIO
#include <Arduino.h>
#include <driver/i2s.h>

#define I2S_PORT I2S_NUM_0

namespace AudioI2S {

bool begin() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100, // Standard GB/GBC sample rate
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // Interrupt level 1
        .dma_buf_count = 4,                       // Number of DMA buffers
        .dma_buf_len = 512,                       // Size of each DMA buffer
        .use_apll = false,
        .tx_desc_auto_clear = true
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = I2S_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Failed to install I2S driver: %d\n", err);
        return false;
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("Failed to set I2S pins: %d\n", err);
        return false;
    }

    // Clear DMA buffer
    i2s_zero_dma_buffer(I2S_PORT);
    return true;
}

void pushSample(int16_t left, int16_t right) {
    uint32_t sample = (uint32_t)(uint16_t)left | ((uint32_t)(uint16_t)right << 16);
    size_t bytes_written;
    // Push to DMA. This will block if the DMA buffer is full, providing natural audio sync!
    i2s_write(I2S_PORT, &sample, 4, &bytes_written, portMAX_DELAY);
}

}
#else
namespace AudioI2S {
    bool begin() { return true; }
    void pushSample(int16_t left, int16_t right) {}
}
#endif
