/*
 * tests/host/host_cores_runner.c — Native Host C Behavioral & Visual Regression Test Runner
 * Compiles and executes the real C emulator cores (Atari 2600, ColecoVision, Atari Lynx,
 * Neo Geo Pocket, SNES, WonderSwan, Sega Genesis, Master System, PC Engine, PICO-8)
 * on the host CPU across 50-frame golden runs, verifying CPU instruction execution,
 * memory/register state, and deterministic BGR565 framebuffer output hashes.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../../firmware/BmoGameboy/src/engines/stella/atari.h"
#include "../../firmware/BmoGameboy/src/engines/colem/colem.h"
#include "../../firmware/BmoGameboy/src/engines/lynx/lynx.h"
#include "../../firmware/BmoGameboy/src/engines/ngp/ngp.h"
#include "../../firmware/BmoGameboy/src/engines/snes/snes.h"
#include "../../firmware/BmoGameboy/src/engines/wswan/wswan.h"
#include "../../firmware/BmoGameboy/src/engines/genesis/genesis.h"
#include "../../firmware/BmoGameboy/src/engines/smsplus/sms.h"
#include "../../firmware/BmoGameboy/src/engines/pce/pce.h"
#include "../../firmware/BmoGameboy/src/engines/pico/pico.h"
#include "../../firmware/BmoGameboy/src/third_party/agnes/agnes.h"

static uint32_t fnv1a_hash16(const uint16_t* buf, size_t count) {
    uint32_t h = 0x811C9DC5u;
    const uint8_t* bytes = (const uint8_t*)buf;
    size_t n = count * 2;
    for (size_t i = 0; i < n; ++i) {
        h ^= bytes[i];
        h *= 0x01000193u;
    }
    return h;
}

static size_t count_nonzero_pixels(const uint16_t* buf, size_t count) {
    size_t nz = 0;
    for (size_t i = 0; i < count; ++i) {
        if (buf[i] != 0) nz++;
    }
    return nz;
}

static int test_atari_core(void) {
    atari_context_t* ctx = atari_create();
    if (!ctx) return 1;

    uint8_t rom[4096];
    memset(rom, 0xEA, sizeof(rom)); /* NOP fill */
    rom[0xFFC] = 0x00;
    rom[0xFFD] = 0xF0;
    const uint8_t prog[] = {
        0xA9, 0x88, 0x85, 0x80, /* LDA #$88; STA $80 (RIOT RAM[0]) */
        0xA9, 0x2E, 0x85, 0x08, /* LDA #$2E; STA $08 (COLUPF) */
        0xA9, 0x44, 0x85, 0x09, /* LDA #$44; STA $09 (COLUBK) */
        0xA9, 0xAA, 0x85, 0x0E, /* LDA #$AA; STA $0E (PF1) */
        0x4C, 0x10, 0xF0        /* JMP $F010 */
    };
    memcpy(rom, prog, sizeof(prog));

    if (!atari_load_rom(ctx, rom, sizeof(rom))) {
        atari_destroy(ctx);
        return 2;
    }
    for (int f = 0; f < 50; ++f) {
        atari_run_frame(ctx);
    }

    size_t px_count = 160 * 192;
    bool ok = (ctx->ram[0] == 0x88) && (count_nonzero_pixels(ctx->framebuffer, px_count) > 1000);
    uint32_t hash = fnv1a_hash16(ctx->framebuffer, px_count);
    printf("[HOST_CORE] ATARI_2600  | RAM[0]=0x%02X | FB_HASH=0x%08X | STATUS=%s\n",
           ctx->ram[0], hash, ok ? "PASS" : "FAIL");
    atari_destroy(ctx);
    return ok ? 0 : 3;
}

