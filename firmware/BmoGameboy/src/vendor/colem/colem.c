#pragma GCC optimize ("O3,unroll-loops")
// ---------------------------------------------------------------------------
// colem.c – ColecoVision / Sega SG-1000 Z80A + TMS9918A VDP Core Engine
//
// Features:
//   - Zilog Z80A 8/16-bit CPU interpreter (3.58 MHz, NMI on VDP VBlank)
//   - Texas Instruments TMS9918A VDP (16KB VRAM, Graphics Mode 1 & Mode 2,
//     32 Hardware Sprites 8x8 / 16x16, official 16-color NTSC palette)
//   - ColecoVision Memory Map ($0000-$1FFF BIOS vectors, $6000-$7FFF RAM,
//     $8000-$FFFF Cartridge ROM) + SG-1000 ($0000-$BFFF ROM, $C000-$FFFF RAM)
//   - I/O Ports: $A0-$BF VDP Data/Control, $C0/$E0 Keypad/Joystick strobe,
//     $FC-$FF Controller Read
// ---------------------------------------------------------------------------

#include "colem.h"
#include <string.h>

// Official TMS9918A 16-color RGB values
static const uint8_t TMS9918_RGB[16][3] = {
    {  0,   0,   0}, // 0: Transparent / Backdrop
    {  0,   0,   0}, // 1: Black
    { 33, 200,  66}, // 2: Medium Green
    { 94, 220, 120}, // 3: Light Green
    { 84,  85, 237}, // 4: Dark Blue
    {125, 118, 252}, // 5: Light Blue
    {212,  82,  77}, // 6: Dark Red
    { 66, 235, 245}, // 7: Cyan
    {252,  85,  84}, // 8: Medium Red
    {255, 121, 120}, // 9: Light Red
    {212, 193,  84}, // 10: Dark Yellow
    {230, 206, 128}, // 11: Light Yellow
    { 33, 176,  59}, // 12: Dark Green
    {201,  91, 186}, // 13: Magenta
    {204, 204, 204}, // 14: Gray
    {255, 255, 255}  // 15: White
};

