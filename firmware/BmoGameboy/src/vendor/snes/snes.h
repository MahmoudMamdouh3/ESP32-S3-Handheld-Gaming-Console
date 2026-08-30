#ifndef SNES_H
#define SNES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t pc;
  uint16_t a, x, y, sp, dp;
  uint8_t p, db, pb;
  uint8_t* rom;
  uint32_t rom_size;
  uint8_t* wram;
  uint8_t* vram;
  uint8_t* sram;
  uint16_t* framebuffer;
  uint8_t pad_state;
} snes_t;

void snes_init(snes_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* wram, uint16_t* fb);
void snes_step_frame(snes_t* emu);
void snes_set_pad(snes_t* emu, uint8_t pad);

#ifdef __cplusplus
}
#endif

#endif // SNES_H