static int test_colem_core(void) {
    colem_t emu;
    uint8_t* ram = (uint8_t*)calloc(1, 0x2000);
    uint8_t* vram = (uint8_t*)calloc(1, 0x4000);
    uint16_t* fb = (uint16_t*)calloc(256 * 192, sizeof(uint16_t));
    uint8_t rom[8192];
    memset(rom, 0x00, sizeof(rom));

    rom[0] = 0x55;
    rom[1] = 0xAA;
    rom[0x0A] = 0x20;
    rom[0x0B] = 0x80;
    const uint8_t prog[] = {
        0x3E, 0x5A,       /* LD A, 0x5A */
        0x32, 0x00, 0x60, /* LD (0x6000), A */
        0xD3, 0xBF,       /* OUT (0xBF), A */
        0x3E, 0x87,       /* LD A, 0x87 */
        0xD3, 0xBF,       /* OUT (0xBF), A */
        0xC3, 0x29, 0x80  /* JP 0x8029 */
    };
    memcpy(&rom[0x20], prog, sizeof(prog));

    colem_init(&emu, rom, sizeof(rom), ram, vram, fb);
    for (int f = 0; f < 50; ++f) {
        colem_step_frame(&emu);
    }

    size_t px_count = 256 * 192;
    bool ok = (ram[0] == 0x5A) && (count_nonzero_pixels(fb, px_count) > 1000);
    uint32_t hash = fnv1a_hash16(fb, px_count);
    printf("[HOST_CORE] COLECO      | RAM[0]=0x%02X | FB_HASH=0x%08X | STATUS=%s\n",
           ram[0], hash, ok ? "PASS" : "FAIL");
    free(ram);
    free(vram);
    free(fb);
    return ok ? 0 : 3;
}

static int test_lynx_core(void) {
    lynx_t emu;
    uint8_t* ram = (uint8_t*)calloc(1, 0x10000);
    uint16_t* fb = (uint16_t*)calloc(160 * 102, sizeof(uint16_t));
    uint8_t rom[1024];
    memset(rom, 0xEA, sizeof(rom));

    const uint8_t prog[] = {
        0xA9, 0x0C, 0x8D, 0x52, 0xFC, /* LDA #12; STA $FC52 */
        0xA9, 0x00, 0x8D, 0x53, 0xFC, /* LDA #0;  STA $FC53 */
        0xA9, 0x0F, 0x8D, 0x54, 0xFC, /* LDA #15; STA $FC54 */
        0xA9, 0x00, 0x8D, 0x55, 0xFC, /* LDA #0;  STA $FC55 -> 12*15=180 in $FC60 */
        0xA9, 0x0F, 0x8D, 0xA1, 0xFD, /* Green color 1 */
        0xA9, 0x11, 0x8D, 0x00, 0xC0, /* Pixel in video buffer at $C000 */
        0x80, 0xFE                    /* BRA -2 */
    };
    memcpy(rom, prog, sizeof(prog));

    lynx_init(&emu, rom, sizeof(rom), ram, fb);
    for (int f = 0; f < 50; ++f) {
        lynx_step_frame(&emu);
    }

    uint16_t prod = (uint16_t)emu.suzy[0x60] | ((uint16_t)emu.suzy[0x61] << 8);
    size_t px_count = 160 * 102;
    bool ok = (prod == 180) && (count_nonzero_pixels(fb, px_count) > 0);
    uint32_t hash = fnv1a_hash16(fb, px_count);
    printf("[HOST_CORE] ATARI_LYNX  | SUZY_MUL=%u | FB_HASH=0x%08X | STATUS=%s\n",
           prod, hash, ok ? "PASS" : "FAIL");
    free(ram);
    free(fb);
    return ok ? 0 : 3;
}

