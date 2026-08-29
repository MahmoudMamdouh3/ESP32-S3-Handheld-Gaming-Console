#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define IRAM_ATTR
#define __attribute__(x) 

#define WALNUT_GB_12_COLOUR 0
#define ENABLE_LCD 0
#define ENABLE_SOUND 0
#define WALNUT_FULL_GBC_SUPPORT 0
#define WALNUT_GB_RGB565_BIGENDIAN 1

namespace WGB {
    #include "../firmware/BmoGameboy/src/engine/walnut_cgb/walnut_cgb.h"
}
using namespace WGB;

static uint8_t* rom_data = NULL;
static size_t rom_len = 0;
static uint8_t cart_ram[128 * 1024];
static struct gb_s gb;

uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr >= rom_len) return 0xFF;
    return rom_data[addr];
}

uint16_t gb_rom_read16(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr + 2 > rom_len) return 0xFFFF;
    return (uint16_t)rom_data[addr] | ((uint16_t)rom_data[addr+1] << 8);
}

uint32_t gb_rom_read32(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr + 4 > rom_len) return 0xFFFFFFFF;
    return (uint32_t)rom_data[addr] | 
          ((uint32_t)rom_data[addr+1] << 8) | 
          ((uint32_t)rom_data[addr+2] << 16) | 
          ((uint32_t)rom_data[addr+3] << 24);
}

uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
    if (addr < sizeof(cart_ram)) return cart_ram[addr];
    return 0xFF;
}

void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    if (addr < sizeof(cart_ram)) cart_ram[addr] = val;
}

void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t val) {
    printf("Walnut-CGB Error: %d at PC 0x%04X\n", gb_err, val);
    exit(1);
}

void lcd_draw_line(struct gb_s *gb, const uint8_t *pixels, const uint_fast8_t line) {}

void serial_tx(struct gb_s *gb, const uint8_t val) {
    putchar(val);
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <rom>\n", argv[0]);
        return 1;
    }
    
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        printf("Failed to open %s\n", argv[1]);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    rom_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    rom_data = (uint8_t*)malloc(rom_len);
    fread(rom_data, 1, rom_len, f);
    fclose(f);
    
    enum gb_init_error_e err = gb_init(&gb, &gb_rom_read, &gb_rom_read16, &gb_rom_read32, &gb_cart_ram_read, &gb_cart_ram_write, &gb_error, NULL);
    if (err != GB_INIT_NO_ERROR) {
        printf("gb_init failed: %d\n", err);
        return 1;
    }
    
    gb.display.lcd_draw_line = &lcd_draw_line;
    gb_init_serial(&gb, &serial_tx, NULL);
    
    printf("Running test %s...\n", argv[1]);
    
    // Blargg's CPU tests output continuously to serial, then eventually print "Passed" or "Failed"
    for (int i = 0; i < 2000; i++) {
        gb_run_frame(&gb);
    }
    
    printf("\nTest finished.\n");
    free(rom_data);
    return 0;
}
