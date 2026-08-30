#ifndef GENESIS_H
#define GENESIS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t pc;
  uint32_t d[8];
  uint32_t a[8];
  uint16_t sr;
  uint8_t* rom;
  uint32_t rom_size;
  uint8_t* ram;
  uint8_t* vram;
  uint8_t* cram;
  uint8_t* vsram;
  uint16_t* framebuffer;
  uint16_t scanline;
  uint8_t pad_state;
} genesis_t;

void genesis_init(genesis_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb);
void genesis_step_frame(genesis_t* emu);
void genesis_set_pad(genesis_t* emu, uint8_t pad);

#ifdef __cplusplus
}
#endif

#endif // GENESIS_H
