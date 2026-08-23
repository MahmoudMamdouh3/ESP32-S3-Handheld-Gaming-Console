import os

roms = [
    ("Amazing Spider-Man, The (USA, Europe).gb", "rom_amazingspiderman", "rom_spiderman.h"),
    ("Kirby's Dream Land 2 (USA, Europe) (SGB Enhanced).gb", "rom_kirby2", "rom_kirby2.h"),
    ("Legend of Zelda, The - Link's Awakening (USA, Europe) (Rev 2).gb", "rom_zeldala", "rom_zeldala.h"),
    ("Sneaky Snakes (USA, Europe).gb", "rom_sneakysnakes", "rom_sneakysnakes.h"),
    ("Super Mario Land (World) (Rev 1).gb", "rom_supermarioland", "rom_supermarioland.h"),
    ("Tetris 2 (USA, Europe) (SGB Enhanced).gb", "rom_tetris2", "rom_tetris2.h")
]

for filename, var_name, out_file in roms:
    in_path = os.path.join('firmware', '03_emulator', filename)
    out_path = os.path.join('firmware', '03_emulator', out_file)
    print(f"Converting {filename}...")
    with open(in_path, 'rb') as f:
        data = f.read()
    
    with open(out_path, 'w') as out:
        out.write('#pragma once\n#if defined(ESP8266) || defined(ESP32)\n#include <pgmspace.h>\n#else\n#define PROGMEM\n#endif\n\n')
        out.write(f'const unsigned char {var_name}[] PROGMEM = {{\n')
        lines = [', '.join(f'0x{b:02X}' for b in data[i:i+20]) for i in range(0, len(data), 20)]
        out.write(',\n'.join(lines))
        out.write(f'\n}};\nconst unsigned int {var_name}_len = {len(data)};\n')
    print(f"Saved {out_path}")
