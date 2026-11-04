import urllib.request
import json

identifiers = [
    'No-Intro_GB',
    'No-Intro_GBC',
    'No-Intro_NES',
    'No-Intro_Super_Nintendo_SNES',
    'nointro.md',
    'nointro.ms-mkiii',
    'nointro.gg',
    'nointro.tg-16',
    'nointro.atari-2600'
]

for ident in identifiers:
    url = f'https://archive.org/metadata/{ident}/files'
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            files = json.loads(resp.read().decode('utf-8'))['result']
            print(f"\n=== {ident} ({len(files)} files) ===")
            # Look for big zip/7z/tar files or torrents
            for f in files:
                name = f.get('name', '')
                sz = int(f.get('size', 0))
                fmt = f.get('format', '')
                if fmt in ('Archive BitTorrent', '7z', 'ZIP') or 'torrent' in name or sz > 50 * 1024 * 1024:
                    print(f"  {name} | {sz / (1024*1024):.1f} MB | {fmt}")
    except Exception as e:
        print(f"Error {ident}: {e}")
