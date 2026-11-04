import urllib.request
import urllib.parse
import json

consoles = [
    'Game Boy',
    'Game Boy Color',
    'Nintendo Entertainment System',
    'Super Nintendo',
    'Mega Drive',
    'Genesis',
    'Master System',
    'Game Gear',
    'PC Engine',
    'Atari 2600',
    'Atari Lynx',
    'ColecoVision',
    'Neo Geo Pocket',
    'WonderSwan'
]

for c in consoles:
    q = urllib.parse.quote(f'title:("No-Intro" AND "{c}") AND mediatype:(software)')
    url = f'https://archive.org/advancedsearch.php?q={q}&fl[]=identifier,title,downloads&sort[]=downloads+desc&rows=3&output=json'
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            docs = data['response']['docs']
            if docs:
                print(f"=== {c} ===")
                for d in docs:
                    print(f"  {d['identifier']}: {d.get('title')} ({d.get('downloads')} dls)")
    except Exception as e:
        print(f"Error for {c}: {e}")
