#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t ram[0x2000];       // 8KB Main RAM
    uint8_t vram[0x10000];     // 64KB VRAM
    uint16_t vce_palette[512]; // 512-color Palette (pre-swapped BGR565)
    uint8_t mpr[8];            // 8 Memory Mapping Registers (8KB pages)
    
    // CPU Registers
    uint16_t pc;
    uint8_t a, x, y, s, p;
    
    const uint8_t* rom;
    size_t rom_size;
    
    uint8_t joypad;            // Active-low: 0=pressed
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
