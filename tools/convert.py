import sys
def bin_to_header(bin_file, out_file, var_name):
    with open(bin_file, 'rb') as f:
        data = f.read()
    with open(out_file, 'w') as f:
        f.write('#pragma once\n#include <stdint.h>\n')
        f.write(f'const uint8_t {var_name}[] __attribute__((section(".rodata"))) __attribute__((aligned(4))) = {{\n')
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            f.write(','.join([f'0x{b:02x}' for b in chunk]) + ',\n')
        f.write('};\n')
        f.write(f'const size_t {var_name}_size = {len(data)};\n')
bin_to_header(r'E:\BMO Gameboy\roms\gbc\mario\Super Mario Bros. Deluxe (USA, Europe) (Rev 1).gbc', 'mario_deluxe.h', 'mario_deluxe_rom')
bin_to_header(r'E:\BMO Gameboy\roms\gbc\zelda\Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc', 'zelda_ages.h', 'zelda_ages_rom')
