#pragma GCC optimize ("O3,unroll-loops")
// ---------------------------------------------------------------------------
// atari.c – Atari 2600 (VCS) MOS 6507 + TIA + MOS 6532 RIOT Core Engine
//
// Features:
//   - MOS 6507 8-bit CPU interpreter (1.19 MHz, 13-bit address bus $0000-$1FFF)
//   - Atari TIA scanline renderer (Playfield PF0/PF1/PF2 reflect/score, P0/P1,
//     M0/M1, Ball, HMOVE fine motion, WSYNC synchronization)
//   - MOS 6532 RIOT 128-byte zero-page RAM ($0080-$00FF) + interval timer
//     (TIM1T, TIM8T, TIM64T, T1024T, INTIM) + SWCHA/SWCHB I/O ports
//   - Standard 2K/4K ROM + F8 (8K) / F6 (16K) / F4 (32K) bankswitching hotspots
//   - 100% Octal PSRAM allocation, pre-swapped BGR565 output (160x192)
// ---------------------------------------------------------------------------

#include "atari.h"
#include <stdlib.h>
#include <string.h>

#if defined(ESP32) || defined(ARDUINO)
#include <esp_heap_caps.h>
#define ATARI_MALLOC(sz) heap_caps_malloc(sz, MALLOC_CAP_SPIRAM)
#define ATARI_FREE(p) heap_caps_free(p)
#else
#define ATARI_MALLOC(sz) malloc(sz)
#define ATARI_FREE(p) free(p)
#endif

// 6507 Status Flags
#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