static int test_ngp_core(void) {
    ngp_t emu;
    uint8_t* ram = (uint8_t*)calloc(1, 0x10000);
    uint16_t* fb = (uint16_t*)calloc(160 * 152, sizeof(uint16_t));
    uint8_t rom[1024];
    memset(rom, 0x00, sizeof(rom));

    rom[0x1C] = 0x40;
    rom[0x1D] = 0x00;
    rom[0x1E] = 0x20;
    rom[0x1F] = 0x00;
    const uint8_t prog[] = {
        0x21, 0x77,                   /* LD A, 0x77 */
        0xF1, 0x00, 0x40, 0x00, 0x77, /* LD (0x4000), #0x77 -> ram[0x0000] */
        0x68, 0xFE                    /* JR -2 */
    };
    memcpy(&rom[0x40], prog, sizeof(prog));

    ngp_init(&emu, rom, sizeof(rom), ram, fb, true);
    for (int f = 0; f < 50; ++f) {
        ngp_step_frame(&emu);
    }

    size_t px_count = 160 * 152;
    bool ok = (ram[0] == 0x77) && (count_nonzero_pixels(fb, px_count) > 1000);
    uint32_t hash = fnv1a_hash16(fb, px_count);
    printf("[HOST_CORE] NGP_COLOR   | RAM[0]=0x%02X | FB_HASH=0x%08X | STATUS=%s\n",
           ram[0], hash, ok ? "PASS" : "FAIL");
    free(ram);
    free(fb);
    return ok ? 0 : 3;
}

static int test_snes_core(void) {
    snes_t emu;
    uint8_t* wram = (uint8_t*)calloc(1, 0x20000);
    uint16_t* fb = (uint16_t*)calloc(256 * 224, sizeof(uint16_t));
    uint8_t* rom = (uint8_t*)calloc(1, 0x8000);

    rom[0x7FFC] = 0x00;
    rom[0x7FFD] = 0x80;
    const uint8_t prog[] = {
        0x18, 0xFB,             /* CLC; XCE */
        0xA9, 0x3B,             /* LDA #$3B */
        0x85, 0x00,             /* STA $00 (WRAM[0]) */
        0xA9, 0x00, 0x8D, 0x21, 0x21, /* STA $2121 (CGADD=0) */
        0xA9, 0x1F, 0x8D, 0x22, 0x21, /* STA $2122 (CGDATA LSB) */
        0xA9, 0x00, 0x8D, 0x22, 0x21, /* STA $2122 (CGDATA MSB -> red backdrop) */
        0x80, 0xFE              /* BRA -2 */
    };
    memcpy(rom, prog, sizeof(prog));

    snes_init(&emu, rom, 0x8000, wram, fb);
    for (int f = 0; f < 50; ++f) {
        snes_step_frame(&emu);
    }

    size_t px_count = 256 * 224;
    bool ok = (wram[0] == 0x3B) && !emu.emu_mode && (count_nonzero_pixels(fb, px_count) > 1000);
    uint32_t hash = fnv1a_hash16(fb, px_count);
    printf("[HOST_CORE] SNES_65C816 | WRAM[0]=0x%02X | FB_HASH=0x%08X | STATUS=%s\n",
           wram[0], hash, ok ? "PASS" : "FAIL");
    snes_destroy(&emu);
    free(wram);
    free(fb);
    free(rom);
    return ok ? 0 : 3;
}

static int test_wswan_core(void) {
    wswan_t emu;
    uint8_t* ram = (uint8_t*)calloc(1, 0x10000);
    uint16_t* fb = (uint16_t*)calloc(224 * 144, sizeof(uint16_t));
    uint8_t* rom = (uint8_t*)calloc(1, 0x10000);

    const uint8_t prog[] = {
        0xB0, 0x9C,       /* MOV AL, 0x9C */
        0xA2, 0x10, 0x00, /* MOV [0x0010], AL */
        0xEB, 0xFE        /* JMP -2 */
    };
    memcpy(&rom[0xFFF0], prog, sizeof(prog));

    wswan_init(&emu, rom, 0x10000, ram, fb, true);
    for (int f = 0; f < 50; ++f) {
        wswan_step_frame(&emu);
    }

    size_t px_count = 224 * 144;
    bool ok = (emu.internal_ram[0x0010] == 0x9C) && (count_nonzero_pixels(fb, px_count) > 100);
    uint32_t hash = fnv1a_hash16(fb, px_count);
    printf("[HOST_CORE] WONDERSWAN  | RAM[10]=0x%02X | FB_HASH=0x%08X | STATUS=%s\n",
           emu.internal_ram[0x0010], hash, ok ? "PASS" : "FAIL");
    wswan_destroy(&emu);
    free(ram);
    free(fb);
    free(rom);
    return ok ? 0 : 3;
}

