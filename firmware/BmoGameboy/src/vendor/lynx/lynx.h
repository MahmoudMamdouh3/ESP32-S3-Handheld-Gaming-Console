#ifndef LYNX_H
#define LYNX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t pc;
  uint8_t a, x, y, sp, p;
  uint8_t* rom;
  uint32_t rom_size;
  uint8_t* ram;          // 64KB RAM
  uint16_t* framebuffer; // 160 x 102
  uint8_t buttons;
} lynx_t;

void lynx_init(lynx_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb);
void lynx_step_frame(lynx_t* emu);
void lynx_set_buttons(lynx_t* emu, uint8_t buttons);

#ifdef __cplusplus
}
#endif

#endif // LYNX_H
