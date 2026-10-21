#pragma GCC optimize ("O3,unroll-loops")
// ---------------------------------------------------------------------------
// lynx.c – Atari Lynx WDC 65SC02 + Mikey & Suzy Hardware Core Engine
//
// Features:
//   - WDC 65SC02 8-bit CMOS CPU interpreter (4.0 MHz, 64KB unified RAM)
//   - CMOS 65SC02 extensions: BRA, STZ, INC A, DEC A, PHX/PLX/PHY/PLY, (zp)
//   - Mikey ($FD00-$FDFF): 16-color 12-bit RGB444 palette ($FDA0-$FDBF),
//     DISPADRL/H ($FD94-$FD95) 4bpp video DMA scanout (160x102, 80 bytes/row)
//   - Suzy ($FC00-$FCFF): Hardware 16x16 multiply / 32/16 divide, JOYSW
//     ($FCB0) controller input, and SCB sprite blitter ($FC91 SPRGO)
// ---------------------------------------------------------------------------

#include "lynx.h"
#include <string.h>

#define LYNX_C 0x01
#define LYNX_Z 0x02
#define LYNX_I 0x04
#define LYNX_D 0x08
#define LYNX_B 0x10
#define LYNX_U 0x20
#define LYNX_V 0x40
#define LYNX_N 0x80