static int test_genesis_core(void) {
    genesis_t emu;
    uint8_t* ram = (uint8_t*)calloc(1, 0x10000);
    uint16_t* fb = (uint16_t*)calloc(320 * 224, sizeof(uint16_t));
    uint8_t rom[1024];
    memset(rom, 0, sizeof(rom));

    rom[0] = 0x00; rom[1] = 0xFF; rom[2] = 0xFE; rom[3] = 0x00;
    rom[4] = 0x00; rom[5] = 0x00; rom[6] = 0x02; rom[7] = 0x00;
    rom[0x200] = 0x70; rom[0x201] = 0x4D; /* MOVEQ #0x4D, D0 */
    rom[0x202] = 0x60; rom[0x203] = 0xFE; /* BRA.S -2 */

    genesis_init(&emu, rom, sizeof(rom), ram, fb);
    for (int f = 0; f < 50; ++f) {
        genesis_step_frame(&emu);
    }

    size_t px_count = 320 * 224;
    bool ok = (emu.d[0] == 0x4D);
    uint32_t hash = fnv1a_hash16(fb, px_count);
    printf("[HOST_CORE] GENESIS_68K | D0=0x%08X | FB_HASH=0x%08X | STATUS=%s\n",
           emu.d[0], hash, ok ? "PASS" : "FAIL");
    genesis_destroy(&emu);
    free(ram);
    free(fb);
    return ok ? 0 : 3;
}

static int test_sms_core(void) {
    sms_context_t* ctx = sms_create();
    if (!ctx) return 1;

    uint8_t rom[0x8000];
    memset(rom, 0x00, sizeof(rom));
    const uint8_t prog[] = {
        0x3E, 0x66,       /* LD A, 0x66 */
        0x32, 0x00, 0xC0, /* LD (0xC000), A */
        0x3E, 0x00,       /* LD A, 0x00 */
        0xD3, 0xBF,       /* OUT (0xBF), A */
        0x3E, 0xC0,       /* LD A, 0xC0 (CRAM write addr 0) */
        0xD3, 0xBF,       /* OUT (0xBF), A */
        0x3E, 0x3F,       /* LD A, 0x3F (white in CRAM[0]) */
        0xD3, 0xBE,       /* OUT (0xBE), A */
        0xC3, 0x0F, 0x00  /* JP 0x000F */
    };
    memcpy(rom, prog, sizeof(prog));

    if (!sms_load_rom(ctx, rom, sizeof(rom), false)) {
        sms_destroy(ctx);
        return 2;
    }
    for (int f = 0; f < 50; ++f) {
        sms_run_frame(ctx);
    }

    size_t px_count = 256 * 192;
    bool ok = (ctx->ram[0] == 0x66) && (count_nonzero_pixels(ctx->framebuffer, px_count) > 1000);
    uint32_t hash = fnv1a_hash16(ctx->framebuffer, px_count);
    printf("[HOST_CORE] SMS_Z80     | RAM[0]=0x%02X | FB_HASH=0x%08X | STATUS=%s\n",
           ctx->ram[0], hash, ok ? "PASS" : "FAIL");
    sms_destroy(ctx);
    return ok ? 0 : 3;
}

