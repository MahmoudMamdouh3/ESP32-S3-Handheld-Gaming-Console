import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
rom_path = REPO_ROOT / 'firmware' / 'BmoGameboy' / 'src' / 'assets' / 'rom_data.h'

with open(rom_path, 'r') as f:
    content = f.read()

# Extract Tobu Tobu Girl ROM bytes
match = re.search(r'const unsigned char tobu_tobu_girl_rom\[\] PROGMEM = \{(.*?)\};', content, re.DOTALL)
if match:
    hex_data = match.group(1).replace('\n', '').replace(' ', '')
    byte_strings = hex_data.split(',')
    rom_bytes = [int(b, 16) for b in byte_strings if b.startswith('0x')]
    
    print(f'ROM Length: {len(rom_bytes)} bytes')
    
    if len(rom_bytes) > 0x150:
        header_checksum = 0
        for j in range(0x0134, 0x014C + 1):
            header_checksum = (header_checksum - rom_bytes[j] - 1) & 0xFF
            
        expected = rom_bytes[0x014D]
        print(f'Calculated Checksum: 0x{header_checksum:02X}')
        print(f'Expected Checksum (from ROM): 0x{expected:02X}')
        if header_checksum == expected:
            print('Result: ROM Integrity Check PASSED')
        else:
            print('Result: ROM Integrity Check FAILED')
    else:
        print('ROM too small!')
else:
    print('ROM not found')
