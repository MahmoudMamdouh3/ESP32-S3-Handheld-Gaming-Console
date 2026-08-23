from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent
ROM_DIR = REPO_ROOT / 'firmware' / '03_emulator'

roms = [
    ('Amazing Spider-Man, The (USA, Europe).gb', 'rom_amazingspiderman', 'rom_spiderman.h'),
    ('Kirby\'s Dream Land 2 (USA, Europe) (SGB Enhanced).gb', 'rom_kirby2', 'rom_kirby2.h'),
    ('Legend of Zelda, The - Link\'s Awakening (USA, Europe) (Rev 2).gb', 'rom_zeldala', 'rom_zeldala.h'),
    ('Sneaky Snakes (USA, Europe).gb', 'rom_sneakysnakes', 'rom_sneakysnakes.h'),
    ('Super Mario Land (World) (Rev 1).gb', 'rom_supermarioland', 'rom_supermarioland.h'),
    ('Tetris 2 (USA, Europe) (SGB Enhanced).gb', 'rom_tetris2', 'rom_tetris2.h'),
]

for filename, var_name, out_file in roms:
    in_path = ROM_DIR / filename
    out_path = ROM_DIR / out_file
    print(f'Converting {filename}...')

    if not in_path.exists():
        print(f'Skipping {filename}: file not found at {in_path}')
        continue

    data = in_path.read_bytes()
    with out_path.open('w', encoding='utf-8') as out:
        out.write('#pragma once\n')
        out.write('#if defined(ESP8266) || defined(ESP32)\n')
        out.write('#include <pgmspace.h>\n')
        out.write('#else\n')
        out.write('#define PROGMEM\n')
        out.write('#endif\n\n')
        out.write(f'const unsigned char {var_name}[] PROGMEM = {{\n')
        lines = [', '.join(f'0x{b:02X}' for b in data[i:i + 20]) for i in range(0, len(data), 20)]
        out.write(',\n'.join(lines))
        out.write(f'\n}};\nconst unsigned int {var_name}_len = {len(data)};\n')
    print(f'Saved {out_path}')
