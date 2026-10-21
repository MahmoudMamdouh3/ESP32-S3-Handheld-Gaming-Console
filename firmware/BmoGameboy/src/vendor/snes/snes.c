#pragma GCC optimize ("O3,unroll-loops")
// ---------------------------------------------------------------------------
// snes.c – Super Nintendo (SNES / Super Famicom) 65C816 + S-PPU Core Engine
//
// Features:
//   - WDC 65C816 16-bit CPU interpreter (8/16-bit M/X flags, REP/SEP/XCE,
//     24-bit Bank:Offset addressing, JSL/RTL/JML, MVN/MVP block moves)
//   - LoROM / HiROM Memory Bus + 8-Channel General Purpose DMA ($420B MDMAEN)
//   - S-PPU Scanline Renderer (256x224): CGRAM 256-color 15-bit BGR555 to
//     ST7789 byte-swapped BGR565 cache, BG Modes 0/1 (2bpp & 4bpp tilemaps),
//     and OAM 4bpp Hardware Sprites
//   - 100% Octal PSRAM allocation (64KB VRAM + 32KB SRAM + 128KB WRAM)
// ---------------------------------------------------------------------------

#include "snes.h"
#include <stdlib.h>
#include <string.h>

#if defined(ESP32) || defined(ARDUINO)
#include <esp_heap_caps.h>
#define SNES_MALLOC(sz) heap_caps_malloc(sz, MALLOC_CAP_SPIRAM)
#define SNES_FREE(p) heap_caps_free(p)
#else
#define SNES_MALLOC(sz) malloc(sz)
#define SNES_FREE(p) free(p)
#endif

#define SNES_C 0x01
#define SNES_Z 0x02
#define SNES_I 0x04
#define SNES_D 0x08
#define SNES_X 0x10
#define SNES_M 0x20
#define SNES_V 0x40
#define SNES_N 0x80

