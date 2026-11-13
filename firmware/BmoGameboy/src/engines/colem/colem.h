#ifndef COLEM_H
#define COLEM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // Z80A CPU Registers
  uint16_t pc, sp, af, bc, de, hl, ix, iy;
  uint16_t af_prime, bc_prime, de_prime, hl_prime;
  uint8_t i, r, iff1, iff2, im;
  bool halted;

  // TMS9918A VDP State
  uint8_t vdp_regs[8];
  uint8_t vdp_status;
  uint8_t vdp_latch;
  uint16_t vdp_addr;
  uint8_t vdp_read_buf;
  uint16_t palette[16];  // Pre-swapped BGR565 TMS9918A 16-color palette

  // Controller & Memory
  bool keypad_mode;      // true = Keypad ($C0), false = Joystick ($E0)
  uint8_t* rom;
  uint32_t rom_size;
  uint8_t* ram;          // 8KB RAM ($6000-$7FFF)
  uint8_t* vram;         // 16KB VRAM
  uint16_t* framebuffer; // 256 x 192
  uint16_t joystick;     // Bit 0: Up, 1: Right, 2: Down, 3: Left, 4: FireL, 5: FireR
} colem_t;

void colem_init(colem_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint8_t* vram, uint16_t* fb);
void colem_step_frame(colem_t* emu);
void colem_set_joystick(colem_t* emu, uint16_t joy);

#ifdef __cplusplus
}
#endif

#endif // COLEM_H
