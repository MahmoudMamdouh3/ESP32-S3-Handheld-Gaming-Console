import urllib.request, re, sys, os, time
from PIL import Image

games = {
    'Zelda LA': 'The_Legend_of_Zelda:_Link%27s_Awakening',
    'Sneaky Snakes': 'Sneaky_Snakes',
    'Super Mario Land': 'Super_Mario_Land',
    'Tetris 2': 'Tetris_2_(Nintendo_game)',
    'Tobu Tobu Girl': '', 
    'CPU Test': ''
}

def get_wiki_image(title):
    if not title: return None
    url = f'https://en.wikipedia.org/wiki/{title}'
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'})
    try:
        with urllib.request.urlopen(req) as response:
            html = response.read().decode('utf-8')
            match = re.search(r'<table class="[^"]*infobox[^"]*".*?<img.*?src="(//upload\.wikimedia\.org/wikipedia/en/.*?)"', html, re.IGNORECASE | re.DOTALL)
            if match:
                return 'https:' + match.group(1)
    except Exception as e:
        print(title, 'Error:', e)
    return None

def convert(img, out_path, var_name):
    img = img.convert('RGB')
    img = img.resize((100, 100), Image.Resampling.LANCZOS)
    data = img.getdata()
    
    with open(out_path, 'w') as out:
        out.write('#pragma once\n#if defined(ESP8266) || defined(ESP32)\n#include <pgmspace.h>\n#else\n#define PROGMEM\n#endif\n\n')
        out.write(f'const unsigned short {var_name}[10000] PROGMEM = {{\n')
        for i, (r, g, b) in enumerate(data):
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            out.write(f'0x{rgb565:04X}, ')
            if i % 10 == 9:
                out.write('\n')
        out.write('};\n')

for name, wiki_title in games.items():
    print(f'Fetching {name}...')
    img_url = get_wiki_image(wiki_title)
    var_name = 'cover_' + re.sub(r'[^a-zA-Z0-9]', '', name).lower()
    out_path = f'firmware/03_emulator/{var_name}.h'
    if img_url:
        time.sleep(2) # Sleep to avoid 429
        req = urllib.request.Request(img_url, headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'})
        try:
            with urllib.request.urlopen(req) as response:
                img = Image.open(response)
                convert(img, out_path, var_name)
                print(f'Saved {out_path}')
        except Exception as e:
            print(f'Error downloading image for {name}: {e}')
            img = Image.new('RGB', (100, 100), color=(0,0,0))
            convert(img, out_path, var_name)
    else:
        if 'tobu' not in name.lower() and 'cpu' not in name.lower():
            img = Image.new('RGB', (100, 100), color=(0,0,0))
            convert(img, out_path, var_name)