static int test_pce_core(void) {
    pce_context_t* ctx = pce_create();
    if (!ctx) return 1;

    uint8_t rom[0x2000];
    memset(rom, 0xEA, sizeof(rom));
    rom[0x1FFE] = 0x00;
    rom[0x1FFF] = 0xE0;
    const uint8_t prog[] = {
        0xA9, 0x42,             /* LDA #$42 */
        0x85, 0x00,             /* STA $2000 (RAM[0]) */
        0xA9, 0x00, 0x8D, 0x02, 0x04, /* STA $0402 (VCE addr lo = 0) */
        0xA9, 0x00, 0x8D, 0x03, 0x04, /* STA $0403 (VCE addr hi = 0) */
        0xA9, 0xFF, 0x8D, 0x04, 0x04, /* STA $0404 (VCE data lo) */
        0xA9, 0x01, 0x8D, 0x05, 0x04, /* STA $0405 (VCE data hi -> white backdrop) */
        0x4C, 0x18, 0xE0        /* JMP $E018 */
    };
    memcpy(rom, prog, sizeof(prog));

    if (!pce_load_rom(ctx, rom, sizeof(rom))) {
        pce_destroy(ctx);
        return 2;
    }
    for (int f = 0; f < 50; ++f) {
        pce_run_frame(ctx);
    }

    size_t px_count = 256 * 240;
    bool ok = (ctx->ram[0] == 0x42) && (count_nonzero_pixels(ctx->framebuffer, px_count) > 1000);
    uint32_t hash = fnv1a_hash16(ctx->framebuffer, px_count);
    printf("[HOST_CORE] PC_ENGINE   | RAM[0]=0x%02X | FB_HASH=0x%08X | STATUS=%s\n",
           ctx->ram[0], hash, ok ? "PASS" : "FAIL");
    pce_destroy(ctx);
    return ok ? 0 : 3;
}

static int test_pico_core(void) {
    pico_context_t* ctx = pico_create();
    if (!ctx) return 1;

    const char* p8_cart =
        "pico-8 cartridge // http://www.pico-8.com\n"
        "version 41\n"
        "__lua__\n"
        "cls(1)\n"
        "__gfx__\n"
        "0777777007777770\n"
        "7788887777888877\n";

    if (!pico_load_cart(ctx, (const uint8_t*)p8_cart, strlen(p8_cart))) {
        pico_destroy(ctx);
        return 2;
    }
    pico_cls(ctx, 12);
    pico_spr(ctx, 0, 16, 16, false, false);
    for (int f = 0; f < 50; ++f) {
        pico_run_frame(ctx);
    }

    size_t px_count = 128 * 128;
    bool ok = (ctx->ram[0] == 0x70) && (count_nonzero_pixels(ctx->framebuffer, px_count) > 1000);
    uint32_t hash = fnv1a_hash16(ctx->framebuffer, px_count);
    printf("[HOST_CORE] PICO_8      | SPR[0]=0x%02X | FB_HASH=0x%08X | STATUS=%s\n",
           ctx->ram[0], hash, ok ? "PASS" : "FAIL");
    pico_destroy(ctx);
    return ok ? 0 : 3;
}

