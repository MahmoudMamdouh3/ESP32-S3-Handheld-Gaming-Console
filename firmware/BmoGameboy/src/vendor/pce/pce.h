#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t ram[0x2000];       // 8KB Main RAM (Page $F8)
    uint8_t bram[0x800];       // 2KB Backup RAM (Page $F7)
    uint8_t vram[0x10000];     // 64KB VRAM (32K 16-bit words)
    uint16_t vce_palette[512]; // 512-color Palette (pre-swapped BGR565 for ST7789)
    uint16_t vce_raw[512];     // 512 9-bit RGB palette values
    uint8_t mpr[8];            // 8 Memory Mapping Registers (8KB pages)
    
    // CPU Registers
    uint16_t pc;
    uint8_t a, x, y, s, p;
    uint32_t cycles;           // Elapsed cycles this frame
    uint8_t irq_mask;          // IRQ mask ($1402)
    uint8_t irq_status;        // Pending IRQs ($1403)
    
    // Timer
    uint8_t timer_reload;      // Timer reload value ($0C00)
    uint8_t timer_counter;     // Timer countdown
    uint8_t timer_enabled;     // Timer enable flag ($0C01)
    
    // VDC Registers (HuC6270)
    uint8_t vdc_reg;           // Currently selected register (0..1F)
    uint16_t vdc_mawr;         // R0: VRAM write address
    uint16_t vdc_marr;         // R1: VRAM read address
    uint16_t vdc_cr;           // R5: Control Register
    uint16_t vdc_rcr;          // R6: Raster Compare Register
    uint16_t vdc_bxr;          // R7: Background X scroll
    uint16_t vdc_byr;          // R8: Background Y scroll
    uint16_t vdc_mwr;          // R9: Memory Width Register
    uint16_t vdc_satb;         // R13: SATB address
    uint16_t vdc_status;       // VDC status (VBlank, Raster, etc.)
    uint8_t vdc_latch;         // VDC write data LSB latch
    
    // VCE Registers (HuC6260)
    uint16_t vce_addr;         // VCE palette address (0..511)
    uint8_t vce_latch;         // VCE palette data LSB latch
    
    // Controller I/O
    uint8_t joypad;            // Active-low: 0=pressed
    uint8_t joypad_select;     // Joypad strobe select (0=buttons, 1=d-pad)
    uint8_t joypad_clr;        // Joypad clear
    
    // ROM reference
    const uint8_t* rom;
    size_t rom_size;
    
    uint16_t* framebuffer;     // 256x240 in PSRAM
} pce_context_t;

pce_context_t* pce_create(void);
void pce_destroy(pce_context_t* ctx);
bool pce_load_rom(pce_context_t* ctx, const uint8_t* rom, size_t size);
void pce_set_input(pce_context_t* ctx, uint8_t joypad);
void pce_run_frame(pce_context_t* ctx);

#ifdef __cplusplus
}
#endif
