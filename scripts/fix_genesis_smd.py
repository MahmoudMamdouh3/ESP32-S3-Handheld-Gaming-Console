import os
from pathlib import Path

GAMES_DIR = Path(r"E:\BMO Gameboy\games")

def deinterleave_smd(payload):
    out = bytearray(len(payload))
    block_size = 16384
    half = 8192
    for b in range(0, len(payload), block_size):
        block = payload[b:b+block_size]
        if len(block) == block_size:
            for i in range(half):
                out[b + 2*i] = block[half + i]
                out[b + 2*i + 1] = block[i]
        else:
            out[b:b+len(block)] = block
    return bytes(out)

def fix_smd_files():
    fixed = 0
    skipped = 0
    for f in GAMES_DIR.glob("*.gen"):
        try:
            with open(f, 'rb') as fp:
                data = fp.read()
            if len(data) > 512 and (data[8:10] == b'\xaa\xbb' or data[1:3] == b'\xaa\xbb'):
                payload = data[512:]
                clean_rom = deinterleave_smd(payload)
                # Verify SEGA magic
                if b'SEGA' in clean_rom[0x100:0x120]:
                    with open(f, 'wb') as fp:
                        fp.write(clean_rom)
                    fixed += 1
                else:
                    print(f"Warning: De-interleaved {f.name} but missing SEGA magic!")
            else:
                skipped += 1
        except Exception as e:
            print(f"Error processing {f.name}: {e}")

    print(f"SMD Conversion Complete: {fixed} files de-interleaved and converted to clean raw Genesis ROMs ({skipped} already raw).")

if __name__ == '__main__':
    fix_smd_files()
