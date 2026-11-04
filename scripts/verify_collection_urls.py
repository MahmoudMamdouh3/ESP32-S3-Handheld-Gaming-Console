import urllib.request
import urllib.parse

BASE_URL = "https://archive.org/download/no-intro_romset_collection/"

packs = [
    "Atari - 2600.7z",
    "Atari - Lynx.7z",
    "Bandai - WonderSwan.7z",
    "Bandai - WonderSwan Color.7z",
    "NEC - PC Engine - TurboGrafx 16.7z",
    "Nintendo - Game Boy.7z",
    "Nintendo - Game Boy Color.7z",
    "Nintendo - Nintendo Entertainment System (Headered).7z",
    "Nintendo - Super Nintendo Entertainment System.7z",
    "SNK - Neo Geo Pocket Color.7z",
    "Sega - Game Gear.7z",
    "Sega - Master System - Mark III.7z",
    "Sega - Mega Drive - Genesis.7z"
]

print("Verifying URLs on Archive.org...")
for p in packs:
    url = BASE_URL + urllib.parse.quote(p)
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'}, method='HEAD')
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            print(f"[OK 200] {p:<55} ({int(resp.headers.get('content-length', 0))/(1024*1024):.1f} MB)")
    except Exception as e:
        print(f"[FAIL] {p}: {e}")
