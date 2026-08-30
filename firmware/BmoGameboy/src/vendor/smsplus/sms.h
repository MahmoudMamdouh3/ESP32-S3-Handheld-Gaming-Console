#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t vram[0x4000];      // 16KB VRAM
    uint8_t cram[64];          // 32-color Palette RAM (SMS 32, GG 64 bytes)
    uint8_t regs[16];          // VDP Registers
    uint8_t status;
    uint8_t latch;
    uint16_t addr;
    uint8_t read_buf;
    bool is_gamegear;
    bool vblank_irq;
} sms_vdp_t;

typedef struct {
    // Z80 Registers
    uint16_t pc, sp, af, bc, de, hl;
    uint16_t af_prime, bc_prime, de_prime, hl_prime;
    uint16_t ix, iy;
    uint8_t i, r;
    uint8_t iff1, iff2, im;
    bool halted;
    
    // System State
    const uint8_t* rom;
    size_t rom_size;
    uint8_t paging[4];         // Sega Mapper page slots (0x0000, 0x4000, 0x8000)
    uint8_t ram[0x2000];       // 8KB Main System RAM
    uint8_t cart_ram[0x8000];   // 32KB Battery/Cart RAM
    bool cart_ram_mapped;
    
    sms_vdp_t vdp;
    uint8_t joypad;            // Active-low: D-pad + 1 + 2
    uint8_t system_pad;        // Active-low: Pause / Start
    
    // Frame buffer (256x192 SMS or 160x144 GG in BGR565 pre-swapped)
    uint16_t* framebuffer;
} sms_context_t;

sms_context_t* sms_create(void);
void sms_destroy(sms_context_t* ctx);
bool sms_load_rom(sms_context_t* ctx, const uint8_t* rom, size_t size, bool is_gamegear);
void sms_set_input(sms_context_t* ctx, uint8_t joypad, uint8_t system_pad);
void sms_run_frame(sms_context_t* ctx);

#ifdef __cplusplus
}
#endif