static inline uint16_t atari_rgb_to_bgr565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t bgr = ((uint16_t)(b & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (r >> 3);
    return (uint16_t)(((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF));
}

static inline void atari_set_zn(atari_context_t* ctx, uint8_t val) {
    ctx->p &= ~(FLAG_Z | FLAG_N);
    if (val == 0) ctx->p |= FLAG_Z;
    if (val & 0x80) ctx->p |= FLAG_N;
}

static inline int8_t atari_hm_delta(uint8_t hm) {
    int8_t d = (int8_t)(hm & 0xF0) >> 4;
    return -d;
}

static void atari_check_bankswitch(atari_context_t* ctx, uint16_t addr13) {
    if (ctx->rom_size == 8192 && (addr13 == 0x1FF8 || addr13 == 0x1FF9)) {
        ctx->rom_bank = (uint8_t)(addr13 - 0x1FF8);
    } else if (ctx->rom_size == 16384 && addr13 >= 0x1FF6 && addr13 <= 0x1FF9) {
        ctx->rom_bank = (uint8_t)(addr13 - 0x1FF6);
    } else if (ctx->rom_size == 32768 && addr13 >= 0x1FF4 && addr13 <= 0x1FFB) {
        ctx->rom_bank = (uint8_t)(addr13 - 0x1FF4);
    }
}

static uint8_t atari_read8(atari_context_t* ctx, uint16_t addr) {
    uint16_t addr13 = addr & 0x1FFF;

    // Cartridge ROM ($1000 - $1FFF, A12 = 1)
    if (addr13 & 0x1000) {
        atari_check_bankswitch(ctx, addr13);
        if (!ctx->rom || ctx->rom_size == 0) return 0xEA; // NOP
        uint32_t offset = addr13 & 0x0FFF;
        if (ctx->rom_size <= 4096) {
            return ctx->rom[offset % ctx->rom_size];
        }
        uint32_t banked = ((uint32_t)ctx->rom_bank * 4096U) + offset;
        return ctx->rom[banked % ctx->rom_size];
    }

    // RAM ($0080 - $00FF, A12 = 0, A9 = 0, A7 = 1)
    if ((addr13 & 0x0280) == 0x0080) {
        return ctx->ram[addr13 & 0x7F];
    }

    // PIA / RIOT I/O & Timer ($0280 - $029F, A12 = 0, A9 = 1, A7 = 1)
    if ((addr13 & 0x0280) == 0x0280) {
        uint8_t reg = addr13 & 0x07;
        if (reg == 0x00) {
            // SWCHA: Player 0 in upper nibble (Bits 4=Up, 5=Down, 6=Left, 7=Right)
            uint8_t p0 = 0xF0;
            if (!(ctx->joypad & 0x01)) p0 &= ~0x10; // Up
            if (!(ctx->joypad & 0x02)) p0 &= ~0x20; // Down
            if (!(ctx->joypad & 0x04)) p0 &= ~0x40; // Left
            if (!(ctx->joypad & 0x08)) p0 &= ~0x80; // Right
            return p0 | 0x0F;
        } else if (reg == 0x02) {
            // SWCHB: Console switches (Bit 0=Reset, Bit 1=Select, Bit 3=Color/BW)
            return ctx->swchb;
        } else if (reg == 0x04 || reg == 0x06) {
            // INTIM: Current timer value
            ctx->riot_timint &= ~0x80;
            return ctx->riot_intim;
        } else if (reg == 0x05 || reg == 0x07) {
            // TIMINT: Timer interrupt flag
            return ctx->riot_timint;
        }
        return 0x00;
    }

    // TIA Read Registers ($0000 - $000D mirrored)
    uint8_t tia_r = addr13 & 0x0F;
    if (tia_r == 0x0C) {
        // INPT4: Player 0 Fire button (Bit 7: 0 = pressed, 1 = released)
        return (ctx->joypad & 0x10) ? 0x80 : 0x00;
    } else if (tia_r == 0x0D) {
        // INPT5: Player 1 Fire button
        return 0x80;
    }
    return 0x00;
}

static void atari_write8(atari_context_t* ctx, uint16_t addr, uint8_t val) {
    uint16_t addr13 = addr & 0x1FFF;

    if (addr13 & 0x1000) {
        atari_check_bankswitch(ctx, addr13);
        return;
    }

    // RIOT RAM ($0080 - $00FF)
    if ((addr13 & 0x0280) == 0x0080) {
        ctx->ram[addr13 & 0x7F] = val;
        return;
    }

    // RIOT I/O & Timer ($0280 - $029F)
    if ((addr13 & 0x0280) == 0x0280) {
        uint8_t sub = addr13 & 0x1F;
        if (sub == 0x14) { ctx->riot_intim = val; ctx->riot_interval = 1;    ctx->riot_timer_clocks = 0; ctx->riot_timint = 0; }
        else if (sub == 0x15) { ctx->riot_intim = val; ctx->riot_interval = 8;    ctx->riot_timer_clocks = 0; ctx->riot_timint = 0; }
        else if (sub == 0x16) { ctx->riot_intim = val; ctx->riot_interval = 64;   ctx->riot_timer_clocks = 0; ctx->riot_timint = 0; }
        else if (sub == 0x17) { ctx->riot_intim = val; ctx->riot_interval = 1024; ctx->riot_timer_clocks = 0; ctx->riot_timint = 0; }
        return;
    }

    // TIA Write Registers ($00 - $3F)
    uint8_t reg = addr13 & 0x3F;
    ctx->tia_regs[reg] = val;
    switch (reg) {
        case 0x02: // WSYNC: Halt CPU until end of current scanline
            ctx->wsync_halt = true;
            break;
        case 0x10: ctx->pos_p0 = 76; break; // RESP0
        case 0x11: ctx->pos_p1 = 76; break; // RESP1
        case 0x12: ctx->pos_m0 = 76; break; // RESM0
        case 0x13: ctx->pos_m1 = 76; break; // RESM1
        case 0x14: ctx->pos_bl = 76; break; // RESBL
        case 0x2A: // HMOVE: Apply horizontal motion vectors
            ctx->pos_p0 = (uint8_t)((ctx->pos_p0 + 160 + atari_hm_delta(ctx->tia_regs[0x20])) % 160);
            ctx->pos_p1 = (uint8_t)((ctx->pos_p1 + 160 + atari_hm_delta(ctx->tia_regs[0x21])) % 160);
            ctx->pos_m0 = (uint8_t)((ctx->pos_m0 + 160 + atari_hm_delta(ctx->tia_regs[0x22])) % 160);
            ctx->pos_m1 = (uint8_t)((ctx->pos_m1 + 160 + atari_hm_delta(ctx->tia_regs[0x23])) % 160);
            ctx->pos_bl = (uint8_t)((ctx->pos_bl + 160 + atari_hm_delta(ctx->tia_regs[0x24])) % 160);
            break;
        case 0x2B: // HMCLR: Clear all horizontal motion registers
            memset(&ctx->tia_regs[0x20], 0, 5);
            break;
        default:
            break;
    }
}

static inline uint16_t atari_read16(atari_context_t* ctx, uint16_t addr) {
    return (uint16_t)atari_read8(ctx, addr) | ((uint16_t)atari_read8(ctx, (uint16_t)(addr + 1)) << 8);
}

static inline void atari_push8(atari_context_t* ctx, uint8_t val) {
    atari_write8(ctx, (uint16_t)(0x0100 | ctx->s), val);
    ctx->s--;
}

static inline uint8_t atari_pop8(atari_context_t* ctx) {
    ctx->s++;
    return atari_read8(ctx, (uint16_t)(0x0100 | ctx->s));
}

static void atari_adc(atari_context_t* ctx, uint8_t val) {
    uint16_t carry = (ctx->p & FLAG_C) ? 1 : 0;
    uint16_t sum = (uint16_t)ctx->a + val + carry;
    ctx->p &= ~(FLAG_C | FLAG_V | FLAG_Z | FLAG_N);
    if (~(ctx->a ^ val) & (ctx->a ^ sum) & 0x80) ctx->p |= FLAG_V;
    if (sum > 0xFF) ctx->p |= FLAG_C;
    ctx->a = (uint8_t)sum;
    atari_set_zn(ctx, ctx->a);
}

static void atari_sbc(atari_context_t* ctx, uint8_t val) {
    atari_adc(ctx, (uint8_t)(~val));
}

static void atari_cmp(atari_context_t* ctx, uint8_t reg, uint8_t val) {
    uint16_t diff = (uint16_t)reg - val;
    ctx->p &= ~(FLAG_C | FLAG_Z | FLAG_N);
    if (reg >= val) ctx->p |= FLAG_C;
    atari_set_zn(ctx, (uint8_t)diff);
}

static void atari_step_riot(atari_context_t* ctx, int cycles) {
    for (int i = 0; i < cycles; i++) {
        ctx->riot_timer_clocks++;
        if (ctx->riot_timer_clocks >= ctx->riot_interval) {
            ctx->riot_timer_clocks = 0;
            if (ctx->riot_intim == 0) {
                ctx->riot_intim = 0xFF;
                ctx->riot_timint |= 0x80;
                ctx->riot_interval = 1; // After underflow, decrements every 1 cycle
            } else {
                ctx->riot_intim--;
            }
        }
    }
}

static int atari_cpu_step(atari_context_t* ctx) {
    uint8_t op = atari_read8(ctx, ctx->pc++);
    int cyc = 2;

    switch (op) {
        case 0x00: { // BRK
            ctx->pc++;
            atari_push8(ctx, (uint8_t)(ctx->pc >> 8));
            atari_push8(ctx, (uint8_t)(ctx->pc & 0xFF));
            atari_push8(ctx, (uint8_t)(ctx->p | FLAG_B | FLAG_U));
            ctx->p |= FLAG_I;
            ctx->pc = atari_read16(ctx, 0xFFFE);
            cyc = 7;
            break;
        }
        case 0xEA: cyc = 2; break; // NOP

        // Load / Store A
        case 0xA9: ctx->a = atari_read8(ctx, ctx->pc++); atari_set_zn(ctx, ctx->a); cyc = 2; break;
        case 0xA5: ctx->a = atari_read8(ctx, atari_read8(ctx, ctx->pc++)); atari_set_zn(ctx, ctx->a); cyc = 3; break;
        case 0xB5: ctx->a = atari_read8(ctx, (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x)); atari_set_zn(ctx, ctx->a); cyc = 4; break;
        case 0xAD: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; ctx->a = atari_read8(ctx, a); atari_set_zn(ctx, ctx->a); cyc = 4; break; }
        case 0xBD: { uint16_t a = atari_read16(ctx, ctx->pc) + ctx->x; ctx->pc += 2; ctx->a = atari_read8(ctx, a); atari_set_zn(ctx, ctx->a); cyc = 4; break; }
        case 0xB9: { uint16_t a = atari_read16(ctx, ctx->pc) + ctx->y; ctx->pc += 2; ctx->a = atari_read8(ctx, a); atari_set_zn(ctx, ctx->a); cyc = 4; break; }
        case 0xA1: { uint8_t zp = (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x); uint16_t a = atari_read8(ctx, zp) | ((uint16_t)atari_read8(ctx, (uint8_t)(zp + 1)) << 8); ctx->a = atari_read8(ctx, a); atari_set_zn(ctx, ctx->a); cyc = 6; break; }
        case 0xB1: { uint8_t zp = atari_read8(ctx, ctx->pc++); uint16_t a = (atari_read8(ctx, zp) | ((uint16_t)atari_read8(ctx, (uint8_t)(zp + 1)) << 8)) + ctx->y; ctx->a = atari_read8(ctx, a); atari_set_zn(ctx, ctx->a); cyc = 5; break; }

        case 0x85: atari_write8(ctx, atari_read8(ctx, ctx->pc++), ctx->a); cyc = 3; break;
        case 0x95: atari_write8(ctx, (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x), ctx->a); cyc = 4; break;
        case 0x8D: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; atari_write8(ctx, a, ctx->a); cyc = 4; break; }
        case 0x9D: { uint16_t a = atari_read16(ctx, ctx->pc) + ctx->x; ctx->pc += 2; atari_write8(ctx, a, ctx->a); cyc = 5; break; }
        case 0x99: { uint16_t a = atari_read16(ctx, ctx->pc) + ctx->y; ctx->pc += 2; atari_write8(ctx, a, ctx->a); cyc = 5; break; }
        case 0x81: { uint8_t zp = (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x); uint16_t a = atari_read8(ctx, zp) | ((uint16_t)atari_read8(ctx, (uint8_t)(zp + 1)) << 8); atari_write8(ctx, a, ctx->a); cyc = 6; break; }
        case 0x91: { uint8_t zp = atari_read8(ctx, ctx->pc++); uint16_t a = (atari_read8(ctx, zp) | ((uint16_t)atari_read8(ctx, (uint8_t)(zp + 1)) << 8)) + ctx->y; atari_write8(ctx, a, ctx->a); cyc = 6; break; }

        // Load / Store X & Y
        case 0xA2: ctx->x = atari_read8(ctx, ctx->pc++); atari_set_zn(ctx, ctx->x); cyc = 2; break;
        case 0xA6: ctx->x = atari_read8(ctx, atari_read8(ctx, ctx->pc++)); atari_set_zn(ctx, ctx->x); cyc = 3; break;
        case 0xB6: ctx->x = atari_read8(ctx, (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->y)); atari_set_zn(ctx, ctx->x); cyc = 4; break;
        case 0xAE: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; ctx->x = atari_read8(ctx, a); atari_set_zn(ctx, ctx->x); cyc = 4; break; }
        case 0xBE: { uint16_t a = atari_read16(ctx, ctx->pc) + ctx->y; ctx->pc += 2; ctx->x = atari_read8(ctx, a); atari_set_zn(ctx, ctx->x); cyc = 4; break; }
        case 0x86: atari_write8(ctx, atari_read8(ctx, ctx->pc++), ctx->x); cyc = 3; break;
        case 0x96: atari_write8(ctx, (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->y), ctx->x); cyc = 4; break;
        case 0x8E: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; atari_write8(ctx, a, ctx->x); cyc = 4; break; }

        case 0xA0: ctx->y = atari_read8(ctx, ctx->pc++); atari_set_zn(ctx, ctx->y); cyc = 2; break;
        case 0xA4: ctx->y = atari_read8(ctx, atari_read8(ctx, ctx->pc++)); atari_set_zn(ctx, ctx->y); cyc = 3; break;
        case 0xB4: ctx->y = atari_read8(ctx, (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x)); atari_set_zn(ctx, ctx->y); cyc = 4; break;
        case 0xAC: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; ctx->y = atari_read8(ctx, a); atari_set_zn(ctx, ctx->y); cyc = 4; break; }
        case 0xBC: { uint16_t a = atari_read16(ctx, ctx->pc) + ctx->x; ctx->pc += 2; ctx->y = atari_read8(ctx, a); atari_set_zn(ctx, ctx->y); cyc = 4; break; }
        case 0x84: atari_write8(ctx, atari_read8(ctx, ctx->pc++), ctx->y); cyc = 3; break;
        case 0x94: atari_write8(ctx, (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x), ctx->y); cyc = 4; break;
        case 0x8C: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; atari_write8(ctx, a, ctx->y); cyc = 4; break; }

        // Transfers & Stack
        case 0xAA: ctx->x = ctx->a; atari_set_zn(ctx, ctx->x); break; // TAX
        case 0x8A: ctx->a = ctx->x; atari_set_zn(ctx, ctx->a); break; // TXA
        case 0xA8: ctx->y = ctx->a; atari_set_zn(ctx, ctx->y); break; // TAY
        case 0x98: ctx->a = ctx->y; atari_set_zn(ctx, ctx->a); break; // TYA
        case 0xBA: ctx->x = ctx->s; atari_set_zn(ctx, ctx->x); break; // TSX
        case 0x9A: ctx->s = ctx->x; break;                            // TXS
        case 0x48: atari_push8(ctx, ctx->a); cyc = 3; break;          // PHA
        case 0x68: ctx->a = atari_pop8(ctx); atari_set_zn(ctx, ctx->a); cyc = 4; break; // PLA
        case 0x08: atari_push8(ctx, (uint8_t)(ctx->p | FLAG_B | FLAG_U)); cyc = 3; break; // PHP
        case 0x28: ctx->p = (uint8_t)((atari_pop8(ctx) & ~FLAG_B) | FLAG_U); cyc = 4; break; // PLP

        // ALU: AND, ORA, EOR, BIT, ADC, SBC, CMP, CPX, CPY
        case 0x29: ctx->a &= atari_read8(ctx, ctx->pc++); atari_set_zn(ctx, ctx->a); break;
        case 0x25: ctx->a &= atari_read8(ctx, atari_read8(ctx, ctx->pc++)); atari_set_zn(ctx, ctx->a); cyc = 3; break;
        case 0x35: ctx->a &= atari_read8(ctx, (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x)); atari_set_zn(ctx, ctx->a); cyc = 4; break;
        case 0x2D: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; ctx->a &= atari_read8(ctx, a); atari_set_zn(ctx, ctx->a); cyc = 4; break; }
        case 0x09: ctx->a |= atari_read8(ctx, ctx->pc++); atari_set_zn(ctx, ctx->a); break;
        case 0x05: ctx->a |= atari_read8(ctx, atari_read8(ctx, ctx->pc++)); atari_set_zn(ctx, ctx->a); cyc = 3; break;
        case 0x15: ctx->a |= atari_read8(ctx, (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x)); atari_set_zn(ctx, ctx->a); cyc = 4; break;
        case 0x0D: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; ctx->a |= atari_read8(ctx, a); atari_set_zn(ctx, ctx->a); cyc = 4; break; }
        case 0x49: ctx->a ^= atari_read8(ctx, ctx->pc++); atari_set_zn(ctx, ctx->a); break;
        case 0x45: ctx->a ^= atari_read8(ctx, atari_read8(ctx, ctx->pc++)); atari_set_zn(ctx, ctx->a); cyc = 3; break;
        case 0x55: ctx->a ^= atari_read8(ctx, (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x)); atari_set_zn(ctx, ctx->a); cyc = 4; break;
        case 0x4D: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; ctx->a ^= atari_read8(ctx, a); atari_set_zn(ctx, ctx->a); cyc = 4; break; }
        case 0x24: { uint8_t m = atari_read8(ctx, atari_read8(ctx, ctx->pc++)); ctx->p = (ctx->p & ~(FLAG_N | FLAG_V | FLAG_Z)) | (m & (FLAG_N | FLAG_V)) | ((ctx->a & m) == 0 ? FLAG_Z : 0); cyc = 3; break; }
        case 0x2C: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; uint8_t m = atari_read8(ctx, a); ctx->p = (ctx->p & ~(FLAG_N | FLAG_V | FLAG_Z)) | (m & (FLAG_N | FLAG_V)) | ((ctx->a & m) == 0 ? FLAG_Z : 0); cyc = 4; break; }

        case 0x69: atari_adc(ctx, atari_read8(ctx, ctx->pc++)); break;
        case 0x65: atari_adc(ctx, atari_read8(ctx, atari_read8(ctx, ctx->pc++))); cyc = 3; break;
        case 0x75: atari_adc(ctx, atari_read8(ctx, (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x))); cyc = 4; break;
        case 0x6D: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; atari_adc(ctx, atari_read8(ctx, a)); cyc = 4; break; }
        case 0xE9: atari_sbc(ctx, atari_read8(ctx, ctx->pc++)); break;
        case 0xE5: atari_sbc(ctx, atari_read8(ctx, atari_read8(ctx, ctx->pc++))); cyc = 3; break;
        case 0xF5: atari_sbc(ctx, atari_read8(ctx, (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x))); cyc = 4; break;
        case 0xED: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; atari_sbc(ctx, atari_read8(ctx, a)); cyc = 4; break; }

        case 0xC9: atari_cmp(ctx, ctx->a, atari_read8(ctx, ctx->pc++)); break;
        case 0xC5: atari_cmp(ctx, ctx->a, atari_read8(ctx, atari_read8(ctx, ctx->pc++))); cyc = 3; break;
        case 0xD5: atari_cmp(ctx, ctx->a, atari_read8(ctx, (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x))); cyc = 4; break;
        case 0xCD: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; atari_cmp(ctx, ctx->a, atari_read8(ctx, a)); cyc = 4; break; }
        case 0xE0: atari_cmp(ctx, ctx->x, atari_read8(ctx, ctx->pc++)); break;
        case 0xE4: atari_cmp(ctx, ctx->x, atari_read8(ctx, atari_read8(ctx, ctx->pc++))); cyc = 3; break;
        case 0xEC: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; atari_cmp(ctx, ctx->x, atari_read8(ctx, a)); cyc = 4; break; }
        case 0xC0: atari_cmp(ctx, ctx->y, atari_read8(ctx, ctx->pc++)); break;
        case 0xC4: atari_cmp(ctx, ctx->y, atari_read8(ctx, atari_read8(ctx, ctx->pc++))); cyc = 3; break;
        case 0xCC: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; atari_cmp(ctx, ctx->y, atari_read8(ctx, a)); cyc = 4; break; }

        // Increment / Decrement
        case 0xE8: ctx->x++; atari_set_zn(ctx, ctx->x); break; // INX
        case 0xCA: ctx->x--; atari_set_zn(ctx, ctx->x); break; // DEX
        case 0xC8: ctx->y++; atari_set_zn(ctx, ctx->y); break; // INY
        case 0x88: ctx->y--; atari_set_zn(ctx, ctx->y); break; // DEY
        case 0xE6: { uint8_t zp = atari_read8(ctx, ctx->pc++); uint8_t v = atari_read8(ctx, zp) + 1; atari_write8(ctx, zp, v); atari_set_zn(ctx, v); cyc = 5; break; }
        case 0xF6: { uint8_t zp = (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x); uint8_t v = atari_read8(ctx, zp) + 1; atari_write8(ctx, zp, v); atari_set_zn(ctx, v); cyc = 6; break; }
        case 0xEE: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; uint8_t v = atari_read8(ctx, a) + 1; atari_write8(ctx, a, v); atari_set_zn(ctx, v); cyc = 6; break; }
        case 0xC6: { uint8_t zp = atari_read8(ctx, ctx->pc++); uint8_t v = atari_read8(ctx, zp) - 1; atari_write8(ctx, zp, v); atari_set_zn(ctx, v); cyc = 5; break; }
        case 0xD6: { uint8_t zp = (uint8_t)(atari_read8(ctx, ctx->pc++) + ctx->x); uint8_t v = atari_read8(ctx, zp) - 1; atari_write8(ctx, zp, v); atari_set_zn(ctx, v); cyc = 6; break; }
        case 0xCE: { uint16_t a = atari_read16(ctx, ctx->pc); ctx->pc += 2; uint8_t v = atari_read8(ctx, a) - 1; atari_write8(ctx, a, v); atari_set_zn(ctx, v); cyc = 6; break; }

        // Shifts & Rotates
        case 0x0A: ctx->p = (ctx->p & ~FLAG_C) | (ctx->a >> 7); ctx->a <<= 1; atari_set_zn(ctx, ctx->a); break; // ASL A
        case 0x4A: ctx->p = (ctx->p & ~FLAG_C) | (ctx->a & 1); ctx->a >>= 1; atari_set_zn(ctx, ctx->a); break; // LSR A
        case 0x2A: { uint8_t c = ctx->p & FLAG_C; ctx->p = (ctx->p & ~FLAG_C) | (ctx->a >> 7); ctx->a = (uint8_t)((ctx->a << 1) | c); atari_set_zn(ctx, ctx->a); break; } // ROL A
        case 0x6A: { uint8_t c = (ctx->p & FLAG_C) << 7; ctx->p = (ctx->p & ~FLAG_C) | (ctx->a & 1); ctx->a = (uint8_t)((ctx->a >> 1) | c); atari_set_zn(ctx, ctx->a); break; } // ROR A

        // Branches
        case 0x90: { int8_t d = (int8_t)atari_read8(ctx, ctx->pc++); if (!(ctx->p & FLAG_C)) { ctx->pc = (uint16_t)(ctx->pc + d); cyc = 3; } break; } // BCC
        case 0xB0: { int8_t d = (int8_t)atari_read8(ctx, ctx->pc++); if (ctx->p & FLAG_C)  { ctx->pc = (uint16_t)(ctx->pc + d); cyc = 3; } break; } // BCS
        case 0xF0: { int8_t d = (int8_t)atari_read8(ctx, ctx->pc++); if (ctx->p & FLAG_Z)  { ctx->pc = (uint16_t)(ctx->pc + d); cyc = 3; } break; } // BEQ
        case 0xD0: { int8_t d = (int8_t)atari_read8(ctx, ctx->pc++); if (!(ctx->p & FLAG_Z)) { ctx->pc = (uint16_t)(ctx->pc + d); cyc = 3; } break; } // BNE
        case 0x30: { int8_t d = (int8_t)atari_read8(ctx, ctx->pc++); if (ctx->p & FLAG_N)  { ctx->pc = (uint16_t)(ctx->pc + d); cyc = 3; } break; } // BMI
        case 0x10: { int8_t d = (int8_t)atari_read8(ctx, ctx->pc++); if (!(ctx->p & FLAG_N)) { ctx->pc = (uint16_t)(ctx->pc + d); cyc = 3; } break; } // BPL
        case 0x50: { int8_t d = (int8_t)atari_read8(ctx, ctx->pc++); if (!(ctx->p & FLAG_V)) { ctx->pc = (uint16_t)(ctx->pc + d); cyc = 3; } break; } // BVC
        case 0x70: { int8_t d = (int8_t)atari_read8(ctx, ctx->pc++); if (ctx->p & FLAG_V)  { ctx->pc = (uint16_t)(ctx->pc + d); cyc = 3; } break; } // BVS

        // Jumps & Subroutines
        case 0x4C: ctx->pc = atari_read16(ctx, ctx->pc); cyc = 3; break; // JMP abs
        case 0x6C: { uint16_t ptr = atari_read16(ctx, ctx->pc); ctx->pc = atari_read16(ctx, ptr); cyc = 5; break; } // JMP (ind)
        case 0x20: { uint16_t dst = atari_read16(ctx, ctx->pc); uint16_t ret = (uint16_t)(ctx->pc + 1); atari_push8(ctx, (uint8_t)(ret >> 8)); atari_push8(ctx, (uint8_t)(ret & 0xFF)); ctx->pc = dst; cyc = 6; break; } // JSR
        case 0x60: { uint16_t lo = atari_pop8(ctx); uint16_t hi = atari_pop8(ctx); ctx->pc = (uint16_t)(((hi << 8) | lo) + 1); cyc = 6; break; } // RTS
        case 0x40: { ctx->p = (uint8_t)((atari_pop8(ctx) & ~FLAG_B) | FLAG_U); uint16_t lo = atari_pop8(ctx); uint16_t hi = atari_pop8(ctx); ctx->pc = (uint16_t)((hi << 8) | lo); cyc = 6; break; } // RTI

        // Flags
        case 0x18: ctx->p &= ~FLAG_C; break; // CLC
        case 0x38: ctx->p |= FLAG_C;  break; // SEC
        case 0x58: ctx->p &= ~FLAG_I; break; // CLI
        case 0x78: ctx->p |= FLAG_I;  break; // SEI
        case 0xB8: ctx->p &= ~FLAG_V; break; // CLV
        case 0xD8: ctx->p &= ~FLAG_D; break; // CLD
        case 0xF8: ctx->p |= FLAG_D;  break; // SED
        default: break;
    }

    ctx->cycles += (uint32_t)cyc;
    atari_step_riot(ctx, cyc);
    return cyc;
}