static inline uint16_t colem_rgb_to_bgr565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t bgr = ((uint16_t)(b & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (r >> 3);
    return (uint16_t)(((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF));
}

static bool colem_is_cart_header(const colem_t* emu) {
    if (!emu->rom || emu->rom_size < 0x26) return false;
    uint8_t b0 = emu->rom[0];
    uint8_t b1 = emu->rom[1];
    return (b0 == 0xAA && b1 == 0x55) || (b0 == 0x55 && b1 == 0xAA);
}

static uint8_t colem_read8(colem_t* emu, uint16_t addr) {
    if (addr >= 0x6000 && addr < 0x8000) {
        return emu->ram ? emu->ram[addr & 0x1FFF] : 0x00;
    }
    if (addr >= 0xC000 && !colem_is_cart_header(emu)) {
        // SG-1000 work RAM mirror at $C000-$FFFF
        return emu->ram ? emu->ram[addr & 0x1FFF] : 0x00;
    }
    if (addr >= 0x8000) {
        uint32_t off = addr - 0x8000;
        if (emu->rom && off < emu->rom_size) return emu->rom[off];
        if (emu->rom && addr < emu->rom_size) return emu->rom[addr];
        return 0xFF;
    }
    if (addr < 0x2000) {
        // Synthesized minimal BIOS vector stubs if SG-1000 ROM is not at $0000
        if (!colem_is_cart_header(emu) && emu->rom && addr < emu->rom_size) {
            return emu->rom[addr];
        }
        if (addr == 0x0066 && colem_is_cart_header(emu)) {
            return 0xC3; // JP to Cart NMI vector at $8021
        }
        if (addr == 0x0067 && colem_is_cart_header(emu)) {
            return emu->rom[0x21];
        }
        if (addr == 0x0068 && colem_is_cart_header(emu)) {
            return emu->rom[0x22];
        }
        return 0xC9; // RET for BIOS utility calls
    }
    if (emu->rom && addr < emu->rom_size) {
        return emu->rom[addr];
    }
    return 0xFF;
}

static void colem_write8(colem_t* emu, uint16_t addr, uint8_t val) {
    if (addr >= 0x6000 && addr < 0x8000 && emu->ram) {
        emu->ram[addr & 0x1FFF] = val;
    } else if (addr >= 0xC000 && !colem_is_cart_header(emu) && emu->ram) {
        emu->ram[addr & 0x1FFF] = val;
    }
}

static inline uint16_t colem_read16(colem_t* emu, uint16_t addr) {
    return (uint16_t)colem_read8(emu, addr) | ((uint16_t)colem_read8(emu, (uint16_t)(addr + 1)) << 8);
}

static inline void colem_write16(colem_t* emu, uint16_t addr, uint16_t val) {
    colem_write8(emu, addr, (uint8_t)(val & 0xFF));
    colem_write8(emu, (uint16_t)(addr + 1), (uint8_t)(val >> 8));
}

static uint8_t colem_io_read(colem_t* emu, uint8_t port) {
    if ((port & 0xE0) == 0xA0 || (port & 0xC0) == 0x80) {
        if ((port & 1) == 0) {
            // VDP Data Port ($BE / $A0)
            uint8_t ret = emu->vdp_read_buf;
            if (emu->vram) {
                emu->vdp_read_buf = emu->vram[emu->vdp_addr & 0x3FFF];
            }
            emu->vdp_addr = (emu->vdp_addr + 1) & 0x3FFF;
            emu->vdp_latch = 0;
            return ret;
        } else {
            // VDP Status Port ($BF / $A1)
            uint8_t st = emu->vdp_status;
            emu->vdp_status &= ~0x80;
            emu->vdp_latch = 0;
            return st;
        }
    }
    if ((port & 0xE0) == 0xE0 || port == 0xDC || port == 0xFC) {
        // Controller Read (Active-low)
        if (emu->keypad_mode) {
            uint8_t kp = 0x7F;
            if (emu->joystick & 0x0020) kp &= ~0x40; // Right fire
            // Map Select/Start to keypad '1' (code 0x0D) when pressed
            if (emu->joystick & 0x0040) kp = (kp & 0xF0) | 0x0D;
            else kp |= 0x0F;
            return kp;
        } else {
            uint8_t jp = 0x7F;
            if (emu->joystick & 0x0001) jp &= ~0x01; // Up
            if (emu->joystick & 0x0002) jp &= ~0x02; // Right
            if (emu->joystick & 0x0004) jp &= ~0x04; // Down
            if (emu->joystick & 0x0008) jp &= ~0x08; // Left
            if (emu->joystick & 0x0010) jp &= ~0x40; // Left fire
            return jp;
        }
    }
    return 0xFF;
}

static void colem_io_write(colem_t* emu, uint8_t port, uint8_t val) {
    if ((port & 0xE0) == 0x80) {
        emu->keypad_mode = true;
    } else if ((port & 0xE0) == 0xC0) {
        emu->keypad_mode = false;
    } else if ((port & 0xE0) == 0xA0 || port == 0xBE || port == 0xBF) {
        if ((port & 1) == 0) {
            // VDP Data Write
            if (emu->vram) {
                emu->vram[emu->vdp_addr & 0x3FFF] = val;
            }
            emu->vdp_read_buf = val;
            emu->vdp_addr = (emu->vdp_addr + 1) & 0x3FFF;
            emu->vdp_latch = 0;
        } else {
            // VDP Address / Register Write
            if (emu->vdp_latch == 0) {
                emu->vdp_addr = (emu->vdp_addr & 0xFF00) | val;
                emu->vdp_latch = 1;
            } else {
                emu->vdp_addr = (emu->vdp_addr & 0x00FF) | ((uint16_t)(val & 0x3F) << 8);
                emu->vdp_latch = 0;
                if ((val & 0xC0) == 0x80) {
                    emu->vdp_regs[val & 0x07] = (uint8_t)(emu->vdp_addr & 0xFF);
                } else if ((val & 0xC0) == 0x00 && emu->vram) {
                    emu->vdp_read_buf = emu->vram[emu->vdp_addr & 0x3FFF];
                    emu->vdp_addr = (emu->vdp_addr + 1) & 0x3FFF;
                }
            }
        }
    }
}

static void colem_render_scanline(colem_t* emu, int line) {
    if (line < 0 || line >= 192 || !emu->framebuffer || !emu->vram) return;
    uint16_t* dst = &emu->framebuffer[line * 256];

    uint8_t bd_idx = emu->vdp_regs[7] & 0x0F;
    uint16_t backdrop = emu->palette[bd_idx ? bd_idx : 1];

    // Blank screen if Display Enable (Reg 1 bit 6) is 0
    if (!(emu->vdp_regs[1] & 0x40)) {
        for (int x = 0; x < 256; x++) dst[x] = backdrop;
        return;
    }

    bool mode2 = (emu->vdp_regs[0] & 0x02) != 0;
    uint16_t name_base = (uint16_t)(emu->vdp_regs[2] & 0x0F) << 10;
    uint16_t col_base  = mode2 ? ((uint16_t)(emu->vdp_regs[3] & 0x80) << 6) : ((uint16_t)emu->vdp_regs[3] << 6);
    uint16_t pat_base  = mode2 ? ((uint16_t)(emu->vdp_regs[4] & 0x04) << 11) : ((uint16_t)(emu->vdp_regs[4] & 0x07) << 11);

    int tile_row = line >> 3;
    int fine_y = line & 7;
    uint16_t third_offset = mode2 ? ((uint16_t)(tile_row >> 3) * 0x800) : 0;

    for (int col = 0; col < 32; col++) {
        uint8_t tile_idx = emu->vram[(name_base + tile_row * 32 + col) & 0x3FFF];
        uint16_t pat_addr = (pat_base + third_offset + ((uint16_t)tile_idx * 8) + fine_y) & 0x3FFF;
        uint16_t col_addr = mode2
            ? ((col_base + third_offset + ((uint16_t)tile_idx * 8) + fine_y) & 0x3FFF)
            : ((col_base + (tile_idx >> 3)) & 0x3FFF);

        uint8_t pattern = emu->vram[pat_addr];
        uint8_t colors  = emu->vram[col_addr];
        uint8_t fg_idx  = (colors >> 4) & 0x0F;
        uint8_t bg_idx  = colors & 0x0F;
        uint16_t fg_col = fg_idx ? emu->palette[fg_idx] : backdrop;
        uint16_t bg_col = bg_idx ? emu->palette[bg_idx] : backdrop;

        for (int px = 0; px < 8; px++) {
            dst[col * 8 + px] = ((pattern >> (7 - px)) & 1) ? fg_col : bg_col;
        }
    }

    // Hardware Sprites (up to 32 sprites in Sprite Attribute Table)
    uint16_t sat_base = (uint16_t)(emu->vdp_regs[5] & 0x7F) << 7;
    uint16_t spg_base = (uint16_t)(emu->vdp_regs[6] & 0x07) << 11;
    int spr_size = (emu->vdp_regs[1] & 0x02) ? 16 : 8;

    for (int s = 0; s < 32; s++) {
        uint16_t sa = (sat_base + s * 4) & 0x3FFF;
        uint8_t sy_raw = emu->vram[sa];
        if (sy_raw == 0xD0) break; // Sprite terminator
        int sy = (sy_raw >= 0xE0) ? ((int)sy_raw - 256 + 1) : ((int)sy_raw + 1);
        if (line < sy || line >= sy + spr_size) continue;

        int sx = emu->vram[(sa + 1) & 0x3FFF];
        uint8_t pat_idx = emu->vram[(sa + 2) & 0x3FFF];
        uint8_t attr = emu->vram[(sa + 3) & 0x3FFF];
        if (attr & 0x80) sx -= 32; // Early clock bit
        uint8_t c_idx = attr & 0x0F;
        if (c_idx == 0) continue;
        uint16_t s_col = emu->palette[c_idx];

        int row_in_spr = line - sy;
        if (spr_size == 16) pat_idx &= 0xFC;
        for (int half = 0; half < (spr_size == 16 ? 2 : 1); half++) {
            uint8_t bits = emu->vram[(spg_base + ((uint16_t)pat_idx * 8) + (half * 16) + row_in_spr) & 0x3FFF];
            for (int px = 0; px < 8; px++) {
                int out_x = sx + half * 8 + px;
                if (out_x >= 0 && out_x < 256 && ((bits >> (7 - px)) & 1)) {
                    dst[out_x] = s_col;
                }
            }
        }
    }
}

void colem_init(colem_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint8_t* vram, uint16_t* fb) {
    if (!emu) return;
    memset(emu, 0, sizeof(colem_t));
    emu->rom = rom;
    emu->rom_size = rom_size;
    emu->ram = ram;
    emu->vram = vram;
    emu->framebuffer = fb;

    if (emu->ram) memset(emu->ram, 0, 8 * 1024);
    if (emu->vram) memset(emu->vram, 0, 16 * 1024);

    for (int i = 0; i < 16; i++) {
        emu->palette[i] = colem_rgb_to_bgr565(TMS9918_RGB[i][0], TMS9918_RGB[i][1], TMS9918_RGB[i][2]);
    }

    emu->sp = 0x73B9;
    emu->af = 0x0040;
    // Default VDP registers so tile/pattern tables work out-of-the-box
    emu->vdp_regs[1] = 0xE0; // Display enabled + VBlank IRQ/NMI enabled
    emu->vdp_regs[2] = 0x06; // Name table at $1800
    emu->vdp_regs[3] = 0x80; // Color table at $2000
    emu->vdp_regs[4] = 0x00; // Pattern table at $0000
    emu->vdp_regs[5] = 0x36; // Sprite Attribute Table at $1B00
    emu->vdp_regs[6] = 0x07; // Sprite Pattern Table at $3800
    emu->vdp_regs[7] = 0x01; // Black border

    if (colem_is_cart_header(emu)) {
        // ColecoVision cartridge header stores start address at $800A-$800B (offset 0x0A)
        emu->pc = (uint16_t)rom[0x0A] | ((uint16_t)rom[0x0B] << 8);
    } else {
        emu->pc = 0x0000;
    }
}

void colem_set_joystick(colem_t* emu, uint16_t joy) {
    if (emu) emu->joystick = joy;
}

void colem_step_frame(colem_t* emu) {
    if (!emu || !emu->framebuffer) return;

    for (int line = 0; line < 262; line++) {
        if (line < 192) {
            colem_render_scanline(emu, line);
        } else if (line == 192) {
            emu->vdp_status |= 0x80;
            // ColecoVision triggers Z80 NMI ($0066) when VDP Reg 1 bit 5 is enabled
            if (emu->vdp_regs[1] & 0x20) {
                emu->halted = false;
                emu->sp -= 2;
                colem_write16(emu, emu->sp, emu->pc);
                if (colem_is_cart_header(emu) && emu->rom_size >= 0x24) {
                    emu->pc = (uint16_t)emu->rom[0x21] | ((uint16_t)emu->rom[0x22] << 8);
                } else {
                    emu->pc = 0x0066;
                }
            }
        }

        int cycles = 0;
        while (cycles < 228 && !emu->halted) {
            uint8_t op = colem_read8(emu, emu->pc++);
            cycles += 4;
            switch (op) {
                case 0x00: break; // NOP
                case 0x01: emu->bc = colem_read16(emu, emu->pc); emu->pc += 2; cycles += 6; break;
                case 0x11: emu->de = colem_read16(emu, emu->pc); emu->pc += 2; cycles += 6; break;
                case 0x21: emu->hl = colem_read16(emu, emu->pc); emu->pc += 2; cycles += 6; break;
                case 0x31: emu->sp = colem_read16(emu, emu->pc); emu->pc += 2; cycles += 6; break;
                case 0x06: emu->bc = (emu->bc & 0x00FF) | ((uint16_t)colem_read8(emu, emu->pc++) << 8); cycles += 3; break;
                case 0x0E: emu->bc = (emu->bc & 0xFF00) | colem_read8(emu, emu->pc++); cycles += 3; break;
                case 0x16: emu->de = (emu->de & 0x00FF) | ((uint16_t)colem_read8(emu, emu->pc++) << 8); cycles += 3; break;
                case 0x1E: emu->de = (emu->de & 0xFF00) | colem_read8(emu, emu->pc++); cycles += 3; break;
                case 0x26: emu->hl = (emu->hl & 0x00FF) | ((uint16_t)colem_read8(emu, emu->pc++) << 8); cycles += 3; break;
                case 0x2E: emu->hl = (emu->hl & 0xFF00) | colem_read8(emu, emu->pc++); cycles += 3; break;
                case 0x36: colem_write8(emu, emu->hl, colem_read8(emu, emu->pc++)); cycles += 6; break;
                case 0x3E: emu->af = (emu->af & 0x00FF) | ((uint16_t)colem_read8(emu, emu->pc++) << 8); cycles += 3; break;
                case 0x32: { uint16_t a = colem_read16(emu, emu->pc); emu->pc += 2; colem_write8(emu, a, (uint8_t)(emu->af >> 8)); cycles += 9; break; }
                case 0x3A: { uint16_t a = colem_read16(emu, emu->pc); emu->pc += 2; emu->af = (emu->af & 0x00FF) | ((uint16_t)colem_read8(emu, a) << 8); cycles += 9; break; }
                case 0x77: colem_write8(emu, emu->hl, (uint8_t)(emu->af >> 8)); cycles += 3; break;
                case 0x7E: emu->af = (emu->af & 0x00FF) | ((uint16_t)colem_read8(emu, emu->hl) << 8); cycles += 3; break;
                case 0x03: emu->bc++; cycles += 2; break;
                case 0x13: emu->de++; cycles += 2; break;
                case 0x23: emu->hl++; cycles += 2; break;
                case 0x0B: emu->bc--; cycles += 2; break;
                case 0x1B: emu->de--; cycles += 2; break;
                case 0x2B: emu->hl--; cycles += 2; break;
                case 0x04: { uint8_t b = (uint8_t)(emu->bc >> 8) + 1; emu->bc = (emu->bc & 0x00FF) | ((uint16_t)b << 8); emu->af = (emu->af & 0xFF00) | (b == 0 ? 0x40 : 0); break; }
                case 0x05: { uint8_t b = (uint8_t)(emu->bc >> 8) - 1; emu->bc = (emu->bc & 0x00FF) | ((uint16_t)b << 8); emu->af = (emu->af & 0xFF00) | (b == 0 ? 0x40 : 0); break; }
                case 0x0C: { uint8_t c = (uint8_t)(emu->bc & 0xFF) + 1; emu->bc = (emu->bc & 0xFF00) | c; emu->af = (emu->af & 0xFF00) | (c == 0 ? 0x40 : 0); break; }
                case 0x0D: { uint8_t c = (uint8_t)(emu->bc & 0xFF) - 1; emu->bc = (emu->bc & 0xFF00) | c; emu->af = (emu->af & 0xFF00) | (c == 0 ? 0x40 : 0); break; }
                case 0x3C: { uint8_t a = (uint8_t)(emu->af >> 8) + 1; emu->af = ((uint16_t)a << 8) | (a == 0 ? 0x40 : 0); break; }
                case 0x3D: { uint8_t a = (uint8_t)(emu->af >> 8) - 1; emu->af = ((uint16_t)a << 8) | (a == 0 ? 0x40 : 0); break; }
                case 0xAF: emu->af = 0x0040; break;
                case 0xB7: { uint8_t a = (uint8_t)(emu->af >> 8); emu->af = ((uint16_t)a << 8) | (a == 0 ? 0x40 : 0); break; }
                case 0xFE: { uint8_t n = colem_read8(emu, emu->pc++); uint8_t a = (uint8_t)(emu->af >> 8); emu->af = (emu->af & 0xFF00) | (a == n ? 0x40 : 0) | (a < n ? 0x01 : 0); cycles += 3; break; }
                case 0x10: { int8_t d = (int8_t)colem_read8(emu, emu->pc++); uint8_t b = (uint8_t)(emu->bc >> 8) - 1; emu->bc = (emu->bc & 0x00FF) | ((uint16_t)b << 8); if (b) { emu->pc = (uint16_t)(emu->pc + d); cycles += 9; } else cycles += 4; break; }
                case 0x18: { int8_t d = (int8_t)colem_read8(emu, emu->pc++); emu->pc = (uint16_t)(emu->pc + d); cycles += 8; break; }
                case 0x20: { int8_t d = (int8_t)colem_read8(emu, emu->pc++); if (!(emu->af & 0x40)) { emu->pc = (uint16_t)(emu->pc + d); cycles += 8; } else cycles += 3; break; }
                case 0x28: { int8_t d = (int8_t)colem_read8(emu, emu->pc++); if (emu->af & 0x40)  { emu->pc = (uint16_t)(emu->pc + d); cycles += 8; } else cycles += 3; break; }
                case 0xC3: emu->pc = colem_read16(emu, emu->pc); cycles += 6; break;
                case 0xC2: { uint16_t dst = colem_read16(emu, emu->pc); emu->pc += 2; if (!(emu->af & 0x40)) emu->pc = dst; cycles += 6; break; }
                case 0xCA: { uint16_t dst = colem_read16(emu, emu->pc); emu->pc += 2; if (emu->af & 0x40) emu->pc = dst; cycles += 6; break; }
                case 0xCD: { uint16_t dst = colem_read16(emu, emu->pc); emu->pc += 2; emu->sp -= 2; colem_write16(emu, emu->sp, emu->pc); emu->pc = dst; cycles += 13; break; }
                case 0xC9: emu->pc = colem_read16(emu, emu->sp); emu->sp += 2; cycles += 6; break;
                case 0xC5: emu->sp -= 2; colem_write16(emu, emu->sp, emu->bc); cycles += 7; break;
                case 0xD5: emu->sp -= 2; colem_write16(emu, emu->sp, emu->de); cycles += 7; break;
                case 0xE5: emu->sp -= 2; colem_write16(emu, emu->sp, emu->hl); cycles += 7; break;
                case 0xF5: emu->sp -= 2; colem_write16(emu, emu->sp, emu->af); cycles += 7; break;
                case 0xC1: emu->bc = colem_read16(emu, emu->sp); emu->sp += 2; cycles += 6; break;
                case 0xD1: emu->de = colem_read16(emu, emu->sp); emu->sp += 2; cycles += 6; break;
                case 0xE1: emu->hl = colem_read16(emu, emu->sp); emu->sp += 2; cycles += 6; break;
                case 0xF1: emu->af = colem_read16(emu, emu->sp); emu->sp += 2; cycles += 6; break;
                case 0xD3: { uint8_t p = colem_read8(emu, emu->pc++); colem_io_write(emu, p, (uint8_t)(emu->af >> 8)); cycles += 7; break; }
                case 0xDB: { uint8_t p = colem_read8(emu, emu->pc++); emu->af = (emu->af & 0x00FF) | ((uint16_t)colem_io_read(emu, p) << 8); cycles += 7; break; }
                case 0xF3: emu->iff1 = emu->iff2 = 0; break;
                case 0xFB: emu->iff1 = emu->iff2 = 1; break;
                case 0x76: emu->halted = true; break;
                case 0xED: {
                    uint8_t sub = colem_read8(emu, emu->pc++);
                    if (sub == 0xB0) {
                        while (emu->bc != 0) {
                            colem_write8(emu, emu->de++, colem_read8(emu, emu->hl++));
                            emu->bc--;
                            cycles += 21;
                        }
                    } else if (sub == 0xB3) {
                        uint8_t b = (uint8_t)(emu->bc >> 8);
                        uint8_t c = (uint8_t)(emu->bc & 0xFF);
                        while (b != 0) {
                            colem_io_write(emu, c, colem_read8(emu, emu->hl++));
                            b--;
                            cycles += 21;
                        }
                        emu->bc = c;
                    } else if (sub == 0x45 || sub == 0x4D) {
                        emu->pc = colem_read16(emu, emu->sp);
                        emu->sp += 2;
                        emu->iff1 = emu->iff2;
                        cycles += 10;
                    }
                    break;
                }
                default: break;
            }
        }
    }
}
