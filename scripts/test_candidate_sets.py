import urllib.request
import json

candidates = [
    'no-intro_romset_collection',
    'ni-roms',
    'No-Intro-Collection_2016-01-03_Fixed',
    'cylums-super-nintendo-rom-collection',
    'cylums-sega-genesis-rom-collection',
    'cylums-nintendo-entertainment-system-rom-collection',
    'cylums-game-boy-rom-collection',
    'cylums-game-boy-color-rom-collection',
    'cylums-sega-master-system-rom-collection',
    'cylums-sega-game-gear-collection',
    'cylums-atari-2600-rom-collection',
    'cylums-atari-lynx-rom-collection',
    'retro-roms-best-set',
    'tiny-best-set-go'
]

for c in candidates:
    url = f'https://archive.org/metadata/{c}/files'
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    try:
        with urllib.request.urlopen(req, timeout=8) as resp:
            files = json.loads(resp.read().decode('utf-8'))['result']
            print(f"\n[FOUND] {c}:")
            for f in files:
                name = f.get('name', '')
                sz = int(f.get('size', 0))
                if sz > 10 * 1024 * 1024 and (name.endswith('.zip') or name.endswith('.7z') or name.endswith('.tar')):
                    print(f"   {name} ({sz / (1024*1024):.1f} MB)")
    except Exception as e:
        # Not found or error
        pass
