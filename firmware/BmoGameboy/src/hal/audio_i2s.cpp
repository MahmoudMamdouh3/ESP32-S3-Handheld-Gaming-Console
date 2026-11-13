#include "audio_i2s.h"
#include "config.h"

#if FEATURE_AUDIO
#if !defined(BMO_HARDWARE_CONFIRMED_AUDIO) || !BMO_HARDWARE_CONFIRMED_AUDIO
#error "FEATURE_AUDIO=1 requires -DBMO_HARDWARE_CONFIRMED_AUDIO=1 confirming physical I2S DAC hardware"
#endif
#include <Arduino.h>
#include <driver/i2s.h>

#define I2S_PORT I2S_NUM_0

namespace AudioI2S {

bool begin() {
  i2s_config_t i2s_config = {};
  i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  i2s_config.sample_rate = 44100; // Standard GB/GBC sample rate
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1; // Interrupt level 1
  i2s_config.dma_desc_num = 4;                        // Number of DMA descriptors
  i2s_config.dma_frame_num = 512;                     // Frames per DMA buffer
  i2s_config.use_apll = false;
  i2s_config.tx_desc_auto_clear = true;

  i2s_pin_config_t pin_config = {};
  pin_config.mck_io_num = I2S_PIN_NO_CHANGE;
  pin_config.bck_io_num = I2S_BCLK;
  pin_config.ws_io_num = I2S_LRC;
  pin_config.data_out_num = I2S_DIN;
  pin_config.data_in_num = I2S_PIN_NO_CHANGE;

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    LOG_ERROR("Failed to install I2S driver: %d", err);
    return false;
  }

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    LOG_ERROR("Failed to set I2S pins: %d", err);
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

} // namespace AudioI2S
#else
namespace AudioI2S {
bool begin() {
  return true;
}
void pushSample(int16_t left, int16_t right) {
  (void)left;
  (void)right;
}
} // namespace AudioI2S
#endif
