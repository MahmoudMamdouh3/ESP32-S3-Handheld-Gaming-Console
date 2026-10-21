#pragma GCC optimize ("O3,unroll-loops")
// ---------------------------------------------------------------------------
// ngp.c – SNK Neo Geo Pocket / Color TLCS-900/H + K1GE/K2GE Video Core Engine
//
// Features:
//   - Toshiba TLCS-900/H 32-bit CISC CPU interpreter (6.144 MHz)
//   - 24-bit linear memory bus: Work RAM ($004000-$006FFF), Joypad ($006F82),
//     K1GE/K2GE Video ASIC ($008000-$00BFFF), Cartridge ROM ($200000-$3FFFFF)
//   - K1GE/K2GE 160x152 Scanline Renderer: Scroll Plane 1 ($9000), Scroll
//     Plane 2 ($9800), 64 Hardware Sprites ($8800), 512 2bpp Character Tiles
//     ($A000-$BFFF), 12-bit RGB444 Color Palette RAM ($8200-$83FF)
// ---------------------------------------------------------------------------

#include "ngp.h"
#include <string.h>

#define NGP_FLAG_C 0x01
#define NGP_FLAG_N 0x02
#define NGP_FLAG_V 0x04
#define NGP_FLAG_Z 0x40
#define NGP_FLAG_S 0x80

