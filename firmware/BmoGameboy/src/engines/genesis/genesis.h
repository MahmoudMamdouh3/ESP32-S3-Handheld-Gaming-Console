#ifndef GENESIS_H
#define GENESIS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // 68000 CPU State
  uint32_t pc;
  uint32_t d[8];
  uint32_t a[8];
  uint32_t usp;       // User Stack Pointer
  uint32_t ssp;       // Supervisor Stack Pointer
  uint16_t sr;        // Status Register (T, S, I2-0, X, N, Z, V, C)
  bool stopped;

  // ROM & RAM
  uint8_t* rom;
  uint32_t rom_size;
  uint8_t* ram;        // 64KB Main 68000 Work RAM
  uint8_t* z80_ram;    // 8KB Z80 RAM
  bool owns_ram;       // true if ram was allocated internally

  // VDP State
  uint8_t* vram;       // 64KB Video RAM (32K words)
  uint16_t* cram;      // 64 words (128 bytes) Color RAM
  uint16_t* vsram;     // 40 words (80 bytes) Vertical Scroll RAM
  uint8_t vdp_regs[24];
  uint32_t vdp_addr;
  uint8_t vdp_code;
  uint8_t vdp_pending;
  uint16_t vdp_status;
  uint8_t vdp_dma_fill;

  // I/O & Controller
  uint8_t pad_state;   // D-Pad + Buttons
  uint8_t io_ctrl[3];  // I/O Control registers
  uint8_t io_data[3];  // I/O Data registers
  uint16_t z80_busreq;
  uint16_t z80_reset;

  // Video Output
  uint16_t* framebuffer; // 320x224 RGB565
  uint16_t scanline;
  uint32_t cycles;

  // Palette cache: 64 entries in BGR565 byte-swapped for direct ST7789 push
  uint16_t palette_cache[64];
} genesis_t;

void genesis_init(genesis_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb);
void genesis_destroy(genesis_t* emu);
void genesis_reset(genesis_t* emu);
void genesis_step_frame(genesis_t* emu);
void genesis_set_pad(genesis_t* emu, uint8_t pad);

#ifdef __cplusplus
}
#endif

#endif // GENESIS_H
