import sys
def convert(filename, var_name, output_file):
    with open(filename, 'rb') as f:
        data = f.read()
    with open(output_file, 'w') as f:
        f.write("#pragma once\n#include <stdint.h>\n")
        f.write(f"const uint8_t {var_name}[] __attribute__((section(\".rodata\"))) __attribute__((aligned(4))) = {{\n")
        for i, byte in enumerate(data):
            f.write(f"0x{byte:02x}, ")
            if i % 16 == 15:
                f.write("\n")
        f.write("};\n")
        f.write(f"const size_t {var_name}_size = {len(data)};\n")

convert('E:\\BMO Gameboy\\roms\\gbc\\Aladdin (USA).gbc', 'aladdin_rom', 'E:\\BMO Gameboy\\firmware\\03_emulator\\aladdin.h')
convert('E:\\BMO Gameboy\\roms\\gbc\\LEGO Racers (USA) (En,Fr,Es).gbc', 'lego_racers_rom', 'E:\\BMO Gameboy\\firmware\\03_emulator\\lego_racers.h')