// Map 24-bit NGP address ($004000-$00BFFF) into the 32KB `emu->ram` buffer:
//   emu->ram[0x0000..0x3FFF] -> $004000..$007FFF (16KB Work/System RAM)
//   emu->ram[0x4000..0x7FFF] -> $008000..$00BFFF (16KB K1GE/K2GE Video RAM/Regs)
static inline uint16_t ngp_rgb444_to_bgr565(uint16_t c12) {
    uint8_t r = (uint8_t)((c12 & 0x0F) * 17);
    uint8_t g = (uint8_t)(((c12 >> 4) & 0x0F) * 17);
    uint8_t b = (uint8_t)(((c12 >> 8) & 0x0F) * 17);
    uint16_t bgr = ((uint16_t)(b & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (r >> 3);
    return (uint16_t)(((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF));
}

static uint8_t ngp_read8(ngp_t* emu, uint32_t addr) {
    addr &= 0xFFFFFF;
    if (addr == 0x006F82 || addr == 0x0000B0) {
        return emu->joypad;
    }
    if (addr >= 0x004000 && addr < 0x00C000 && emu->ram) {
        return emu->ram[addr - 0x004000];
    }
    if (addr < 0x004000 && emu->ram) {
        return emu->ram[addr & 0x3FFF];
    }
    if (addr >= 0x200000 && emu->rom && emu->rom_size > 0) {
        uint32_t off = addr - 0x200000;
        if (off < emu->rom_size) return emu->rom[off];
    }
    if (emu->rom && addr < emu->rom_size) {
        return emu->rom[addr];
    }
    return 0x00;
}

static void ngp_write8(ngp_t* emu, uint32_t addr, uint8_t val) {
    addr &= 0xFFFFFF;
    if (addr >= 0x004000 && addr < 0x00C000 && emu->ram) {
        uint16_t off = (uint16_t)(addr - 0x004000);
        emu->ram[off] = val;
        // K2GE Color Palette RAM ($008200 - $0083FF -> off 0x4200 - 0x43FF)
        if (addr >= 0x008200 && addr < 0x008400) {
            uint16_t pal_idx = (uint16_t)((addr - 0x008200) >> 1);
            uint16_t base = (uint16_t)(0x4200 + (pal_idx << 1));
            uint16_t c12 = (uint16_t)emu->ram[base] | ((uint16_t)emu->ram[base + 1] << 8);
            emu->palette_cache[pal_idx & 0xFF] = ngp_rgb444_to_bgr565(c12);
        }
        return;
    }
    if (addr < 0x004000 && emu->ram) {
        emu->ram[addr & 0x3FFF] = val;
    }
}

static inline uint16_t ngp_read16(ngp_t* emu, uint32_t addr) {
    return (uint16_t)ngp_read8(emu, addr) | ((uint16_t)ngp_read8(emu, addr + 1) << 8);
}

static inline uint32_t ngp_read32(ngp_t* emu, uint32_t addr) {
    return (uint32_t)ngp_read16(emu, addr) | ((uint32_t)ngp_read16(emu, addr + 2) << 16);
}

static inline void ngp_write16(ngp_t* emu, uint32_t addr, uint16_t val) {
    ngp_write8(emu, addr, (uint8_t)(val & 0xFF));
    ngp_write8(emu, addr + 1, (uint8_t)(val >> 8));
}

static inline void ngp_write32(ngp_t* emu, uint32_t addr, uint32_t val) {
    ngp_write16(emu, addr, (uint16_t)(val & 0xFFFF));
    ngp_write16(emu, addr + 2, (uint16_t)(val >> 16));
}

static inline void ngp_set_sz8(ngp_t* emu, uint8_t v) {
    emu->sr &= ~(NGP_FLAG_Z | NGP_FLAG_S);
    if (v == 0) emu->sr |= NGP_FLAG_Z;
    if (v & 0x80) emu->sr |= NGP_FLAG_S;
}

static inline void ngp_set_sz16(ngp_t* emu, uint16_t v) {
    emu->sr &= ~(NGP_FLAG_Z | NGP_FLAG_S);
    if (v == 0) emu->sr |= NGP_FLAG_Z;
    if (v & 0x8000) emu->sr |= NGP_FLAG_S;
}

static int ngp_cpu_step(ngp_t* emu) {
    uint8_t op = ngp_read8(emu, emu->pc++);
    int cyc = 4;

    switch (op) {
        case 0x00: break; // NOP
        case 0x05: emu->halted = true; break; // HALT
        case 0x0E: { // RET
            emu->pc = ngp_read32(emu, emu->r[7]);
            emu->r[7] += 4;
            cyc = 10;
            break;
        }
        // LD R, #imm8 (0x20..0x27)
        case 0x20: case 0x21: case 0x22: case 0x23:
        case 0x24: case 0x25: case 0x26: case 0x27: {
            uint8_t reg = op & 7;
            uint8_t imm = ngp_read8(emu, emu->pc++);
            emu->r[reg] = (emu->r[reg] & 0xFFFFFF00U) | imm;
            break;
        }
        // LD RR, #imm16 (0x30..0x37)
        case 0x30: case 0x31: case 0x32: case 0x33:
        case 0x34: case 0x35: case 0x36: case 0x37: {
            uint8_t reg = op & 7;
            uint16_t imm = ngp_read16(emu, emu->pc);
            emu->pc += 2;
            emu->r[reg] = (emu->r[reg] & 0xFFFF0000U) | imm;
            cyc = 6;
            break;
        }
        // LD XRR, #imm32 (0x40..0x47)
        case 0x40: case 0x41: case 0x42: case 0x43:
        case 0x44: case 0x45: case 0x46: case 0x47: {
            uint8_t reg = op & 7;
            emu->r[reg] = ngp_read32(emu, emu->pc);
            emu->pc += 4;
            cyc = 8;
            break;
        }
        // PUSH XRR (0x28..0x2F) & POP XRR (0x58..0x5F)
        case 0x28: case 0x29: case 0x2A: case 0x2B:
        case 0x2C: case 0x2D: case 0x2E: case 0x2F: {
            emu->r[7] -= 4;
            ngp_write32(emu, emu->r[7], emu->r[op & 7]);
            cyc = 6;
            break;
        }
        case 0x58: case 0x59: case 0x5A: case 0x5B:
        case 0x5C: case 0x5D: case 0x5E: case 0x5F: {
            emu->r[op & 7] = ngp_read32(emu, emu->r[7]);
            emu->r[7] += 4;
            cyc = 6;
            break;
        }
        // JR cc, d8 (0x60..0x6F)
        case 0x60: case 0x61: case 0x62: case 0x63:
        case 0x64: case 0x65: case 0x66: case 0x67:
        case 0x68: case 0x69: case 0x6A: case 0x6B:
        case 0x6C: case 0x6D: case 0x6E: case 0x6F: {
            int8_t disp = (int8_t)ngp_read8(emu, emu->pc++);
            uint8_t cc = op & 0x0F;
            bool take = false;
            if (cc == 0x08) take = true; // T (Always)
            else if (cc == 0x06) take = (emu->sr & NGP_FLAG_Z) != 0; // Z/EQ
            else if (cc == 0x0E) take = (emu->sr & NGP_FLAG_Z) == 0; // NZ/NE
            else if (cc == 0x07) take = (emu->sr & NGP_FLAG_C) != 0; // C/ULT
            else if (cc == 0x0F) take = (emu->sr & NGP_FLAG_C) == 0; // NC/UGE
            if (take) { emu->pc = (uint32_t)((int32_t)emu->pc + disp); cyc = 8; }
            break;
        }
        // JR cc, d16 (0x78 = Always)
        case 0x78: {
            int16_t disp = (int16_t)ngp_read16(emu, emu->pc);
            emu->pc += 2;
            emu->pc = (uint32_t)((int32_t)emu->pc + disp);
            cyc = 8;
            break;
        }
        // JP #addr24 (0x1A) / CALL #addr24 (0x1C)
        case 0x1A: {
            uint32_t dst = ngp_read32(emu, emu->pc) & 0xFFFFFF;
            emu->pc = dst;
            cyc = 8;
            break;
        }
        case 0x1C: {
            uint32_t dst = ngp_read32(emu, emu->pc) & 0xFFFFFF;
            emu->pc += 3;
            emu->r[7] -= 4;
            ngp_write32(emu, emu->r[7], emu->pc);
            emu->pc = dst;
            cyc = 12;
            break;
        }
        // Indirect memory load/store via (XRR): 0x80..0x87 read byte, 0x90..0x97 read word, 0xB0..0xB7 write
        case 0x80: case 0x81: case 0x82: case 0x83:
        case 0x84: case 0x85: case 0x86: case 0x87: {
            uint32_t addr = emu->r[op & 7];
            uint8_t sub = ngp_read8(emu, emu->pc++);
            uint8_t v = ngp_read8(emu, addr);
            if ((sub & 0xF8) == 0x20) {
                emu->r[sub & 7] = (emu->r[sub & 7] & 0xFFFFFF00U) | v;
                ngp_set_sz8(emu, v);
            }
            cyc = 6;
            break;
        }
        case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        case 0xB4: case 0xB5: case 0xB6: case 0xB7: {
            uint32_t addr = emu->r[op & 7];
            uint8_t sub = ngp_read8(emu, emu->pc++);
            if (sub == 0x00) { // LD (XRR), #imm8
                ngp_write8(emu, addr, ngp_read8(emu, emu->pc++));
            } else if (sub == 0x02) { // LDW (XRR), #imm16
                ngp_write16(emu, addr, ngp_read16(emu, emu->pc));
                emu->pc += 2;
            } else if ((sub & 0xF8) == 0x40) { // LD (XRR), R
                ngp_write8(emu, addr, (uint8_t)(emu->r[sub & 7] & 0xFF));
            } else if ((sub & 0xF8) == 0x50) { // LD (XRR), RR
                ngp_write16(emu, addr, (uint16_t)(emu->r[sub & 7] & 0xFFFF));
            }
            cyc = 6;
            break;
        }
        // Register ALU prefix 0xC8..0xCF (byte) & 0xD8..0xDF (word)
        case 0xC8: case 0xC9: case 0xCA: case 0xCB:
        case 0xCC: case 0xCD: case 0xCE: case 0xCF: {
            uint8_t reg = op & 7;
            uint8_t sub = ngp_read8(emu, emu->pc++);
            uint8_t cur = (uint8_t)(emu->r[reg] & 0xFF);
            if (sub == 0x61) { cur++; emu->r[reg] = (emu->r[reg] & 0xFFFFFF00U) | cur; ngp_set_sz8(emu, cur); } // INC 1, R
            else if (sub == 0x69) { cur--; emu->r[reg] = (emu->r[reg] & 0xFFFFFF00U) | cur; ngp_set_sz8(emu, cur); } // DEC 1, R
            else if (sub == 0xC8) { uint8_t imm = ngp_read8(emu, emu->pc++); cur += imm; emu->r[reg] = (emu->r[reg] & 0xFFFFFF00U) | cur; ngp_set_sz8(emu, cur); } // ADD R, #imm8
            else if (sub == 0xCA) { uint8_t imm = ngp_read8(emu, emu->pc++); cur -= imm; emu->r[reg] = (emu->r[reg] & 0xFFFFFF00U) | cur; ngp_set_sz8(emu, cur); } // SUB R, #imm8
            else if (sub == 0xCF) { uint8_t imm = ngp_read8(emu, emu->pc++); uint8_t d = cur - imm; emu->sr = (emu->sr & ~NGP_FLAG_C) | (cur < imm ? NGP_FLAG_C : 0); ngp_set_sz8(emu, d); } // CP R, #imm8
            break;
        }
        case 0xD8: case 0xD9: case 0xDA: case 0xDB:
        case 0xDC: case 0xDD: case 0xDE: case 0xDF: {
            uint8_t reg = op & 7;
            uint8_t sub = ngp_read8(emu, emu->pc++);
            uint16_t cur = (uint16_t)(emu->r[reg] & 0xFFFF);
            if (sub == 0x61) { cur++; emu->r[reg] = (emu->r[reg] & 0xFFFF0000U) | cur; ngp_set_sz16(emu, cur); }
            else if (sub == 0x62) { cur += 2; emu->r[reg] = (emu->r[reg] & 0xFFFF0000U) | cur; ngp_set_sz16(emu, cur); }
            else if (sub == 0x69) { cur--; emu->r[reg] = (emu->r[reg] & 0xFFFF0000U) | cur; ngp_set_sz16(emu, cur); }
            else if (sub == 0xC8) { uint16_t imm = ngp_read16(emu, emu->pc); emu->pc += 2; cur += imm; emu->r[reg] = (emu->r[reg] & 0xFFFF0000U) | cur; ngp_set_sz16(emu, cur); }
            else if (sub == 0xCF) { uint16_t imm = ngp_read16(emu, emu->pc); emu->pc += 2; uint16_t d = cur - imm; emu->sr = (emu->sr & ~NGP_FLAG_C) | (cur < imm ? NGP_FLAG_C : 0); ngp_set_sz16(emu, d); }
            break;
        }
        // Direct 16-bit address write prefix 0xF1 (e.g., LD (#addr16), #imm8 / R)
        case 0xF1: {
            uint16_t addr = ngp_read16(emu, emu->pc);
            emu->pc += 2;
            uint8_t sub = ngp_read8(emu, emu->pc++);
            if (sub == 0x00) {
                ngp_write8(emu, addr, ngp_read8(emu, emu->pc++));
            } else if (sub == 0x02) {
                ngp_write16(emu, addr, ngp_read16(emu, emu->pc));
                emu->pc += 2;
            } else if ((sub & 0xF8) == 0x40) {
                ngp_write8(emu, addr, (uint8_t)(emu->r[sub & 7] & 0xFF));
            } else if ((sub & 0xF8) == 0x50) {
                ngp_write16(emu, addr, (uint16_t)(emu->r[sub & 7] & 0xFFFF));
            }
            cyc = 8;
            break;
        }
        // Direct 16-bit address read prefix 0xC1
        case 0xC1: {
            uint16_t addr = ngp_read16(emu, emu->pc);
            emu->pc += 2;
            uint8_t sub = ngp_read8(emu, emu->pc++);
            uint8_t v = ngp_read8(emu, addr);
            if ((sub & 0xF8) == 0x20) {
                emu->r[sub & 7] = (emu->r[sub & 7] & 0xFFFFFF00U) | v;
                ngp_set_sz8(emu, v);
            }
            cyc = 8;
            break;
        }
        default: break;
    }
    return cyc;
}

static void ngp_render_plane(ngp_t* emu, uint16_t* dst, int line, uint16_t map_off, uint8_t scx, uint8_t scy, uint8_t pal_base) {
    uint8_t* vram = &emu->ram[0x4000]; // $008000 maps to emu->ram[0x4000]
    int py = (line + scy) & 0xFF;
    int tile_r = py >> 3;

    for (int sx = 0; sx < 160; sx++) {
        int px = (sx + scx) & 0xFF;
        int tile_c = px >> 3;
        uint16_t entry_addr = map_off + (uint16_t)((tile_r * 32 + tile_c) * 2);
        uint16_t entry = (uint16_t)vram[entry_addr] | ((uint16_t)vram[entry_addr + 1] << 8);

        uint16_t tile_idx = entry & 0x01FF;
        bool hflip = (entry & 0x8000) != 0;
        bool vflip = (entry & 0x4000) != 0;
        uint8_t pal = (uint8_t)((entry >> 9) & 0x0F);

        int fine_y = vflip ? (7 - (py & 7)) : (py & 7);
        int fine_x = hflip ? (px & 7) : (7 - (px & 7));

        // 2bpp tile data at $A000 (offset 0x2000 from $8000): 16 bytes per 8x8 tile (2 bytes/row)
        uint16_t row_addr = (uint16_t)(0x2000 + (tile_idx * 16) + (fine_y * 2));
        uint16_t row_bits = (uint16_t)vram[row_addr & 0x3FFF] | ((uint16_t)vram[(row_addr + 1) & 0x3FFF] << 8);
        uint8_t color_idx = (uint8_t)((row_bits >> (fine_x * 2)) & 0x03);
        if (color_idx != 0) {
            dst[sx] = emu->palette_cache[(pal_base + pal * 4 + color_idx) & 0xFF];
        }
    }
}

static void ngp_render_scanline(ngp_t* emu, int line) {
    if (line < 0 || line >= 152 || !emu->framebuffer || !emu->ram) return;
    uint16_t* dst = &emu->framebuffer[line * 160];
    uint8_t* vram = &emu->ram[0x4000]; // Base $008000

    uint16_t bg_col = emu->palette_cache[0xC0 + (vram[0x0118] & 0x07)];
    for (int x = 0; x < 160; x++) dst[x] = bg_col;

    bool pf_order = (vram[0x0030] & 0x80) != 0; // 0 = Plane 1 in front, 1 = Plane 2 in front
    uint8_t sc1x = vram[0x0020], sc1y = vram[0x0021];
    uint8_t sc2x = vram[0x0032], sc2y = vram[0x0033];

    if (pf_order) {
        ngp_render_plane(emu, dst, line, 0x1000, sc1x, sc1y, 0x40); // Plane 1 ($9000)
        ngp_render_plane(emu, dst, line, 0x1800, sc2x, sc2y, 0x80); // Plane 2 ($9800)
    } else {
        ngp_render_plane(emu, dst, line, 0x1800, sc2x, sc2y, 0x80);
        ngp_render_plane(emu, dst, line, 0x1000, sc1x, sc1y, 0x40);
    }

    // 64 Hardware Sprites at $8800 (vram[0x0800..0x08FF])
    for (int s = 0; s < 64; s++) {
        uint16_t sa = (uint16_t)(0x0800 + s * 4);
        uint8_t flags = vram[sa + 1];
        uint8_t prio = (flags >> 3) & 0x03;
        if (prio == 0) continue; // Sprite disabled

        uint16_t tile_idx = vram[sa] | ((uint16_t)(flags & 0x01) << 8);
        int sx = vram[sa + 2];
        int sy = vram[sa + 3];
        if (line < sy || line >= sy + 8) continue;

        bool hflip = (flags & 0x80) != 0;
        bool vflip = (flags & 0x40) != 0;
        uint8_t pal = vram[0x0C00 + s] & 0x0F;
        int fine_y = vflip ? (7 - (line - sy)) : (line - sy);
        uint16_t row_addr = (uint16_t)(0x2000 + (tile_idx * 16) + (fine_y * 2));
        uint16_t row_bits = (uint16_t)vram[row_addr & 0x3FFF] | ((uint16_t)vram[(row_addr + 1) & 0x3FFF] << 8);

        for (int px = 0; px < 8; px++) {
            int out_x = sx + px;
            if (out_x < 0 || out_x >= 160) continue;
            int fine_x = hflip ? px : (7 - px);
            uint8_t c = (uint8_t)((row_bits >> (fine_x * 2)) & 0x03);
            if (c != 0) {
                dst[out_x] = emu->palette_cache[(pal * 4 + c) & 0xFF];
            }
        }
    }
}

void ngp_init(ngp_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb, bool is_color) {
    if (!emu) return;
    memset(emu, 0, sizeof(ngp_t));
    emu->rom = rom;
    emu->rom_size = rom_size;
    emu->ram = ram;
    emu->framebuffer = fb;
    emu->is_color = is_color;

    if (emu->ram) memset(emu->ram, 0, 32 * 1024);

    // Seed default K1GE/K2GE palette cache
    uint16_t default_shades[4] = {
        ngp_rgb444_to_bgr565(0xEEE),
        ngp_rgb444_to_bgr565(0xAAA),
        ngp_rgb444_to_bgr565(0x555),
        ngp_rgb444_to_bgr565(0x111)
    };
    for (int i = 0; i < 256; i++) {
        emu->palette_cache[i] = default_shades[i & 3];
    }

    emu->r[7] = 0x00006C00; // Initial XSP stack pointer in Work RAM
    if (rom && rom_size >= 0x20) {
        // NGP Cartridge Header stores entry point at offset 0x1C (4-byte little-endian)
        uint32_t entry = (uint32_t)rom[0x1C] | ((uint32_t)rom[0x1D] << 8) |
                         ((uint32_t)rom[0x1E] << 16) | ((uint32_t)rom[0x1F] << 24);
        if (entry >= 0x200000 && entry < 0x200000 + rom_size) {
            emu->pc = entry;
        } else {
            emu->pc = (uint32_t)rom[0] | ((uint32_t)rom[1] << 8) |
                      ((uint32_t)rom[2] << 16) | ((uint32_t)rom[3] << 24);
        }
    } else {
        emu->pc = 0x200000;
    }
}

void ngp_set_joypad(ngp_t* emu, uint8_t pad) {
    if (emu) emu->joypad = pad;
}

void ngp_step_frame(ngp_t* emu) {
    if (!emu || !emu->framebuffer || !emu->ram) return;

    // 199 scanlines per 60Hz frame (152 active + 47 VBlank)
    for (int line = 0; line < 199; line++) {
        emu->ram[0x4009] = (uint8_t)line; // K1GE RAS.Y vertical line counter ($8009)
        if (line < 152) {
            emu->ram[0x4010] &= ~0x40;
            ngp_render_scanline(emu, line);
        } else if (line == 152) {
            emu->ram[0x4010] |= 0x40; // VBlank status bit ($8010 bit 6)
        }

        int cycles = 0;
        while (cycles < 515 && !emu->halted) {
            cycles += ngp_cpu_step(emu);
        }
    }
}
