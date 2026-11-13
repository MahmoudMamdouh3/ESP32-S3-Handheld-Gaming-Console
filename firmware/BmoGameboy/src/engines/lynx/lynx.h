#ifndef LYNX_H
#define LYNX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // WDC 65SC02 CPU Registers
  uint16_t pc;
  uint8_t a, x, y, sp, p;
  bool halted;

  // Suzy ($FC00-$FCFF) & Mikey ($FD00-$FDFF) Hardware State
  uint8_t suzy[256];
  uint8_t mikey[256];
  uint16_t palette_cache[16]; // 16-color BGR565 pre-swapped Mikey palette
  uint16_t disp_addr;         // Video DMA framebuffer base address in 64KB RAM
  uint32_t rom_shift_addr;    // Cartridge serial shift address counter

  uint8_t* rom;
  uint32_t rom_size;
  uint32_t header_len;
  uint8_t* ram;               // 64KB Unified RAM in PSRAM
  uint16_t* framebuffer;      // 160 x 102 BGR565 in PSRAM
  uint8_t buttons;            // Bit 0: Up, 1: Down, 2: Left, 3: Right, 4: A, 5: B, 6: Opt1
} lynx_t;

void lynx_init(lynx_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb);
void lynx_step_frame(lynx_t* emu);
void lynx_set_buttons(lynx_t* emu, uint8_t buttons);

#ifdef __cplusplus
}
#endif

#endif // LYNX_H
