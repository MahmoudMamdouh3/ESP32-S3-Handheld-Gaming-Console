#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t ram[128];          // 128 bytes RIOT RAM ($0080-$00FF)
    uint8_t tia_regs[64];      // TIA write registers ($00-$3F)
    uint16_t tia_palette[128]; // NTSC TIA palette (pre-swapped BGR565)

    // TIA internal horizontal positioning & motion state
    uint8_t pos_p0, pos_p1, pos_m0, pos_m1, pos_bl;
    bool wsync_halt;

    // MOS 6532 RIOT Timer state
    uint32_t riot_timer_clocks;
    uint16_t riot_interval;    // 1, 8, 64, or 1024
    uint8_t riot_intim;
    uint8_t riot_timint;
    uint8_t swcha;             // Joystick port A ($0280)
    uint8_t swchb;             // Console switches ($0282)

    // 6507 CPU Registers
    uint16_t pc;
    uint8_t a, x, y, s, p;
    uint32_t cycles;

    const uint8_t* rom;
    size_t rom_size;
    uint8_t rom_bank;          // F8/F6/F4 bankswitching support

    uint8_t joypad;            // Active-low: Bit 0: Up, 1: Down, 2: Left, 3: Right, 4: Fire
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
