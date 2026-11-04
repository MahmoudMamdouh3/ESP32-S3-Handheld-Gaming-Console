#pragma once
// Host simulator GPIO input register. Buttons::update() reads this exactly as
// it reads the ESP32-S3 GPIO_IN_REG on hardware.
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
uint32_t sim_gpio_in_reg(void);
#ifdef __cplusplus
}
#endif

#define GPIO_IN_REG 0x3FF44000
#define REG_READ(addr) (((addr) == GPIO_IN_REG) ? sim_gpio_in_reg() : 0u)
