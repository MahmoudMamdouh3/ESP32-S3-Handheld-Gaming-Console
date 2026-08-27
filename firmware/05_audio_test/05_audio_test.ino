// -----------------------------------------------------------------------
// Milestone 5: Audio Subsystem Test
//
// Standalone I2S beep test for the MAX98357A I2S Class-D amplifier.
// Verifies wiring and that audio DMA is correctly configured without
// being intertwined with emulator core complexity.
// -----------------------------------------------------------------------

#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

#define I2S_PORT I2S_NUM_0
#define I2S_BCLK 38
#define I2S_LRC  39
#define I2S_DIN  40

#define SAMPLE_RATE 44100
#define FREQUENCY   440.0 // A4 beep

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting I2S Audio Beep Test...");

  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 512,
      .use_apll = false,
      .tx_desc_auto_clear = true
  };

  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCLK,
      .ws_io_num = I2S_LRC,
      .data_out_num = I2S_DIN,
      .data_in_num = I2S_PIN_NO_CHANGE
  };

  if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) {
    Serial.println("Failed to install I2S driver!");
    while(1) delay(100);
  }

  if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
    Serial.println("Failed to set I2S pins!");
    while(1) delay(100);
  }
  
  i2s_zero_dma_buffer(I2S_PORT);
  Serial.println("I2S initialized. You should hear a 440Hz tone...");
}

void loop() {
  // Generate a simple sine wave
  static float phase = 0.0f;
  float phase_increment = (TWO_PI * FREQUENCY) / SAMPLE_RATE;
  
  for (int i = 0; i < 512; i++) {
    int16_t sample = (int16_t)(sin(phase) * 10000.0f); // ~30% volume
    uint32_t stereo_sample = (uint32_t)((uint16_t)sample) | (((uint32_t)(uint16_t)sample) << 16);
    
    size_t bytes_written;
    i2s_write(I2S_PORT, &stereo_sample, 4, &bytes_written, portMAX_DELAY);
    
    phase += phase_increment;
    if (phase >= TWO_PI) phase -= TWO_PI;
  }
}