static bool atari_get_playfield_bit(const atari_context_t* ctx, int pf_col) {
    // Playfield is 40 columns wide (4 pixels per column = 160 pixels)
    // Left half is 20 columns: PF0[4..7] (cols 0..3), PF1[7..0] (cols 4..11), PF2[0..7] (cols 12..19)
    int idx = pf_col;
    if (idx >= 20) {
        bool reflect = (ctx->tia_regs[0x0A] & 0x01) != 0;
        idx = reflect ? (39 - idx) : (idx - 20);
    }
    if (idx < 4) {
        return ((ctx->tia_regs[0x0D] >> (4 + idx)) & 1) != 0;
    } else if (idx < 12) {
        return ((ctx->tia_regs[0x0E] >> (7 - (idx - 4))) & 1) != 0;
    } else {
        return ((ctx->tia_regs[0x0F] >> (idx - 12)) & 1) != 0;
    }
}

static void atari_render_scanline(atari_context_t* ctx, int y) {
    if (y < 0 || y >= 192 || !ctx->framebuffer) return;
    uint16_t* line = &ctx->framebuffer[y * 160];

    // VBLANK bit 1 blanks the video output
    if (ctx->tia_regs[0x01] & 0x02) {
        uint16_t blk = ctx->tia_palette[0];
        for (int x = 0; x < 160; x++) line[x] = blk;
        return;
    }

    uint16_t col_bk = ctx->tia_palette[(ctx->tia_regs[0x09] >> 1) & 0x7F];
    uint16_t col_pf = ctx->tia_palette[(ctx->tia_regs[0x08] >> 1) & 0x7F];
    uint16_t col_p0 = ctx->tia_palette[(ctx->tia_regs[0x06] >> 1) & 0x7F];
    uint16_t col_p1 = ctx->tia_palette[(ctx->tia_regs[0x07] >> 1) & 0x7F];
    bool score_mode = (ctx->tia_regs[0x0A] & 0x02) != 0;
    bool pf_priority = (ctx->tia_regs[0x0A] & 0x04) != 0;

    uint8_t grp0 = ctx->tia_regs[0x1B];
    uint8_t grp1 = ctx->tia_regs[0x1C];
    bool refp0 = (ctx->tia_regs[0x0B] & 0x08) != 0;
    bool refp1 = (ctx->tia_regs[0x0C] & 0x08) != 0;
    bool enam0 = (ctx->tia_regs[0x1D] & 0x02) != 0;
    bool enam1 = (ctx->tia_regs[0x1E] & 0x02) != 0;
    bool enabl = (ctx->tia_regs[0x1F] & 0x02) != 0;

    for (int x = 0; x < 160; x++) {
        uint16_t pixel = col_bk;

        // Playfield & Ball
        int pf_col = x >> 2;
        bool pf_on = atari_get_playfield_bit(ctx, pf_col);
        bool bl_on = enabl && (x >= ctx->pos_bl && x < ctx->pos_bl + 2);
        uint16_t pf_color = score_mode ? ((pf_col < 20) ? col_p0 : col_p1) : col_pf;

        if (!pf_priority && (pf_on || bl_on)) {
            pixel = pf_color;
        }

        // Player 1 & Missile 1
        if (grp1 && x >= ctx->pos_p1 && x < ctx->pos_p1 + 8) {
            int bit = refp1 ? (x - ctx->pos_p1) : (7 - (x - ctx->pos_p1));
            if ((grp1 >> bit) & 1) pixel = col_p1;
        }
        if (enam1 && x >= ctx->pos_m1 && x < ctx->pos_m1 + 2) {
            pixel = col_p1;
        }

        // Player 0 & Missile 0
        if (grp0 && x >= ctx->pos_p0 && x < ctx->pos_p0 + 8) {
            int bit = refp0 ? (x - ctx->pos_p0) : (7 - (x - ctx->pos_p0));
            if ((grp0 >> bit) & 1) pixel = col_p0;
        }
        if (enam0 && x >= ctx->pos_m0 && x < ctx->pos_m0 + 2) {
            pixel = col_p0;
        }

        if (pf_priority && (pf_on || bl_on)) {
            pixel = pf_color;
        }

        line[x] = pixel;
    }
}

