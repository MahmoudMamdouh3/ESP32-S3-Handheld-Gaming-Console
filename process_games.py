import os
import zipfile
import re
from PIL import Image, ImageDraw, ImageFont

GAMES_DIR = r'E:\BMO Gameboy\Gameboyt color games'
OUT_DIR = r'firmware\03_emulator'

def generate_gradient_cover(title, out_path, var_name):
    img = Image.new('RGB', (100, 100))
    draw = ImageDraw.Draw(img)
    # Draw simple gradient
    for y in range(100):
        r = int(255 * (y / 100))
        g = int(128 * (y / 100))
        b = int(255 - (128 * (y / 100)))
        draw.line([(0, y), (100, y)], fill=(r, g, b))
    
    # Try to load a font, fallback to default
    try:
        font = ImageFont.truetype('arial.ttf', 12)
    except:
        font = ImageFont.load_default()
    
    # Split title into lines
    words = title.split()
    lines = []
    line = ''
    for w in words:
        if len(line) + len(w) > 12:
            lines.append(line)
            line = w
        else:
            line += ' ' + w if line else w
    if line: lines.append(line)
    
    y = 10
    for l in lines:
        draw.text((5, y), l, font=font, fill=(255, 255, 255))
        y += 15

    # Convert to RGB565
    c_array = []
    for py in range(100):
        for px in range(100):
            r, g, b = img.getpixel((px, py))
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            # Swap endian for SPI
            swapped = ((rgb565 & 0xFF) << 8) | ((rgb565 >> 8) & 0xFF)
            c_array.append(f'0x{swapped:04X}')

    with open(out_path, 'w') as f:
        f.write(f'#pragma once\n#include <stdint.h>\n#ifdef __AVR__\n#include <avr/pgmspace.h>\n#else\n#define PROGMEM\n#endif\n\n')
        f.write(f'const uint16_t {var_name}[10000] PROGMEM = {{\n')
        for i in range(0, len(c_array), 10):
            f.write('  ' + ', '.join(c_array[i:i+10]) + ',\n')
        f.write('};\n')

generated_games = []

for filename in os.listdir(GAMES_DIR):
    if filename.endswith('.zip'):
        title = os.path.splitext(filename)[0]
        zip_path = os.path.join(GAMES_DIR, filename)
        
        with zipfile.ZipFile(zip_path, 'r') as z:
            # Find the ROM file
            rom_name = None
            for name in z.namelist():
                if name.endswith('.gbc') or name.endswith('.gb'):
                    rom_name = name
                    break
            
            if rom_name:
                rom_data = z.read(rom_name)
                var_base = re.sub(r'[^a-zA-Z0-9]', '', title).lower()
                rom_var = f'rom_{var_base}'
                cover_var = f'cover_{var_base}'
                
                # Write ROM header
                rom_out_path = os.path.join(OUT_DIR, f'rom_{var_base}.h')
                with open(rom_out_path, 'w') as f:
                    f.write(f'#pragma once\n#include <stdint.h>\n#ifdef __AVR__\n#include <avr/pgmspace.h>\n#else\n#define PROGMEM\n#endif\n\n')
                    f.write(f'const size_t {rom_var}_len = {len(rom_data)};\n')
                    f.write(f'const unsigned char {rom_var}[] PROGMEM = {{\n')
                    # format as hex
                    hex_data = [f'0x{b:02X}' for b in rom_data]
                    for i in range(0, len(hex_data), 16):
                        f.write('  ' + ', '.join(hex_data[i:i+16]) + ',\n')
                    f.write('};\n')
                
                # Write Cover header
                cover_out_path = os.path.join(OUT_DIR, f'cover_{var_base}.h')
                generate_gradient_cover(title, cover_out_path, cover_var)
                
                generated_games.append({
                    'title': title,
                    'rom_var': rom_var,
                    'cover_var': cover_var,
                    'rom_file': f'rom_{var_base}.h',
                    'cover_file': f'cover_{var_base}.h'
                })
                print(f'Processed: {title}')

# Output include block
print('\n// INCLUDES TO ADD')
for g in generated_games:
    print(f'#include "{g["rom_file"]}"')
    print(f'#include "{g["cover_file"]}"')

print('\n// ARRAY ENTRIES TO ADD')
for g in generated_games:
    print(f'{{"{g["title"]}", {g["rom_var"]}, {g["rom_var"]}_len, {g["cover_var"]}}},')

