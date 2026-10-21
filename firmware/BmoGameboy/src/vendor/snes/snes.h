#ifndef SNES_H
#define SNES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // WDC 65C816 16-bit CPU Registers
  uint32_t pc;                // 16-bit PC
  uint16_t a, x, y, sp, dp;   // 16-bit Accumulator, Index X/Y, Stack, Direct Page
  uint8_t p, db, pb;          // Processor Status, Data Bank, Program Bank
  bool emu_mode;              // 6502 Emulation mode flag (E)
  bool stopped;

  // S-PPU & DMA State
  uint8_t ppu_regs[64];       // $2100 - $213F
  uint16_t vram_addr;         // VMADDL/H word address
  uint8_t vmain;              // VMAIN increment control
  uint8_t cgram_addr;         // CGADD palette index
  uint8_t cgram_latch;
  uint8_t cgram_lsb;
  uint16_t oam_addr;
  uint8_t dma_regs[128];      // 8 DMA channels ($4300-$437F)
  uint8_t nmitimen;           // $4200 NMI/IRQ enable
  uint8_t rdnmi;              // $4210 VBlank NMI flag
  uint16_t bg_hofs[4];
  uint16_t bg_vofs[4];
  uint8_t bg_scroll_prev;

  // Memory Buffers (100% Octal PSRAM)
  uint8_t* rom;
  uint32_t rom_size;
  uint32_t header_offset;     // 512 if SMC copier header present, else 0
  uint8_t* wram;              // 128KB Work RAM
  uint8_t* vram;              // 64KB Video RAM
  uint8_t* sram;              // 32KB Cartridge Save RAM
  uint8_t cgram[512];         // 256-color 15-bit BGR555 Palette RAM
  uint8_t oam[544];           // Object Attribute Memory (128 sprites)
  uint16_t palette_cache[256];// Pre-swapped BGR565 palette cache for ST7789
  uint16_t* framebuffer;      // 256 x 224 BGR565
  uint8_t pad_state;          // Bit 0: Up, 1: Down, 2: Left, 3: Right, 4: B, 5: A, 6: Start, 7: Select
} snes_t;

void snes_init(snes_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* wram, uint16_t* fb);
void snes_destroy(snes_t* emu);
void snes_step_frame(snes_t* emu);
void snes_set_pad(snes_t* emu, uint8_t pad);

#ifdef __cplusplus
}
#endif

#endif // SNES_H
