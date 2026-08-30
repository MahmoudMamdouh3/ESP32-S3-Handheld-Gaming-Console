#ifndef COLEM_H
#define COLEM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t pc, sp, af, bc, de, hl;
  uint8_t* rom;
  uint32_t rom_size;
  uint8_t* ram;          // 1KB / 8KB RAM
  uint8_t* vram;         // 16KB VRAM
  uint16_t* framebuffer; // 256 x 192
  uint16_t joystick;
} colem_t;

void colem_init(colem_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint8_t* vram, uint16_t* fb);
void colem_step_frame(colem_t* emu);
void colem_set_joystick(colem_t* emu, uint16_t joy);

#ifdef __cplusplus
}
#endif

#endif // COLEM_H
