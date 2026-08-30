#ifndef NGP_H
#define NGP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t pc;
  uint32_t r[8];
  uint8_t* rom;
  uint32_t rom_size;
  uint8_t* ram;
  uint16_t* framebuffer; // 160 x 152
  uint8_t joypad;
  bool is_color;
} ngp_t;

void ngp_init(ngp_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb, bool is_color);
void ngp_step_frame(ngp_t* emu);
void ngp_set_joypad(ngp_t* emu, uint8_t pad);

#ifdef __cplusplus
}
#endif

#endif // NGP_H