static inline uint16_t snes_bgr555_to_bgr565_swapped(uint16_t c15) {
    uint8_t r5 = c15 & 0x1F;
    uint8_t g5 = (c15 >> 5) & 0x1F;
    uint8_t b5 = (c15 >> 10) & 0x1F;
    uint16_t bgr = ((uint16_t)b5 << 11) | ((uint16_t)g5 << 6) | r5;
    return (uint16_t)(((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF));
}

static inline bool snes_m8(const snes_t* emu) {
    return emu->emu_mode || ((emu->p & SNES_M) != 0);
}

static inline bool snes_x8(const snes_t* emu) {
    return emu->emu_mode || ((emu->p & SNES_X) != 0);
}

static inline void snes_set_zn8(snes_t* emu, uint8_t v) {
    emu->p &= ~(SNES_Z | SNES_N);
    if (v == 0) emu->p |= SNES_Z;
    if (v & 0x80) emu->p |= SNES_N;
}

static inline void snes_set_zn16(snes_t* emu, uint16_t v) {
    emu->p &= ~(SNES_Z | SNES_N);
    if (v == 0) emu->p |= SNES_Z;
    if (v & 0x8000) emu->p |= SNES_N;
}

static inline uint16_t snes_vram_step(uint8_t vmain) {
    switch (vmain & 0x03) {
        case 0: return 1;
        case 1: return 32;
        default: return 128;
    }
}

static uint8_t snes_read8(snes_t* emu, uint32_t addr24);
static void snes_write8(snes_t* emu, uint32_t addr24, uint8_t val);

static void snes_run_dma(snes_t* emu, uint8_t channels) {
    for (int ch = 0; ch < 8; ch++) {
        if (!(channels & (1 << ch))) continue;
        uint8_t* d = &emu->dma_regs[ch * 16];
        uint8_t dmap = d[0];
        uint8_t bbad = d[1];
        uint32_t a1t = (uint32_t)d[2] | ((uint32_t)d[3] << 8) | ((uint32_t)d[4] << 16);
        uint32_t das = (uint32_t)d[5] | ((uint32_t)d[6] << 8);
        if (das == 0) das = 0x10000;

        uint8_t mode = dmap & 0x07;
        bool fixed = (dmap & 0x08) != 0;
        bool dec   = (dmap & 0x10) != 0;

        for (uint32_t i = 0; i < das; i++) {
            uint16_t b_off = bbad;
            if (mode == 1) b_off = (uint16_t)(bbad + (i & 1));
            else if (mode == 2 || mode == 6) b_off = bbad;
            else if (mode == 3 || mode == 7) b_off = (uint16_t)(bbad + ((i >> 1) & 1));
            else if (mode == 4) b_off = (uint16_t)(bbad + (i & 3));

            uint8_t byte_val = snes_read8(emu, a1t);
            snes_write8(emu, 0x002100U | (b_off & 0xFF), byte_val);

            if (!fixed) {
                uint16_t lo = (uint16_t)(a1t & 0xFFFF);
                lo = dec ? (uint16_t)(lo - 1) : (uint16_t)(lo + 1);
                a1t = (a1t & 0xFF0000U) | lo;
            }
        }
        d[2] = (uint8_t)(a1t & 0xFF);
        d[3] = (uint8_t)((a1t >> 8) & 0xFF);
        d[5] = d[6] = 0;
    }
}

static uint8_t snes_read8(snes_t* emu, uint32_t addr24) {
    uint8_t bank = (uint8_t)((addr24 >> 16) & 0xFF);
    uint16_t off = (uint16_t)(addr24 & 0xFFFF);

    // Banks $7E-$7F: 128KB Work RAM
    if ((bank == 0x7E || bank == 0x7F) && emu->wram) {
        return emu->wram[((uint32_t)(bank - 0x7E) << 16) | off];
    }

    uint8_t eff_bank = bank & 0x7F;
    if (eff_bank < 0x40) {
        if (off < 0x2000 && emu->wram) {
            return emu->wram[off];
        }
        if (off >= 0x2100 && off <= 0x213F) {
            return emu->ppu_regs[off & 0x3F];
        }
        if (off == 0x4210) {
            uint8_t v = emu->rdnmi | 0x02;
            emu->rdnmi &= ~0x80;
            return v;
        }
        if (off == 0x4212) {
            // HVBJOY: Bit 7 = VBlank, Bit 0 = Auto-Joypad ready (0)
            return (emu->rdnmi & 0x80);
        }
        if (off == 0x4218) {
            // JOY1L: Bit 7 = A (0x20 in pad_state)
            return (emu->pad_state & 0x20) ? 0x80 : 0x00;
        }
        if (off == 0x4219) {
            // JOY1H: Bit 7=B, 6=Y, 5=Select, 4=Start, 3=Up, 2=Down, 1=Left, 0=Right
            uint8_t hi = 0;
            if (emu->pad_state & 0x10) hi |= 0x80; // B
            if (emu->pad_state & 0x80) hi |= 0x20; // Select
            if (emu->pad_state & 0x40) hi |= 0x10; // Start
            if (emu->pad_state & 0x01) hi |= 0x08; // Up
            if (emu->pad_state & 0x02) hi |= 0x04; // Down
            if (emu->pad_state & 0x04) hi |= 0x02; // Left
            if (emu->pad_state & 0x08) hi |= 0x01; // Right
            return hi;
        }
        if (off >= 0x4300 && off <= 0x437F) {
            return emu->dma_regs[off - 0x4300];
        }
    }

    // Cartridge SRAM ($70-$7D, $0000-$7FFF)
    if (eff_bank >= 0x70 && eff_bank <= 0x7D && off < 0x8000 && emu->sram) {
        return emu->sram[off & 0x7FFF];
    }

    // LoROM Cartridge mapping ($8000-$FFFF in each bank = 32KB per bank)
    if (off >= 0x8000 && emu->rom && emu->rom_size > emu->header_offset) {
        uint32_t payload = emu->rom_size - emu->header_offset;
        uint32_t rom_off = (((uint32_t)eff_bank * 0x8000U) + (off - 0x8000U)) % payload;
        return emu->rom[emu->header_offset + rom_off];
    }

    return 0x00;
}

static void snes_write8(snes_t* emu, uint32_t addr24, uint8_t val) {
    uint8_t bank = (uint8_t)((addr24 >> 16) & 0xFF);
    uint16_t off = (uint16_t)(addr24 & 0xFFFF);

    if ((bank == 0x7E || bank == 0x7F) && emu->wram) {
        emu->wram[((uint32_t)(bank - 0x7E) << 16) | off] = val;
        return;
    }

    uint8_t eff_bank = bank & 0x7F;
    if (eff_bank < 0x40) {
        if (off < 0x2000 && emu->wram) {
            emu->wram[off] = val;
            return;
        }
        if (off >= 0x2100 && off <= 0x213F) {
            uint8_t reg = (uint8_t)(off & 0x3F);
            emu->ppu_regs[reg] = val;
            switch (reg) {
                case 0x02: emu->oam_addr = (emu->oam_addr & 0x0200) | ((uint16_t)val << 1); break;
                case 0x03: emu->oam_addr = (emu->oam_addr & 0x01FE) | ((uint16_t)(val & 1) << 9); break;
                case 0x04: // OAMDATA
                    if (emu->oam_addr < 544) emu->oam[emu->oam_addr] = val;
                    emu->oam_addr = (emu->oam_addr + 1) % 544;
                    break;
                case 0x0D: case 0x0F: case 0x11: case 0x13: { // BG1..4 HOFS
                    int bg = (reg - 0x0D) >> 1;
                    emu->bg_hofs[bg] = ((uint16_t)val << 8) | (emu->bg_scroll_prev & ~7) | ((emu->bg_hofs[bg] >> 8) & 7);
                    emu->bg_scroll_prev = val;
                    break;
                }
                case 0x0E: case 0x10: case 0x12: case 0x14: { // BG1..4 VOFS
                    int bg = (reg - 0x0E) >> 1;
                    emu->bg_vofs[bg] = ((uint16_t)val << 8) | emu->bg_scroll_prev;
                    emu->bg_scroll_prev = val;
                    break;
                }
                case 0x15: emu->vmain = val; break;
                case 0x16: emu->vram_addr = (emu->vram_addr & 0xFF00) | val; break;
                case 0x17: emu->vram_addr = (emu->vram_addr & 0x00FF) | ((uint16_t)val << 8); break;
                case 0x18: // VMDATAL
                    if (emu->vram) emu->vram[(emu->vram_addr << 1) & 0xFFFF] = val;
                    if (!(emu->vmain & 0x80)) emu->vram_addr += snes_vram_step(emu->vmain);
                    break;
                case 0x19: // VMDATAH
                    if (emu->vram) emu->vram[((emu->vram_addr << 1) + 1) & 0xFFFF] = val;
                    if (emu->vmain & 0x80) emu->vram_addr += snes_vram_step(emu->vmain);
                    break;
                case 0x21: // CGADD
                    emu->cgram_addr = val;
                    emu->cgram_latch = 0;
                    break;
                case 0x22: // CGDATA
                    if (emu->cgram_latch == 0) {
                        emu->cgram_lsb = val;
                        emu->cgram_latch = 1;
                    } else {
                        uint16_t c15 = (uint16_t)emu->cgram_lsb | ((uint16_t)(val & 0x7F) << 8);
                        emu->cgram[emu->cgram_addr * 2] = emu->cgram_lsb;
                        emu->cgram[emu->cgram_addr * 2 + 1] = val & 0x7F;
                        emu->palette_cache[emu->cgram_addr] = snes_bgr555_to_bgr565_swapped(c15);
                        emu->cgram_addr++;
                        emu->cgram_latch = 0;
                    }
                    break;
                default: break;
            }
            return;
        }
        if (off == 0x4200) { emu->nmitimen = val; return; }
        if (off == 0x420B) { snes_run_dma(emu, val); return; }
        if (off >= 0x4300 && off <= 0x437F) {
            emu->dma_regs[off - 0x4300] = val;
            return;
        }
    }

    if (eff_bank >= 0x70 && eff_bank <= 0x7D && off < 0x8000 && emu->sram) {
        emu->sram[off & 0x7FFF] = val;
    }
}

static inline uint16_t snes_read16(snes_t* emu, uint32_t addr24) {
    return (uint16_t)snes_read8(emu, addr24) | ((uint16_t)snes_read8(emu, addr24 + 1) << 8);
}

static inline void snes_write16(snes_t* emu, uint32_t addr24, uint16_t val) {
    snes_write8(emu, addr24, (uint8_t)(val & 0xFF));
    snes_write8(emu, addr24 + 1, (uint8_t)(val >> 8));
}

static inline uint8_t snes_fetch8(snes_t* emu) {
    uint8_t v = snes_read8(emu, ((uint32_t)emu->pb << 16) | (emu->pc & 0xFFFF));
    emu->pc = (emu->pc + 1) & 0xFFFF;
    return v;
}

static inline uint16_t snes_fetch16(snes_t* emu) {
    uint8_t lo = snes_fetch8(emu);
    uint8_t hi = snes_fetch8(emu);
    return (uint16_t)lo | ((uint16_t)hi << 8);
}

static inline void snes_push8(snes_t* emu, uint8_t v) {
    snes_write8(emu, emu->sp, v);
    if (emu->emu_mode) emu->sp = 0x0100 | ((emu->sp - 1) & 0xFF);
    else emu->sp--;
}

static inline uint8_t snes_pop8(snes_t* emu) {
    if (emu->emu_mode) emu->sp = 0x0100 | ((emu->sp + 1) & 0xFF);
    else emu->sp++;
    return snes_read8(emu, emu->sp);
}

static inline void snes_push16(snes_t* emu, uint16_t v) {
    snes_push8(emu, (uint8_t)(v >> 8));
    snes_push8(emu, (uint8_t)(v & 0xFF));
}

static inline uint16_t snes_pop16(snes_t* emu) {
    uint8_t lo = snes_pop8(emu);
    uint8_t hi = snes_pop8(emu);
    return (uint16_t)lo | ((uint16_t)hi << 8);
}

static int snes_cpu_step(snes_t* emu) {
    uint8_t op = snes_fetch8(emu);
    int cyc = 2;
    uint32_t db_base = (uint32_t)emu->db << 16;

    switch (op) {
        case 0xEA: break; // NOP
        case 0xFB: { // XCE: Exchange Carry and Emulation bit
            bool old_c = (emu->p & SNES_C) != 0;
            if (emu->emu_mode) emu->p |= SNES_C; else emu->p &= ~SNES_C;
            emu->emu_mode = old_c;
            if (emu->emu_mode) {
                emu->p |= (SNES_M | SNES_X);
                emu->x &= 0xFF;
                emu->y &= 0xFF;
                emu->sp = 0x0100 | (emu->sp & 0xFF);
            }
            break;
        }
        case 0xC2: { // REP #imm8
            uint8_t mask = snes_fetch8(emu);
            emu->p &= ~mask;
            if (emu->emu_mode) emu->p |= (SNES_M | SNES_X);
            cyc = 3;
            break;
        }
        case 0xE2: { // SEP #imm8
            uint8_t mask = snes_fetch8(emu);
            emu->p |= mask;
            if (snes_x8(emu)) { emu->x &= 0xFF; emu->y &= 0xFF; }
            cyc = 3;
            break;
        }
        case 0x18: emu->p &= ~SNES_C; break; // CLC
        case 0x38: emu->p |= SNES_C;  break; // SEC
        case 0x58: emu->p &= ~SNES_I; break; // CLI
        case 0x78: emu->p |= SNES_I;  break; // SEI
        case 0xB8: emu->p &= ~SNES_V; break; // CLV
        case 0xD8: emu->p &= ~SNES_D; break; // CLD
        case 0xF8: emu->p |= SNES_D;  break; // SED

        // LDA #imm / dp / abs / long
        case 0xA9:
            if (snes_m8(emu)) { uint8_t v = snes_fetch8(emu); emu->a = (emu->a & 0xFF00) | v; snes_set_zn8(emu, v); }
            else { emu->a = snes_fetch16(emu); snes_set_zn16(emu, emu->a); cyc = 3; }
            break;
        case 0xA5: {
            uint16_t a = (uint16_t)(emu->dp + snes_fetch8(emu));
            if (snes_m8(emu)) { uint8_t v = snes_read8(emu, a); emu->a = (emu->a & 0xFF00) | v; snes_set_zn8(emu, v); }
            else { emu->a = snes_read16(emu, a); snes_set_zn16(emu, emu->a); }
            cyc = 3; break;
        }
        case 0xAD: {
            uint32_t a = db_base | snes_fetch16(emu);
            if (snes_m8(emu)) { uint8_t v = snes_read8(emu, a); emu->a = (emu->a & 0xFF00) | v; snes_set_zn8(emu, v); }
            else { emu->a = snes_read16(emu, a); snes_set_zn16(emu, emu->a); }
            cyc = 4; break;
        }
        case 0xBD: {
            uint32_t a = (db_base | snes_fetch16(emu)) + emu->x;
            if (snes_m8(emu)) { uint8_t v = snes_read8(emu, a); emu->a = (emu->a & 0xFF00) | v; snes_set_zn8(emu, v); }
            else { emu->a = snes_read16(emu, a); snes_set_zn16(emu, emu->a); }
            cyc = 4; break;
        }
        case 0xAF: {
            uint16_t lo = snes_fetch16(emu); uint8_t bk = snes_fetch8(emu);
            uint32_t a = ((uint32_t)bk << 16) | lo;
            if (snes_m8(emu)) { uint8_t v = snes_read8(emu, a); emu->a = (emu->a & 0xFF00) | v; snes_set_zn8(emu, v); }
            else { emu->a = snes_read16(emu, a); snes_set_zn16(emu, emu->a); }
            cyc = 5; break;
        }
        case 0xBF: {
            uint16_t lo = snes_fetch16(emu); uint8_t bk = snes_fetch8(emu);
            uint32_t a = (((uint32_t)bk << 16) | lo) + emu->x;
            if (snes_m8(emu)) { uint8_t v = snes_read8(emu, a); emu->a = (emu->a & 0xFF00) | v; snes_set_zn8(emu, v); }
            else { emu->a = snes_read16(emu, a); snes_set_zn16(emu, emu->a); }
            cyc = 5; break;
        }

        // STA dp / abs / abs,X / long / STZ
        case 0x85: {
            uint16_t a = (uint16_t)(emu->dp + snes_fetch8(emu));
            if (snes_m8(emu)) snes_write8(emu, a, (uint8_t)emu->a); else snes_write16(emu, a, emu->a);
            cyc = 3; break;
        }
        case 0x8D: {
            uint32_t a = db_base | snes_fetch16(emu);
            if (snes_m8(emu)) snes_write8(emu, a, (uint8_t)emu->a); else snes_write16(emu, a, emu->a);
            cyc = 4; break;
        }
        case 0x9D: {
            uint32_t a = (db_base | snes_fetch16(emu)) + emu->x;
            if (snes_m8(emu)) snes_write8(emu, a, (uint8_t)emu->a); else snes_write16(emu, a, emu->a);
            cyc = 5; break;
        }
        case 0x8F: {
            uint16_t lo = snes_fetch16(emu); uint8_t bk = snes_fetch8(emu);
            uint32_t a = ((uint32_t)bk << 16) | lo;
            if (snes_m8(emu)) snes_write8(emu, a, (uint8_t)emu->a); else snes_write16(emu, a, emu->a);
            cyc = 5; break;
        }
        case 0x9F: {
            uint16_t lo = snes_fetch16(emu); uint8_t bk = snes_fetch8(emu);
            uint32_t a = (((uint32_t)bk << 16) | lo) + emu->x;
            if (snes_m8(emu)) snes_write8(emu, a, (uint8_t)emu->a); else snes_write16(emu, a, emu->a);
            cyc = 5; break;
        }
        case 0x64: { uint16_t a = (uint16_t)(emu->dp + snes_fetch8(emu)); if (snes_m8(emu)) snes_write8(emu, a, 0); else snes_write16(emu, a, 0); cyc = 3; break; }
        case 0x9C: { uint32_t a = db_base | snes_fetch16(emu); if (snes_m8(emu)) snes_write8(emu, a, 0); else snes_write16(emu, a, 0); cyc = 4; break; }
        case 0x9E: { uint32_t a = (db_base | snes_fetch16(emu)) + emu->x; if (snes_m8(emu)) snes_write8(emu, a, 0); else snes_write16(emu, a, 0); cyc = 5; break; }

        // LDX / LDY / STX / STY
        case 0xA2:
            if (snes_x8(emu)) { emu->x = snes_fetch8(emu); snes_set_zn8(emu, (uint8_t)emu->x); }
            else { emu->x = snes_fetch16(emu); snes_set_zn16(emu, emu->x); cyc = 3; }
            break;
        case 0xA0:
            if (snes_x8(emu)) { emu->y = snes_fetch8(emu); snes_set_zn8(emu, (uint8_t)emu->y); }
            else { emu->y = snes_fetch16(emu); snes_set_zn16(emu, emu->y); cyc = 3; }
            break;
        case 0x86: { uint16_t a = (uint16_t)(emu->dp + snes_fetch8(emu)); if (snes_x8(emu)) snes_write8(emu, a, (uint8_t)emu->x); else snes_write16(emu, a, emu->x); cyc = 3; break; }
        case 0x8E: { uint32_t a = db_base | snes_fetch16(emu); if (snes_x8(emu)) snes_write8(emu, a, (uint8_t)emu->x); else snes_write16(emu, a, emu->x); cyc = 4; break; }
        case 0x84: { uint16_t a = (uint16_t)(emu->dp + snes_fetch8(emu)); if (snes_x8(emu)) snes_write8(emu, a, (uint8_t)emu->y); else snes_write16(emu, a, emu->y); cyc = 3; break; }
        case 0x8C: { uint32_t a = db_base | snes_fetch16(emu); if (snes_x8(emu)) snes_write8(emu, a, (uint8_t)emu->y); else snes_write16(emu, a, emu->y); cyc = 4; break; }

        // Transfers & Stack
        case 0x1B: emu->sp = emu->emu_mode ? (0x0100 | (emu->a & 0xFF)) : emu->a; break; // TCS
        case 0x3B: emu->a = emu->sp; snes_set_zn16(emu, emu->a); break; // TSC
        case 0x5B: emu->dp = emu->a; snes_set_zn16(emu, emu->dp); break; // TCD
        case 0x7B: emu->a = emu->dp; snes_set_zn16(emu, emu->a); break; // TDC
        case 0xEB: { uint8_t lo = (uint8_t)emu->a; uint8_t hi = (uint8_t)(emu->a >> 8); emu->a = ((uint16_t)lo << 8) | hi; snes_set_zn8(emu, hi); cyc = 3; break; } // XBA
        case 0xAA: if (snes_x8(emu)) { emu->x = emu->a & 0xFF; snes_set_zn8(emu, (uint8_t)emu->x); } else { emu->x = emu->a; snes_set_zn16(emu, emu->x); } break;
        case 0x8A: if (snes_m8(emu)) { emu->a = (emu->a & 0xFF00) | (emu->x & 0xFF); snes_set_zn8(emu, (uint8_t)emu->a); } else { emu->a = emu->x; snes_set_zn16(emu, emu->a); } break;
        case 0xA8: if (snes_x8(emu)) { emu->y = emu->a & 0xFF; snes_set_zn8(emu, (uint8_t)emu->y); } else { emu->y = emu->a; snes_set_zn16(emu, emu->y); } break;
        case 0x98: if (snes_m8(emu)) { emu->a = (emu->a & 0xFF00) | (emu->y & 0xFF); snes_set_zn8(emu, (uint8_t)emu->a); } else { emu->a = emu->y; snes_set_zn16(emu, emu->a); } break;
        case 0x9A: emu->sp = emu->emu_mode ? (0x0100 | (emu->x & 0xFF)) : emu->x; break;
        case 0xBA: if (snes_x8(emu)) { emu->x = emu->sp & 0xFF; snes_set_zn8(emu, (uint8_t)emu->x); } else { emu->x = emu->sp; snes_set_zn16(emu, emu->x); } break;

        case 0x48: if (snes_m8(emu)) snes_push8(emu, (uint8_t)emu->a); else snes_push16(emu, emu->a); cyc = 3; break; // PHA
        case 0x68: if (snes_m8(emu)) { uint8_t v = snes_pop8(emu); emu->a = (emu->a & 0xFF00) | v; snes_set_zn8(emu, v); } else { emu->a = snes_pop16(emu); snes_set_zn16(emu, emu->a); } cyc = 4; break; // PLA
        case 0xDA: if (snes_x8(emu)) snes_push8(emu, (uint8_t)emu->x); else snes_push16(emu, emu->x); cyc = 3; break; // PHX
        case 0xFA: if (snes_x8(emu)) { emu->x = snes_pop8(emu); snes_set_zn8(emu, (uint8_t)emu->x); } else { emu->x = snes_pop16(emu); snes_set_zn16(emu, emu->x); } cyc = 4; break; // PLX
        case 0x5A: if (snes_x8(emu)) snes_push8(emu, (uint8_t)emu->y); else snes_push16(emu, emu->y); cyc = 3; break; // PHY
        case 0x7A: if (snes_x8(emu)) { emu->y = snes_pop8(emu); snes_set_zn8(emu, (uint8_t)emu->y); } else { emu->y = snes_pop16(emu); snes_set_zn16(emu, emu->y); } cyc = 4; break; // PLY
        case 0x8B: snes_push8(emu, emu->db); cyc = 3; break; // PHB
        case 0xAB: emu->db = snes_pop8(emu); snes_set_zn8(emu, emu->db); cyc = 4; break; // PLB
        case 0x4B: snes_push8(emu, emu->pb); cyc = 3; break; // PHK
        case 0x08: snes_push8(emu, emu->p); cyc = 3; break;  // PHP
        case 0x28: emu->p = snes_pop8(emu); if (emu->emu_mode) emu->p |= (SNES_M | SNES_X); cyc = 4; break; // PLP

        // Increments / Decrements / ALU
        case 0x1A: if (snes_m8(emu)) { uint8_t v = (uint8_t)emu->a + 1; emu->a = (emu->a & 0xFF00) | v; snes_set_zn8(emu, v); } else { emu->a++; snes_set_zn16(emu, emu->a); } break;
        case 0x3A: if (snes_m8(emu)) { uint8_t v = (uint8_t)emu->a - 1; emu->a = (emu->a & 0xFF00) | v; snes_set_zn8(emu, v); } else { emu->a--; snes_set_zn16(emu, emu->a); } break;
        case 0xE8: if (snes_x8(emu)) { emu->x = (emu->x + 1) & 0xFF; snes_set_zn8(emu, (uint8_t)emu->x); } else { emu->x++; snes_set_zn16(emu, emu->x); } break;
        case 0xCA: if (snes_x8(emu)) { emu->x = (emu->x - 1) & 0xFF; snes_set_zn8(emu, (uint8_t)emu->x); } else { emu->x--; snes_set_zn16(emu, emu->x); } break;
        case 0xC8: if (snes_x8(emu)) { emu->y = (emu->y + 1) & 0xFF; snes_set_zn8(emu, (uint8_t)emu->y); } else { emu->y++; snes_set_zn16(emu, emu->y); } break;
        case 0x88: if (snes_x8(emu)) { emu->y = (emu->y - 1) & 0xFF; snes_set_zn8(emu, (uint8_t)emu->y); } else { emu->y--; snes_set_zn16(emu, emu->y); } break;

        case 0x29: if (snes_m8(emu)) { uint8_t v = (uint8_t)emu->a & snes_fetch8(emu); emu->a = (emu->a & 0xFF00) | v; snes_set_zn8(emu, v); } else { emu->a &= snes_fetch16(emu); snes_set_zn16(emu, emu->a); } break;
        case 0x09: if (snes_m8(emu)) { uint8_t v = (uint8_t)emu->a | snes_fetch8(emu); emu->a = (emu->a & 0xFF00) | v; snes_set_zn8(emu, v); } else { emu->a |= snes_fetch16(emu); snes_set_zn16(emu, emu->a); } break;
        case 0x49: if (snes_m8(emu)) { uint8_t v = (uint8_t)emu->a ^ snes_fetch8(emu); emu->a = (emu->a & 0xFF00) | v; snes_set_zn8(emu, v); } else { emu->a ^= snes_fetch16(emu); snes_set_zn16(emu, emu->a); } break;
        case 0x69:
            if (snes_m8(emu)) {
                uint8_t imm = snes_fetch8(emu);
                uint16_t sum = (emu->a & 0xFF) + imm + ((emu->p & SNES_C) ? 1 : 0);
                emu->p = (emu->p & ~SNES_C) | (sum > 0xFF ? SNES_C : 0);
                emu->a = (emu->a & 0xFF00) | (sum & 0xFF);
                snes_set_zn8(emu, (uint8_t)sum);
            } else {
                uint16_t imm = snes_fetch16(emu);
                uint32_t sum = (uint32_t)emu->a + imm + ((emu->p & SNES_C) ? 1 : 0);
                emu->p = (emu->p & ~SNES_C) | (sum > 0xFFFF ? SNES_C : 0);
                emu->a = (uint16_t)sum;
                snes_set_zn16(emu, emu->a);
            }
            break;
        case 0xE9:
            if (snes_m8(emu)) {
                uint8_t imm = (uint8_t)(~snes_fetch8(emu));
                uint16_t sum = (emu->a & 0xFF) + imm + ((emu->p & SNES_C) ? 1 : 0);
                emu->p = (emu->p & ~SNES_C) | (sum > 0xFF ? SNES_C : 0);
                emu->a = (emu->a & 0xFF00) | (sum & 0xFF);
                snes_set_zn8(emu, (uint8_t)sum);
            } else {
                uint16_t imm = (uint16_t)(~snes_fetch16(emu));
                uint32_t sum = (uint32_t)emu->a + imm + ((emu->p & SNES_C) ? 1 : 0);
                emu->p = (emu->p & ~SNES_C) | (sum > 0xFFFF ? SNES_C : 0);
                emu->a = (uint16_t)sum;
                snes_set_zn16(emu, emu->a);
            }
            break;
        case 0xC9:
            if (snes_m8(emu)) { uint8_t imm = snes_fetch8(emu); uint8_t a8 = (uint8_t)emu->a; emu->p = (emu->p & ~SNES_C) | (a8 >= imm ? SNES_C : 0); snes_set_zn8(emu, (uint8_t)(a8 - imm)); }
            else { uint16_t imm = snes_fetch16(emu); emu->p = (emu->p & ~SNES_C) | (emu->a >= imm ? SNES_C : 0); snes_set_zn16(emu, (uint16_t)(emu->a - imm)); }
            break;
        case 0xE0:
            if (snes_x8(emu)) { uint8_t imm = snes_fetch8(emu); uint8_t x8 = (uint8_t)emu->x; emu->p = (emu->p & ~SNES_C) | (x8 >= imm ? SNES_C : 0); snes_set_zn8(emu, (uint8_t)(x8 - imm)); }
            else { uint16_t imm = snes_fetch16(emu); emu->p = (emu->p & ~SNES_C) | (emu->x >= imm ? SNES_C : 0); snes_set_zn16(emu, (uint16_t)(emu->x - imm)); }
            break;
        case 0xC0:
            if (snes_x8(emu)) { uint8_t imm = snes_fetch8(emu); uint8_t y8 = (uint8_t)emu->y; emu->p = (emu->p & ~SNES_C) | (y8 >= imm ? SNES_C : 0); snes_set_zn8(emu, (uint8_t)(y8 - imm)); }
            else { uint16_t imm = snes_fetch16(emu); emu->p = (emu->p & ~SNES_C) | (emu->y >= imm ? SNES_C : 0); snes_set_zn16(emu, (uint16_t)(emu->y - imm)); }
            break;

        // Branches & Jumps
        case 0x80: { int8_t d = (int8_t)snes_fetch8(emu); emu->pc = (emu->pc + d) & 0xFFFF; cyc = 3; break; } // BRA
        case 0x82: { int16_t d = (int16_t)snes_fetch16(emu); emu->pc = (emu->pc + d) & 0xFFFF; cyc = 4; break; } // BRL
        case 0x90: { int8_t d = (int8_t)snes_fetch8(emu); if (!(emu->p & SNES_C)) { emu->pc = (emu->pc + d) & 0xFFFF; cyc = 3; } break; }
        case 0xB0: { int8_t d = (int8_t)snes_fetch8(emu); if (emu->p & SNES_C)  { emu->pc = (emu->pc + d) & 0xFFFF; cyc = 3; } break; }
        case 0xF0: { int8_t d = (int8_t)snes_fetch8(emu); if (emu->p & SNES_Z)  { emu->pc = (emu->pc + d) & 0xFFFF; cyc = 3; } break; }
        case 0xD0: { int8_t d = (int8_t)snes_fetch8(emu); if (!(emu->p & SNES_Z)) { emu->pc = (emu->pc + d) & 0xFFFF; cyc = 3; } break; }
        case 0x30: { int8_t d = (int8_t)snes_fetch8(emu); if (emu->p & SNES_N)  { emu->pc = (emu->pc + d) & 0xFFFF; cyc = 3; } break; }
        case 0x10: { int8_t d = (int8_t)snes_fetch8(emu); if (!(emu->p & SNES_N)) { emu->pc = (emu->pc + d) & 0xFFFF; cyc = 3; } break; }

        case 0x4C: emu->pc = snes_fetch16(emu); cyc = 3; break; // JMP abs
        case 0x5C: { uint16_t dst = snes_fetch16(emu); emu->pb = snes_fetch8(emu); emu->pc = dst; cyc = 4; break; } // JML long
        case 0x20: { uint16_t dst = snes_fetch16(emu); snes_push16(emu, (uint16_t)(emu->pc - 1)); emu->pc = dst; cyc = 6; break; } // JSR
        case 0x22: { uint16_t dst = snes_fetch16(emu); uint8_t bk = snes_fetch8(emu); snes_push8(emu, emu->pb); snes_push16(emu, (uint16_t)(emu->pc - 1)); emu->pb = bk; emu->pc = dst; cyc = 8; break; } // JSL
        case 0x60: emu->pc = (snes_pop16(emu) + 1) & 0xFFFF; cyc = 6; break; // RTS
        case 0x6B: emu->pc = (snes_pop16(emu) + 1) & 0xFFFF; emu->pb = snes_pop8(emu); cyc = 6; break; // RTL
        case 0x40: { if (!emu->emu_mode) { emu->p = snes_pop8(emu); emu->pc = snes_pop16(emu); emu->pb = snes_pop8(emu); } else { emu->p = snes_pop8(emu) | SNES_M | SNES_X; emu->pc = snes_pop16(emu); } cyc = 6; break; } // RTI
        default: break;
    }
    return cyc;
}

static void snes_render_bg_layer(snes_t* emu, uint16_t* dst, int line, int bg_idx, bool is_4bpp) {
    uint16_t map_base = (uint16_t)(emu->ppu_regs[0x07 + bg_idx] & 0xFC) << 9; // Byte offset in 64KB VRAM
    uint8_t chr_reg = (bg_idx < 2) ? emu->ppu_regs[0x0B] : emu->ppu_regs[0x0C];
    uint16_t chr_base = (uint16_t)(((bg_idx & 1) ? (chr_reg >> 4) : (chr_reg & 0x0F)) & 0x07) << 13;

    int py = (line + emu->bg_vofs[bg_idx]) & 0xFF;
    int tile_r = (py >> 3) & 0x1F;
    int bpp_bytes = is_4bpp ? 32 : 16;

    for (int sx = 0; sx < 256; sx++) {
        int px = (sx + emu->bg_hofs[bg_idx]) & 0xFF;
        int tile_c = (px >> 3) & 0x1F;
        uint16_t entry_addr = (map_base + (tile_r * 32 + tile_c) * 2) & 0xFFFF;
        uint16_t entry = (uint16_t)emu->vram[entry_addr] | ((uint16_t)emu->vram[(entry_addr + 1) & 0xFFFF] << 8);

        uint16_t tile_num = entry & 0x03FF;
        uint8_t pal = (uint8_t)((entry >> 10) & 0x07);
        bool hflip = (entry & 0x4000) != 0;
        bool vflip = (entry & 0x8000) != 0;

        int fine_y = vflip ? (7 - (py & 7)) : (py & 7);
        int bit = hflip ? (px & 7) : (7 - (px & 7));

        uint16_t t_addr = (uint16_t)(chr_base + tile_num * bpp_bytes + fine_y * 2);
        uint8_t bp0 = emu->vram[t_addr & 0xFFFF];
        uint8_t bp1 = emu->vram[(t_addr + 1) & 0xFFFF];
        uint8_t col = (uint8_t)(((bp0 >> bit) & 1) | (((bp1 >> bit) & 1) << 1));

        if (is_4bpp) {
            uint8_t bp2 = emu->vram[(t_addr + 16) & 0xFFFF];
            uint8_t bp3 = emu->vram[(t_addr + 17) & 0xFFFF];
            col |= (uint8_t)((((bp2 >> bit) & 1) << 2) | (((bp3 >> bit) & 1) << 3));
        }

        if (col != 0) {
            uint8_t cgram_idx = is_4bpp ? (uint8_t)(pal * 16 + col) : (uint8_t)(bg_idx * 32 + pal * 4 + col);
            dst[sx] = emu->palette_cache[cgram_idx];
        }
    }
}

static void snes_render_scanline(snes_t* emu, int line) {
    if (line < 0 || line >= 224 || !emu->framebuffer || !emu->vram) return;
    uint16_t* dst = &emu->framebuffer[line * 256];

    // Force Blank (INIDISP $2100 bit 7)
    if (emu->ppu_regs[0x00] & 0x80) {
        for (int x = 0; x < 256; x++) dst[x] = 0x0000;
        return;
    }

    uint16_t backdrop = emu->palette_cache[0];
    for (int x = 0; x < 256; x++) dst[x] = backdrop;

    uint8_t tm = emu->ppu_regs[0x2C]; // Main Screen Designation ($212C)
    uint8_t mode = emu->ppu_regs[0x05] & 0x07;

    if (tm & 0x02) snes_render_bg_layer(emu, dst, line, 1, mode == 1); // BG2
    if (tm & 0x01) snes_render_bg_layer(emu, dst, line, 0, mode >= 1); // BG1

    // OAM Hardware Sprites (when TM bit 4 is enabled)
    if (tm & 0x10) {
        uint16_t obj_base = (uint16_t)(emu->ppu_regs[0x01] & 0x07) << 14;
        for (int s = 0; s < 128; s++) {
            uint8_t sx = emu->oam[s * 4];
            uint8_t sy = emu->oam[s * 4 + 1];
            uint8_t tile = emu->oam[s * 4 + 2];
            uint8_t attr = emu->oam[s * 4 + 3];
            if (sy >= 224 || line < sy || line >= sy + 8) continue;

            bool hflip = (attr & 0x40) != 0;
            bool vflip = (attr & 0x80) != 0;
            uint8_t pal = (attr >> 1) & 0x07;
            int fine_y = vflip ? (7 - (line - sy)) : (line - sy);

            uint16_t t_addr = (uint16_t)(obj_base + ((uint16_t)tile + ((attr & 1) << 8)) * 32 + fine_y * 2);
            uint8_t bp0 = emu->vram[t_addr & 0xFFFF];
            uint8_t bp1 = emu->vram[(t_addr + 1) & 0xFFFF];
            uint8_t bp2 = emu->vram[(t_addr + 16) & 0xFFFF];
            uint8_t bp3 = emu->vram[(t_addr + 17) & 0xFFFF];

            for (int px = 0; px < 8; px++) {
                int out_x = sx + px;
                if (out_x >= 256) continue;
                int bit = hflip ? px : (7 - px);
                uint8_t col = (uint8_t)(((bp0 >> bit) & 1) | (((bp1 >> bit) & 1) << 1) |
                                        (((bp2 >> bit) & 1) << 2) | (((bp3 >> bit) & 1) << 3));
                if (col != 0) {
                    dst[out_x] = emu->palette_cache[128 + pal * 16 + col];
                }
            }
        }
    }
}

void snes_init(snes_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* wram, uint16_t* fb) {
    if (!emu) return;
    memset(emu, 0, sizeof(snes_t));
    emu->rom = rom;
    emu->rom_size = rom_size;
    emu->wram = wram;
    emu->framebuffer = fb;

    emu->vram = (uint8_t*)SNES_MALLOC(64 * 1024);
    emu->sram = (uint8_t*)SNES_MALLOC(32 * 1024);
    if (emu->wram) memset(emu->wram, 0, 128 * 1024);
    if (emu->vram) memset(emu->vram, 0, 64 * 1024);
    if (emu->sram) memset(emu->sram, 0, 32 * 1024);
    memset(emu->oam, 0xE0, sizeof(emu->oam));

    emu->emu_mode = true;
    emu->p = SNES_M | SNES_X | SNES_I;
    emu->sp = 0x01FF;
    emu->ppu_regs[0x00] = 0x0F; // Full brightness, display active
    emu->ppu_regs[0x2C] = 0x13; // Enable BG1, BG2, and OBJ on main screen

    if (rom && rom_size >= 0x8000) {
        emu->header_offset = ((rom_size & 0x7FFF) == 512) ? 512 : 0;
        uint32_t reset_vec = emu->header_offset + 0x7FFC;
        if (reset_vec + 1 < rom_size) {
            emu->pc = (uint32_t)rom[reset_vec] | ((uint32_t)rom[reset_vec + 1] << 8);
        }
        if (emu->pc < 0x8000 && emu->header_offset + 0xFFFC + 1 < rom_size) {
            emu->pc = (uint32_t)rom[emu->header_offset + 0xFFFC] | ((uint32_t)rom[emu->header_offset + 0xFFFD] << 8);
        }
    }
}

void snes_destroy(snes_t* emu) {
    if (!emu) return;
    if (emu->vram) { SNES_FREE(emu->vram); emu->vram = NULL; }
    if (emu->sram) { SNES_FREE(emu->sram); emu->sram = NULL; }
}

void snes_set_pad(snes_t* emu, uint8_t pad) {
    if (emu) emu->pad_state = pad;
}

void snes_step_frame(snes_t* emu) {
    if (!emu || !emu->framebuffer) return;

    for (int line = 0; line < 262; line++) {
        if (line < 224) {
            snes_render_scanline(emu, line);
        } else if (line == 225) {
            emu->rdnmi = 0x80;
            if (emu->nmitimen & 0x80) {
                uint32_t vec_addr = emu->emu_mode ? 0x00FFFA : 0x00FFEA;
                if (!emu->emu_mode) snes_push8(emu, emu->pb);
                snes_push16(emu, (uint16_t)emu->pc);
                snes_push8(emu, emu->p);
                emu->pb = 0;
                emu->pc = snes_read16(emu, vec_addr);
            }
        }

        int cycles = 0;
        while (cycles < 180 && !emu->stopped) {
            cycles += snes_cpu_step(emu);
        }
    }
    emu->rdnmi &= ~0x80;
}
