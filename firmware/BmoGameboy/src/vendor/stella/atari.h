#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t ram[128];          // 128 bytes RIOT RAM
    uint8_t tia_regs[64];      // TIA registers
    uint16_t tia_palette[128]; // NTSC/PAL TIA palette (pre-swapped BGR565)
    
    // 6507 CPU Registers
    uint16_t pc;
    uint8_t a, x, y, s, p;
    
    const uint8_t* rom;
    size_t rom_size;
    
    uint8_t joypad;            // Bit 0: Up, 1: Down, 2: Left, 3: Right, 4: Fire
    uint16_t* framebuffer;     // 160x192 in PSRAM
} atari_context_t;

atari_context_t* atari_create(void);
void atari_destroy(atari_context_t* ctx);
bool atari_load_rom(atari_context_t* ctx, const uint8_t* rom, size_t size);
void atari_set_input(atari_context_t* ctx, uint8_t joypad);
void atari_run_frame(atari_context_t* ctx);

#ifdef __cplusplus
}
#endif