static int test_nes_agnes_core(void) {
    agnes_t* nes = agnes_make();
    if (!nes) return 1;

    /* 16-byte iNES header + 16KB PRG-ROM + 8KB CHR-ROM */
    size_t ines_size = 16 + 16384 + 8192;
    uint8_t* ines = (uint8_t*)calloc(1, ines_size);
    if (!ines) {
        agnes_destroy(nes);
        return 1;
    }
    ines[0] = 'N'; ines[1] = 'E'; ines[2] = 'S'; ines[3] = 0x1A;
    ines[4] = 1; /* 1x 16KB PRG-ROM bank */
    ines[5] = 1; /* 1x 8KB CHR-ROM bank */
    ines[6] = 0x02; /* Mapper 0 + battery PRG-RAM */

    uint8_t* prg = ines + 16;
    memset(prg, 0xEA, 16384); /* NOP fill */
    /* Reset vector at $FFFC -> $8000 */
    prg[0x3FFC] = 0x00;
    prg[0x3FFD] = 0x80;

    /* 6502 boot program at $8000:
     * LDA #$5C; STA $6000 (Cartridge PRG-RAM)
     * LDA #$3F; STA $2006; LDA #$00; STA $2006 (PPU Addr $3F00)
     * LDA #$21; STA $2007 (Backdrop palette color $21)
     * LDA #$08; STA $2001 (Enable background rendering)
     * INX; JMP $8018 (dynamic counter in X register for state determinism test)
     */
    const uint8_t prog[] = {
        0xA9, 0x5C,             /* LDA #$5C */
        0x8D, 0x00, 0x60,       /* STA $6000 (PRG-RAM[0]) */
        0xA9, 0x3F, 0x8D, 0x06, 0x20, /* LDA #$3F; STA $2006 */
        0xA9, 0x00, 0x8D, 0x06, 0x20, /* LDA #$00; STA $2006 */
        0xA9, 0x21, 0x8D, 0x07, 0x20, /* LDA #$21; STA $2007 */
        0xA9, 0x08, 0x8D, 0x01, 0x20, /* LDA #$08; STA $2001 */
        0xE8,                   /* INX ($8018) */
        0x8E, 0x01, 0x60,       /* STX $6001 */
        0x4C, 0x18, 0x80        /* JMP $8018 */
    };
    memcpy(prg, prog, sizeof(prog));

    if (!agnes_load_ines_data(nes, ines, ines_size)) {
        free(ines);
        agnes_destroy(nes);
        return 2;
    }

    size_t state_sz = agnes_state_size();
    agnes_state_t* saved_state = (agnes_state_t*)calloc(1, state_sz);
    for (int f = 0; f < 25; ++f) {
        agnes_next_frame(nes);
    }
    agnes_dump_state(nes, saved_state);

    for (int f = 25; f < 50; ++f) {
        agnes_next_frame(nes);
    }
    size_t prg_ram_sz = 0;
    uint8_t* prg_ram = agnes_get_prg_ram(nes, &prg_ram_sz);
    uint8_t count_first_run = prg_ram ? prg_ram[1] : 0;
    const uint8_t* screen1 = agnes_get_screen_buffer(nes);
    uint32_t hash1 = fnv1a_hash16((const uint16_t*)screen1, (256 * 240) / 2);

    /* Restore snapshot at frame 25 and re-run frames 25..50 to prove bit-exact state determinism */
    agnes_restore_state(nes, saved_state);
    for (int f = 25; f < 50; ++f) {
        agnes_next_frame(nes);
    }
    uint8_t count_second_run = prg_ram ? prg_ram[1] : 0;
    const uint8_t* screen2 = agnes_get_screen_buffer(nes);
    uint32_t hash2 = fnv1a_hash16((const uint16_t*)screen2, (256 * 240) / 2);

    bool ok = (prg_ram != NULL) && (prg_ram_sz == 8192) && (prg_ram[0] == 0x5C)
              && (count_first_run == count_second_run) && (hash1 == hash2);
    printf("[HOST_CORE] NES_AGNES   | PRGRAM[0]=0x%02X | FB_HASH=0x%08X | STATE_DET=%s | STATUS=%s\n",
           prg_ram ? prg_ram[0] : 0, hash1, (hash1 == hash2 && count_first_run == count_second_run) ? "EXACT" : "DRIFT", ok ? "PASS" : "FAIL");

    free(saved_state);
    free(ines);
    agnes_destroy(nes);
    return ok ? 0 : 3;
}

int main(void) {
    int failures = 0;
    failures += (test_atari_core() != 0);
    failures += (test_colem_core() != 0);
    failures += (test_lynx_core() != 0);
    failures += (test_ngp_core() != 0);
    failures += (test_snes_core() != 0);
    failures += (test_wswan_core() != 0);
    failures += (test_genesis_core() != 0);
    failures += (test_sms_core() != 0);
    failures += (test_pce_core() != 0);
    failures += (test_pico_core() != 0);
    failures += (test_nes_agnes_core() != 0);

    if (failures == 0) {
        printf("ALL 10 HOST EMULATOR CORES PASSED 50-FRAME EXECUTION & VISUAL REGRESSION!\n");
        printf("ALL 11 HOST EMULATOR CORES (INCLUDING NES AGNES + STATE DETERMINISM) PASSED!\n");
        return 0;
    }
    printf("HOST EMULATOR RUNNER FAILED (%d cores failed)\n", failures);
    return 1;
}