atari_context_t* atari_create(void) {
    atari_context_t* ctx = (atari_context_t*)ATARI_MALLOC(sizeof(atari_context_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(atari_context_t));

    ctx->framebuffer = (uint16_t*)ATARI_MALLOC(160 * 192 * sizeof(uint16_t));
    if (!ctx->framebuffer) {
        ATARI_FREE(ctx);
        return NULL;
    }

    // Initialize standard NTSC TIA 128-color palette
    for (int i = 0; i < 128; i++) {
        uint8_t lum = (uint8_t)((i & 0x07) * 36);
        uint8_t hue = (uint8_t)((i >> 3) & 0x0F);
        uint8_t r = lum, g = lum, b = lum;
        if (hue == 1)      { r = lum; g = lum; b = 0; }
        else if (hue == 2) { r = lum; g = (uint8_t)(lum * 2 / 3); b = 0; }
        else if (hue == 3) { r = lum; g = (uint8_t)(lum / 3); b = 0; }
        else if (hue == 4) { r = lum; g = 0; b = 0; }
        else if (hue == 5) { r = lum; g = 0; b = (uint8_t)(lum / 2); }
        else if (hue == 6) { r = (uint8_t)(lum * 2 / 3); g = 0; b = lum; }
        else if (hue == 7) { r = (uint8_t)(lum / 3); g = 0; b = lum; }
        else if (hue == 8) { r = 0; g = 0; b = lum; }
        else if (hue == 9) { r = 0; g = (uint8_t)(lum / 2); b = lum; }
        else if (hue == 10){ r = 0; g = lum; b = lum; }
        else if (hue == 11){ r = 0; g = lum; b = (uint8_t)(lum / 2); }
        else if (hue == 12){ r = 0; g = lum; b = 0; }
        else if (hue == 13){ r = (uint8_t)(lum / 2); g = lum; b = 0; }
        else if (hue == 14){ r = lum; g = lum; b = (uint8_t)(lum / 3); }
        else if (hue == 15){ r = lum; g = (uint8_t)(lum * 3 / 4); b = (uint8_t)(lum / 2); }
        ctx->tia_palette[i] = atari_rgb_to_bgr565(r, g, b);
    }
    return ctx;
}

void atari_destroy(atari_context_t* ctx) {
    if (ctx) {
        if (ctx->framebuffer) ATARI_FREE(ctx->framebuffer);
        ATARI_FREE(ctx);
    }
}

bool atari_load_rom(atari_context_t* ctx, const uint8_t* rom, size_t size) {
    if (!ctx || !rom || size == 0) return false;
    ctx->rom = rom;
    ctx->rom_size = size;
    ctx->rom_bank = 0;

    memset(ctx->ram, 0, sizeof(ctx->ram));
    memset(ctx->tia_regs, 0, sizeof(ctx->tia_regs));
    ctx->pos_p0 = 40;
    ctx->pos_p1 = 100;
    ctx->pos_m0 = 40;
    ctx->pos_m1 = 100;
    ctx->pos_bl = 80;
    ctx->riot_interval = 1024;
    ctx->riot_intim = 0xFF;
    ctx->swcha = 0xFF;
    ctx->swchb = 0x0B; // Color mode, Select & Reset unpressed (active low)

    // 6507 Reset vector at $FFFC-$FFFD (offset 0xFFC in bank 0 or last bank)
    ctx->s = 0xFD;
    ctx->p = FLAG_U | FLAG_I;
    ctx->a = ctx->x = ctx->y = 0;
    ctx->joypad = 0xFF;
    ctx->pc = atari_read16(ctx, 0xFFFC);

    return true;
}

void atari_set_input(atari_context_t* ctx, uint8_t joypad) {
    if (ctx) {
        ctx->joypad = joypad;
    }
}

void atari_run_frame(atari_context_t* ctx) {
    if (!ctx || !ctx->framebuffer) return;

    // 262 NTSC scanlines per frame (76 CPU cycles per scanline = 228 TIA color clocks)
    // Scanlines 0..39: VSync + VBlank, 40..231: 192 Visible lines, 232..261: Overscan
    for (int line = 0; line < 262; line++) {
        ctx->wsync_halt = false;
        int line_cycles = 0;
        while (line_cycles < 76) {
            if (ctx->wsync_halt) {
                int rem = 76 - line_cycles;
                ctx->cycles += (uint32_t)rem;
                atari_step_riot(ctx, rem);
                break;
            }
            line_cycles += atari_cpu_step(ctx);
        }

        if (line >= 40 && line < 232) {
            atari_render_scanline(ctx, line - 40);
        }
    }
}