static inline uint16_t lynx_rgb444_to_bgr565(uint8_t r4, uint8_t g4, uint8_t b4) {
    uint8_t r = (uint8_t)(r4 * 17);
    uint8_t g = (uint8_t)(g4 * 17);
    uint8_t b = (uint8_t)(b4 * 17);
    uint16_t bgr = ((uint16_t)(b & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (r >> 3);
    return (uint16_t)(((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF));
}

static inline void lynx_set_zn(lynx_t* emu, uint8_t val) {
    emu->p &= ~(LYNX_Z | LYNX_N);
    if (val == 0) emu->p |= LYNX_Z;
    if (val & 0x80) emu->p |= LYNX_N;
}

static void lynx_update_palette(lynx_t* emu, uint8_t idx) {
    idx &= 0x0F;
    uint8_t g4 = emu->mikey[0xA0 + idx] & 0x0F;
    uint8_t br = emu->mikey[0xB0 + idx];
    uint8_t b4 = (br >> 4) & 0x0F;
    uint8_t r4 = br & 0x0F;
    emu->palette_cache[idx] = lynx_rgb444_to_bgr565(r4, g4, b4);
}

// Execute Suzy Sprite Control Block (SCB) blitter when SPRGO ($FC91) bit 0 is written
static void lynx_execute_scb(lynx_t* emu) {
    if (!emu->ram) return;
    uint16_t scb = (uint16_t)emu->suzy[0x10] | ((uint16_t)emu->suzy[0x11] << 8);
    uint16_t vid_base = (uint16_t)emu->suzy[0x08] | ((uint16_t)emu->suzy[0x09] << 8);
    if (vid_base == 0) vid_base = emu->disp_addr;

    int safety = 0;
    while ((scb & 0xFF00) != 0 && safety++ < 64) {
        uint8_t sprctl0 = emu->ram[scb];
        uint8_t sprctl1 = emu->ram[(uint16_t)(scb + 1)];
        uint16_t next_scb = (uint16_t)emu->ram[(uint16_t)(scb + 3)] | ((uint16_t)emu->ram[(uint16_t)(scb + 4)] << 8);

        if (!(sprctl1 & 0x04)) { // Skip sprite bit not set
            uint16_t sprdline = (uint16_t)emu->ram[(uint16_t)(scb + 5)] | ((uint16_t)emu->ram[(uint16_t)(scb + 6)] << 8);
            int16_t hpos = (int16_t)((uint16_t)emu->ram[(uint16_t)(scb + 7)] | ((uint16_t)emu->ram[(uint16_t)(scb + 8)] << 8));
            int16_t vpos = (int16_t)((uint16_t)emu->ram[(uint16_t)(scb + 9)] | ((uint16_t)emu->ram[(uint16_t)(scb + 10)] << 8));
            uint8_t w = emu->ram[sprdline];
            uint8_t h = emu->ram[(uint16_t)(sprdline + 1)];
            uint16_t data_ptr = (uint16_t)(sprdline + 2);
            if (w > 0 && w <= 80 && h > 0 && h <= 102) {
                for (int ry = 0; ry < h; ry++) {
                    int py = vpos + ry;
                    if (py < 0 || py >= 102) continue;
                    for (int rx = 0; rx < w; rx++) {
                        int px = hpos + rx;
                        if (px < 0 || px >= 160) continue;
                        uint8_t c = emu->ram[(uint16_t)(data_ptr + ry * w + rx)] & 0x0F;
                        if (c == 0 && (sprctl0 & 0x07) != 4) continue;
                        uint16_t dst_addr = (uint16_t)(vid_base + py * 80 + (px >> 1));
                        if (px & 1) {
                            emu->ram[dst_addr] = (emu->ram[dst_addr] & 0xF0) | c;
                        } else {
                            emu->ram[dst_addr] = (emu->ram[dst_addr] & 0x0F) | (uint8_t)(c << 4);
                        }
                    }
                }
            }
        }
        scb = next_scb;
    }
    emu->suzy[0x92] &= ~0x01; // Clear SPRSYS busy flag
}

static uint8_t lynx_read8(lynx_t* emu, uint16_t addr) {
    if (!emu->ram) return 0xFF;
    uint8_t mmap = emu->ram[0xFFF9];

    // Suzy Hardware Registers ($FC00-$FCFF when bit 0 of MAPCTL is 0)
    if (addr >= 0xFC00 && addr <= 0xFCFF && !(mmap & 0x01)) {
        uint8_t reg = (uint8_t)(addr & 0xFF);
        if (reg == 0xB0) {
            // JOYSW: Bit 7=Up, 6=Down, 5=Left, 4=Right, 3=Opt1, 2=Opt2, 1=B, 0=A
            uint8_t j = 0;
            if (emu->buttons & 0x01) j |= 0x80; // Up
            if (emu->buttons & 0x02) j |= 0x40; // Down
            if (emu->buttons & 0x04) j |= 0x20; // Left
            if (emu->buttons & 0x08) j |= 0x10; // Right
            if (emu->buttons & 0x40) j |= 0x08; // Option 1
            if (emu->buttons & 0x20) j |= 0x02; // Inside/B
            if (emu->buttons & 0x10) j |= 0x01; // Outside/A
            return j;
        }
        if (reg == 0xB2) {
            // RCART0: Cartridge serial data read
            if (emu->rom && emu->rom_size > emu->header_len) {
                uint32_t payload = emu->rom_size - emu->header_len;
                uint8_t b = emu->rom[emu->header_len + (emu->rom_shift_addr % payload)];
                emu->rom_shift_addr++;
                return b;
            }
            return 0xFF;
        }
        return emu->suzy[reg];
    }

    // Mikey Hardware Registers ($FD00-$FDFF when bit 1 of MAPCTL is 0)
    if (addr >= 0xFD00 && addr <= 0xFDFF && !(mmap & 0x02)) {
        return emu->mikey[addr & 0xFF];
    }

    return emu->ram[addr];
}

static void lynx_write8(lynx_t* emu, uint16_t addr, uint8_t val) {
    if (!emu->ram) return;
    if (addr == 0xFFF9) {
        emu->ram[0xFFF9] = val;
        return;
    }
    uint8_t mmap = emu->ram[0xFFF9];

    if (addr >= 0xFC00 && addr <= 0xFCFF && !(mmap & 0x01)) {
        uint8_t reg = (uint8_t)(addr & 0xFF);
        emu->suzy[reg] = val;
        if (reg == 0x52) {
            // MATHA write triggers 16x16 hardware multiply: AB * CD -> EFGH
            uint16_t ab = (uint16_t)emu->suzy[0x53] | ((uint16_t)emu->suzy[0x52] << 8);
            uint16_t cd = (uint16_t)emu->suzy[0x55] | ((uint16_t)emu->suzy[0x54] << 8);
            uint32_t prod = (uint32_t)ab * (uint32_t)cd;
            emu->suzy[0x63] = (uint8_t)(prod & 0xFF);
            emu->suzy[0x62] = (uint8_t)((prod >> 8) & 0xFF);
            emu->suzy[0x61] = (uint8_t)((prod >> 16) & 0xFF);
            emu->suzy[0x60] = (uint8_t)((prod >> 24) & 0xFF);
        } else if (reg == 0x91 && (val & 0x01)) {
            lynx_execute_scb(emu);
        }
        return;
    }

    if (addr >= 0xFD00 && addr <= 0xFDFF && !(mmap & 0x02)) {
        uint8_t reg = (uint8_t)(addr & 0xFF);
        emu->mikey[reg] = val;
        if (reg == 0x94 || reg == 0x95) {
            emu->disp_addr = (uint16_t)emu->mikey[0x94] | ((uint16_t)emu->mikey[0x95] << 8);
        } else if (reg >= 0xA0 && reg <= 0xBF) {
            lynx_update_palette(emu, reg & 0x0F);
        }
        return;
    }

    emu->ram[addr] = val;
}

static inline uint16_t lynx_read16(lynx_t* emu, uint16_t addr) {
    return (uint16_t)lynx_read8(emu, addr) | ((uint16_t)lynx_read8(emu, (uint16_t)(addr + 1)) << 8);
}

static inline void lynx_push8(lynx_t* emu, uint8_t val) {
    lynx_write8(emu, (uint16_t)(0x0100 | emu->sp), val);
    emu->sp--;
}

static inline uint8_t lynx_pop8(lynx_t* emu) {
    emu->sp++;
    return lynx_read8(emu, (uint16_t)(0x0100 | emu->sp));
}

static void lynx_adc(lynx_t* emu, uint8_t val) {
    uint16_t sum = (uint16_t)emu->a + val + ((emu->p & LYNX_C) ? 1 : 0);
    emu->p &= ~(LYNX_C | LYNX_V | LYNX_Z | LYNX_N);
    if (~(emu->a ^ val) & (emu->a ^ sum) & 0x80) emu->p |= LYNX_V;
    if (sum > 0xFF) emu->p |= LYNX_C;
    emu->a = (uint8_t)sum;
    lynx_set_zn(emu, emu->a);
}

static void lynx_sbc(lynx_t* emu, uint8_t val) {
    lynx_adc(emu, (uint8_t)(~val));
}

static void lynx_cmp(lynx_t* emu, uint8_t reg, uint8_t val) {
    uint16_t diff = (uint16_t)reg - val;
    emu->p &= ~(LYNX_C | LYNX_Z | LYNX_N);
    if (reg >= val) emu->p |= LYNX_C;
    lynx_set_zn(emu, (uint8_t)diff);
}

static int lynx_cpu_step(lynx_t* emu) {
    uint8_t op = lynx_read8(emu, emu->pc++);
    int cyc = 2;

    switch (op) {
        case 0xEA: break; // NOP
        // 65SC02 CMOS extensions
        case 0x80: { int8_t d = (int8_t)lynx_read8(emu, emu->pc++); emu->pc = (uint16_t)(emu->pc + d); cyc = 3; break; } // BRA
        case 0x64: lynx_write8(emu, lynx_read8(emu, emu->pc++), 0); cyc = 3; break; // STZ zp
        case 0x74: lynx_write8(emu, (uint8_t)(lynx_read8(emu, emu->pc++) + emu->x), 0); cyc = 4; break; // STZ zp,X
        case 0x9C: { uint16_t a = lynx_read16(emu, emu->pc); emu->pc += 2; lynx_write8(emu, a, 0); cyc = 4; break; } // STZ abs
        case 0x9E: { uint16_t a = lynx_read16(emu, emu->pc) + emu->x; emu->pc += 2; lynx_write8(emu, a, 0); cyc = 5; break; } // STZ abs,X
        case 0x1A: emu->a++; lynx_set_zn(emu, emu->a); break; // INC A
        case 0x3A: emu->a--; lynx_set_zn(emu, emu->a); break; // DEC A
        case 0xDA: lynx_push8(emu, emu->x); cyc = 3; break; // PHX
        case 0xFA: emu->x = lynx_pop8(emu); lynx_set_zn(emu, emu->x); cyc = 4; break; // PLX
        case 0x5A: lynx_push8(emu, emu->y); cyc = 3; break; // PHY
        case 0x7A: emu->y = lynx_pop8(emu); lynx_set_zn(emu, emu->y); cyc = 4; break; // PLY
        case 0xB2: { uint8_t zp = lynx_read8(emu, emu->pc++); uint16_t a = lynx_read8(emu, zp) | ((uint16_t)lynx_read8(emu, (uint8_t)(zp + 1)) << 8); emu->a = lynx_read8(emu, a); lynx_set_zn(emu, emu->a); cyc = 5; break; } // LDA (zp)
        case 0x92: { uint8_t zp = lynx_read8(emu, emu->pc++); uint16_t a = lynx_read8(emu, zp) | ((uint16_t)lynx_read8(emu, (uint8_t)(zp + 1)) << 8); lynx_write8(emu, a, emu->a); cyc = 5; break; } // STA (zp)

        // Standard 6502 Opcodes
        case 0xA9: emu->a = lynx_read8(emu, emu->pc++); lynx_set_zn(emu, emu->a); break;
        case 0xA5: emu->a = lynx_read8(emu, lynx_read8(emu, emu->pc++)); lynx_set_zn(emu, emu->a); cyc = 3; break;
        case 0xB5: emu->a = lynx_read8(emu, (uint8_t)(lynx_read8(emu, emu->pc++) + emu->x)); lynx_set_zn(emu, emu->a); cyc = 4; break;
        case 0xAD: { uint16_t a = lynx_read16(emu, emu->pc); emu->pc += 2; emu->a = lynx_read8(emu, a); lynx_set_zn(emu, emu->a); cyc = 4; break; }
        case 0xBD: { uint16_t a = lynx_read16(emu, emu->pc) + emu->x; emu->pc += 2; emu->a = lynx_read8(emu, a); lynx_set_zn(emu, emu->a); cyc = 4; break; }
        case 0xB9: { uint16_t a = lynx_read16(emu, emu->pc) + emu->y; emu->pc += 2; emu->a = lynx_read8(emu, a); lynx_set_zn(emu, emu->a); cyc = 4; break; }
        case 0xB1: { uint8_t zp = lynx_read8(emu, emu->pc++); uint16_t a = (lynx_read8(emu, zp) | ((uint16_t)lynx_read8(emu, (uint8_t)(zp + 1)) << 8)) + emu->y; emu->a = lynx_read8(emu, a); lynx_set_zn(emu, emu->a); cyc = 5; break; }

        case 0x85: lynx_write8(emu, lynx_read8(emu, emu->pc++), emu->a); cyc = 3; break;
        case 0x95: lynx_write8(emu, (uint8_t)(lynx_read8(emu, emu->pc++) + emu->x), emu->a); cyc = 4; break;
        case 0x8D: { uint16_t a = lynx_read16(emu, emu->pc); emu->pc += 2; lynx_write8(emu, a, emu->a); cyc = 4; break; }
        case 0x9D: { uint16_t a = lynx_read16(emu, emu->pc) + emu->x; emu->pc += 2; lynx_write8(emu, a, emu->a); cyc = 5; break; }
        case 0x99: { uint16_t a = lynx_read16(emu, emu->pc) + emu->y; emu->pc += 2; lynx_write8(emu, a, emu->a); cyc = 5; break; }
        case 0x91: { uint8_t zp = lynx_read8(emu, emu->pc++); uint16_t a = (lynx_read8(emu, zp) | ((uint16_t)lynx_read8(emu, (uint8_t)(zp + 1)) << 8)) + emu->y; lynx_write8(emu, a, emu->a); cyc = 6; break; }

        case 0xA2: emu->x = lynx_read8(emu, emu->pc++); lynx_set_zn(emu, emu->x); break;
        case 0xA6: emu->x = lynx_read8(emu, lynx_read8(emu, emu->pc++)); lynx_set_zn(emu, emu->x); cyc = 3; break;
        case 0xAE: { uint16_t a = lynx_read16(emu, emu->pc); emu->pc += 2; emu->x = lynx_read8(emu, a); lynx_set_zn(emu, emu->x); cyc = 4; break; }
        case 0x86: lynx_write8(emu, lynx_read8(emu, emu->pc++), emu->x); cyc = 3; break;
        case 0x8E: { uint16_t a = lynx_read16(emu, emu->pc); emu->pc += 2; lynx_write8(emu, a, emu->x); cyc = 4; break; }

        case 0xA0: emu->y = lynx_read8(emu, emu->pc++); lynx_set_zn(emu, emu->y); break;
        case 0xA4: emu->y = lynx_read8(emu, lynx_read8(emu, emu->pc++)); lynx_set_zn(emu, emu->y); cyc = 3; break;
        case 0xAC: { uint16_t a = lynx_read16(emu, emu->pc); emu->pc += 2; emu->y = lynx_read8(emu, a); lynx_set_zn(emu, emu->y); cyc = 4; break; }
        case 0x84: lynx_write8(emu, lynx_read8(emu, emu->pc++), emu->y); cyc = 3; break;
        case 0x8C: { uint16_t a = lynx_read16(emu, emu->pc); emu->pc += 2; lynx_write8(emu, a, emu->y); cyc = 4; break; }

        case 0xAA: emu->x = emu->a; lynx_set_zn(emu, emu->x); break;
        case 0x8A: emu->a = emu->x; lynx_set_zn(emu, emu->a); break;
        case 0xA8: emu->y = emu->a; lynx_set_zn(emu, emu->y); break;
        case 0x98: emu->a = emu->y; lynx_set_zn(emu, emu->a); break;
        case 0xBA: emu->x = emu->sp; lynx_set_zn(emu, emu->x); break;
        case 0x9A: emu->sp = emu->x; break;
        case 0x48: lynx_push8(emu, emu->a); cyc = 3; break;
        case 0x68: emu->a = lynx_pop8(emu); lynx_set_zn(emu, emu->a); cyc = 4; break;

        case 0x29: emu->a &= lynx_read8(emu, emu->pc++); lynx_set_zn(emu, emu->a); break;
        case 0x09: emu->a |= lynx_read8(emu, emu->pc++); lynx_set_zn(emu, emu->a); break;
        case 0x49: emu->a ^= lynx_read8(emu, emu->pc++); lynx_set_zn(emu, emu->a); break;
        case 0x69: lynx_adc(emu, lynx_read8(emu, emu->pc++)); break;
        case 0x65: lynx_adc(emu, lynx_read8(emu, lynx_read8(emu, emu->pc++))); cyc = 3; break;
        case 0xE9: lynx_sbc(emu, lynx_read8(emu, emu->pc++)); break;
        case 0xE5: lynx_sbc(emu, lynx_read8(emu, lynx_read8(emu, emu->pc++))); cyc = 3; break;
        case 0xC9: lynx_cmp(emu, emu->a, lynx_read8(emu, emu->pc++)); break;
        case 0xE0: lynx_cmp(emu, emu->x, lynx_read8(emu, emu->pc++)); break;
        case 0xC0: lynx_cmp(emu, emu->y, lynx_read8(emu, emu->pc++)); break;

        case 0xE8: emu->x++; lynx_set_zn(emu, emu->x); break;
        case 0xCA: emu->x--; lynx_set_zn(emu, emu->x); break;
        case 0xC8: emu->y++; lynx_set_zn(emu, emu->y); break;
        case 0x88: emu->y--; lynx_set_zn(emu, emu->y); break;
        case 0xE6: { uint8_t zp = lynx_read8(emu, emu->pc++); uint8_t v = lynx_read8(emu, zp) + 1; lynx_write8(emu, zp, v); lynx_set_zn(emu, v); cyc = 5; break; }
        case 0xC6: { uint8_t zp = lynx_read8(emu, emu->pc++); uint8_t v = lynx_read8(emu, zp) - 1; lynx_write8(emu, zp, v); lynx_set_zn(emu, v); cyc = 5; break; }

        case 0x0A: emu->p = (emu->p & ~LYNX_C) | (emu->a >> 7); emu->a <<= 1; lynx_set_zn(emu, emu->a); break;
        case 0x4A: emu->p = (emu->p & ~LYNX_C) | (emu->a & 1); emu->a >>= 1; lynx_set_zn(emu, emu->a); break;

        case 0x90: { int8_t d = (int8_t)lynx_read8(emu, emu->pc++); if (!(emu->p & LYNX_C)) { emu->pc = (uint16_t)(emu->pc + d); cyc = 3; } break; }
        case 0xB0: { int8_t d = (int8_t)lynx_read8(emu, emu->pc++); if (emu->p & LYNX_C)  { emu->pc = (uint16_t)(emu->pc + d); cyc = 3; } break; }
        case 0xF0: { int8_t d = (int8_t)lynx_read8(emu, emu->pc++); if (emu->p & LYNX_Z)  { emu->pc = (uint16_t)(emu->pc + d); cyc = 3; } break; }
        case 0xD0: { int8_t d = (int8_t)lynx_read8(emu, emu->pc++); if (!(emu->p & LYNX_Z)) { emu->pc = (uint16_t)(emu->pc + d); cyc = 3; } break; }
        case 0x30: { int8_t d = (int8_t)lynx_read8(emu, emu->pc++); if (emu->p & LYNX_N)  { emu->pc = (uint16_t)(emu->pc + d); cyc = 3; } break; }
        case 0x10: { int8_t d = (int8_t)lynx_read8(emu, emu->pc++); if (!(emu->p & LYNX_N)) { emu->pc = (uint16_t)(emu->pc + d); cyc = 3; } break; }

        case 0x4C: emu->pc = lynx_read16(emu, emu->pc); cyc = 3; break;
        case 0x20: { uint16_t dst = lynx_read16(emu, emu->pc); uint16_t ret = (uint16_t)(emu->pc + 1); lynx_push8(emu, (uint8_t)(ret >> 8)); lynx_push8(emu, (uint8_t)(ret & 0xFF)); emu->pc = dst; cyc = 6; break; }
        case 0x60: { uint16_t lo = lynx_pop8(emu); uint16_t hi = lynx_pop8(emu); emu->pc = (uint16_t)(((hi << 8) | lo) + 1); cyc = 6; break; }
        case 0x40: { emu->p = (uint8_t)((lynx_pop8(emu) & ~LYNX_B) | LYNX_U); uint16_t lo = lynx_pop8(emu); uint16_t hi = lynx_pop8(emu); emu->pc = (uint16_t)((hi << 8) | lo); cyc = 6; break; }

        case 0x18: emu->p &= ~LYNX_C; break;
        case 0x38: emu->p |= LYNX_C;  break;
        case 0x58: emu->p &= ~LYNX_I; break;
        case 0x78: emu->p |= LYNX_I;  break;
        case 0xD8: emu->p &= ~LYNX_D; break;
        default: break;
    }
    return cyc;
}

void lynx_init(lynx_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb) {
    if (!emu) return;
    memset(emu, 0, sizeof(lynx_t));
    emu->rom = rom;
    emu->rom_size = rom_size;
    emu->ram = ram;
    emu->framebuffer = fb;

    if (emu->ram) memset(emu->ram, 0, 64 * 1024);

    // Default Mikey 16-color grayscale/color ramp
    for (int i = 0; i < 16; i++) {
        emu->mikey[0xA0 + i] = (uint8_t)i;
        emu->mikey[0xB0 + i] = (uint8_t)((i << 4) | i);
        lynx_update_palette(emu, (uint8_t)i);
    }

    emu->disp_addr = 0xC000;
    emu->mikey[0x94] = 0x00;
    emu->mikey[0x95] = 0xC0;
    emu->sp = 0xFF;
    emu->p = LYNX_U | LYNX_I;

    if (rom && rom_size >= 64) {
        emu->header_len = (rom[0] == 'L' && rom[1] == 'Y' && rom[2] == 'N' && rom[3] == 'X') ? 64 : 0;
        uint32_t payload = rom_size - emu->header_len;
        // Load initial boot image into RAM at $0200 (standard Lynx boot load address)
        if (emu->ram && payload > 0) {
            uint32_t copy_len = (payload > 0xFA00) ? 0xFA00 : payload;
            memcpy(&emu->ram[0x0200], &rom[emu->header_len], copy_len);
        }
        emu->pc = 0x0200;
    }
}

void lynx_set_buttons(lynx_t* emu, uint8_t buttons) {
    if (emu) emu->buttons = buttons;
}

void lynx_step_frame(lynx_t* emu) {
    if (!emu || !emu->framebuffer || !emu->ram) return;

    // Step 105 scanlines per 60Hz frame (102 visible + 3 VBlank)
    for (int line = 0; line < 105; line++) {
        emu->mikey[0x0A] = (uint8_t)(104 - line); // Timer 2 (VBlank counter)
        int cycles = 0;
        while (cycles < 256 && !emu->halted) {
            cycles += lynx_cpu_step(emu);
        }

        if (line < 102) {
            uint16_t* dst = &emu->framebuffer[line * 160];
            uint16_t row_base = (uint16_t)(emu->disp_addr + line * 80);
            for (int byte_x = 0; byte_x < 80; byte_x++) {
                uint8_t b = emu->ram[(uint16_t)(row_base + byte_x)];
                dst[byte_x * 2]     = emu->palette_cache[(b >> 4) & 0x0F];
                dst[byte_x * 2 + 1] = emu->palette_cache[b & 0x0F];
            }
        }
    }
}
