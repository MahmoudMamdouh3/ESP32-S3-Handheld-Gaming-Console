import re
import zipfile
from pathlib import Path

from repo_tools import find_game_roots, sanitize_identifier

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:  # pragma: no cover - runtime dependency guard
    raise SystemExit(
        "Pillow is required for cover generation. Install it with: python -m pip install pillow"
    ) from exc

REPO_ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = REPO_ROOT / 'firmware' / 'BmoGameboy' / 'src' / 'assets' / 'roms'


def generate_gradient_cover(title, out_path, var_name):
    img = Image.new('RGB', (100, 100))
    draw = ImageDraw.Draw(img)
    for y in range(100):
        r = int(255 * (y / 100))
        g = int(128 * (y / 100))
        b = int(255 - (128 * (y / 100)))
        draw.line([(0, y), (100, y)], fill=(r, g, b))

    try:
        font = ImageFont.truetype('arial.ttf', 12)
    except OSError:
        font = ImageFont.load_default()

    words = title.split()
    lines = []
    line = ''
    for word in words:
        if len(line) + len(word) > 12:
            lines.append(line)
            line = word
        else:
            line = f'{line} {word}' if line else word
    if line:
        lines.append(line)

    y = 10
    for text_line in lines:
        draw.text((5, y), text_line, font=font, fill=(255, 255, 255))
        y += 15

    c_array = []
    for py in range(100):
        for px in range(100):
            r, g, b = img.getpixel((px, py))
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            swapped = ((rgb565 & 0xFF) << 8) | ((rgb565 >> 8) & 0xFF)
            c_array.append(f'0x{swapped:04X}')

    out_path.write_text(
        '#pragma once\n'
        '#include <stdint.h>\n'
        '#ifdef __AVR__\n'
        '#include <avr/pgmspace.h>\n'
        '#else\n'
        '#define PROGMEM\n'
        '#endif\n\n'
        f'const uint16_t {var_name}[10000] PROGMEM = {{\n'
        + '\n'.join(
            '  ' + ', '.join(c_array[i:i + 10]) + ','
            for i in range(0, len(c_array), 10)
        )
        + '\n};\n'
    )


def main():
    game_roots = find_game_roots(REPO_ROOT)
    if not game_roots:
        raise FileNotFoundError('No game directories found under the repo root.')

    games_dir = next(iter(game_roots.values()))
    generated_games = []

    for zip_path in sorted(games_dir.glob('*.zip')):
        title = zip_path.stem
        with zipfile.ZipFile(zip_path, 'r') as archive:
            rom_name = next(
                (name for name in archive.namelist() if name.lower().endswith(('.gb', '.gbc'))),
                None,
            )
            if not rom_name:
                continue

            rom_data = archive.read(rom_name)
            var_base = sanitize_identifier(title)
            rom_var = f'rom_{var_base}'
            cover_var = f'cover_{var_base}'

            rom_out_path = OUT_DIR / f'rom_{var_base}.h'
            rom_out_path.write_text(
                '#pragma once\n'
                '#include <stdint.h>\n'
                '#ifdef __AVR__\n'
                '#include <avr/pgmspace.h>\n'
                '#else\n'
                '#define PROGMEM\n'
                '#endif\n\n'
                f'const size_t {rom_var}_len = {len(rom_data)};\n'
                f'const unsigned char {rom_var}[] PROGMEM = {{\n'
                + '\n'.join(
                    '  ' + ', '.join(f'0x{b:02X}' for b in rom_data[index:index + 16]) + ','
                    for index in range(0, len(rom_data), 16)
                )
                + '\n};\n'
            )

            cover_out_path = OUT_DIR / f'cover_{var_base}.h'
            generate_gradient_cover(title, cover_out_path, cover_var)

            generated_games.append({
                'title': title,
                'rom_var': rom_var,
                'cover_var': cover_var,
                'rom_file': f'rom_{var_base}.h',
                'cover_file': f'cover_{var_base}.h',
            })
            print(f'Processed: {title}')

    print('\n// INCLUDES TO ADD')
    for game in generated_games:
        print(f'#include "{game["rom_file"]}"')
        print(f'#include "{game["cover_file"]}"')

    print('\n// ARRAY ENTRIES TO ADD')
    for game in generated_games:
        print(f'{{"{game["title"]}", {game["rom_var"]}, {game["rom_var"]}_len, {game["cover_var"]}}},')


if __name__ == '__main__':
    main()

