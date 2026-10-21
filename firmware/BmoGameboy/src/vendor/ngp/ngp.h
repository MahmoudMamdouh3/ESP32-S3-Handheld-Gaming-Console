#ifndef NGP_H
#define NGP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // Toshiba TLCS-900/H 32-bit CPU State
  uint32_t pc;
  uint32_t r[8];              // XWA, XBC, XDE, XHL, XIX, XIY, XIZ, XSP
  uint16_t sr;                // Status Register (Flags in low byte)
  bool halted;

  // Memory & K1GE/K2GE Video State
  uint8_t* rom;
  uint32_t rom_size;
  uint8_t* ram;               // 32KB unified Work RAM + Video ASIC space ($0000-$7FFF + $8000-$BFFF)
  uint16_t palette_cache[256];// Pre-swapped BGR565 K2GE color palette cache
  uint16_t* framebuffer;      // 160 x 152 BGR565 in PSRAM
  uint8_t joypad;             // Bit 0: Up, 1: Down, 2: Left, 3: Right, 4: A, 5: B, 6: Option
  bool is_color;
} ngp_t;

void ngp_init(ngp_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb, bool is_color);
void ngp_step_frame(ngp_t* emu);
void ngp_set_joypad(ngp_t* emu, uint8_t pad);

#ifdef __cplusplus
}
#endif

#endif // NGP_H
