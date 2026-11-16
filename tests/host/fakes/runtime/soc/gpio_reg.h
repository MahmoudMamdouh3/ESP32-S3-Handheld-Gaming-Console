#pragma once
#include <stdint.h>

#define GPIO_IN_REG 0x6000403CU
#define GPIO_IN1_REG 0x60004040U

namespace HostRuntimeFake {
inline uint32_t g_gpioInLow = 0xFFFFFFFFU;
inline uint32_t g_gpioInHigh = 0xFFFFFFFFU;
} // namespace HostRuntimeFake

inline uint32_t REG_READ(uint32_t reg) {
  return (reg == GPIO_IN1_REG) ? HostRuntimeFake::g_gpioInHigh : HostRuntimeFake::g_gpioInLow;
}
