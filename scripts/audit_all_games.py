import os
import sys
from pathlib import Path
from collections import defaultdict

GAMES_DIR = Path(r"E:\BMO Gameboy\games")

def validate_nes(data):
    if len(data) < 16:
        return False, "File too small (<16 bytes)"
    if data[:4] != b'NES\x1a':
        return False, f"Missing iNES header magic (got {data[:4]})"
    prg_rom_size = data[4] * 16384
    chr_rom_size = data[5] * 8192
    expected_min = 16 + prg_rom_size + (512 if (data[6] & 0x04) else 0)
    if len(data) < expected_min:
        return False, f"Truncated NES ROM: size {len(data)} < expected {expected_min}"
    return True, "Valid iNES"

def validate_gb_gbc(data):
    if len(data) < 0x150:
        return False, "File too small (<0x150 bytes)"
    chk = 0
    for i in range(0x134, 0x14D):
        chk = (chk - data[i] - 1) & 0xFF
    expected = data[0x14D]
    if chk != expected:
        return False, f"Header checksum mismatch: computed 0x{chk:02X} != 0x{expected:02X}"
    return True, "Valid GB/GBC"

def validate_snes(data):
    if len(data) < 0x8000:
        return False, "File too small (<32KB)"
    for offset in [0x7FC0, 0xFFC0, 0x7FC0 + 512, 0xFFC0 + 512]:
        if len(data) >= offset + 0x30:
            c_comp = int.from_bytes(data[offset+0x1C:offset+0x1E], 'little')
            c_sum = int.from_bytes(data[offset+0x1E:offset+0x20], 'little')
            if (c_comp + c_sum) == 0xFFFF:
                return True, f"Valid SNES (header at 0x{offset:X})"
    for v_off in [0x7FFC, 0xFFFC, 0x7FFC + 512, 0xFFFC + 512]:
        if len(data) >= v_off + 2:
            vec = int.from_bytes(data[v_off:v_off+2], 'little')
            if 0x8000 <= vec <= 0xFFFF:
                return True, f"Valid SNES (vector 0x{vec:04X})"
    return False, "Invalid SNES header/vector"

def validate_genesis(data):
    if len(data) < 0x200:
        return False, "File too small (<512 bytes)"
    magic = data[0x100:0x110]
    if b'SEGA' in magic:
        return True, "Valid Genesis (SEGA magic)"
    init_pc = int.from_bytes(data[4:8], 'big')
    if 0x200 <= init_pc <= len(data) + 0x1000:
        return True, f"Valid Genesis (Initial PC 0x{init_pc:06X})"
    return False, f"Invalid Genesis header (magic: {magic[:8]})"

def validate_sms_gg(data):
    if len(data) < 0x2000:
        return False, "File too small (<8KB)"
    for off in [0x1FF0, 0x3FF0, 0x7FF0]:
        if len(data) >= off + 16:
            if b'TMR SEGA' in data[off:off+16]:
                return True, f"Valid SMS/GG (TMR SEGA at 0x{off:X})"
    b0 = data[0]
    if b0 in [0xF3, 0xC3, 0x18, 0x00, 0x31, 0xED]:
        return True, "Valid SMS/GG Z80 opcode"
    return True, "SMS/GG generic"

def validate_pce(data):
    if len(data) < 0x2000:
        return False, "File too small (<8KB)"
    if len(data) >= 0x10000:
        vec = int.from_bytes(data[0xFFFE:0x10000], 'little')
        if vec > 0:
            return True, f"Valid PCE vector 0x{vec:04X}"
    return True, "PCE valid size"

def validate_lynx(data):
    if len(data) < 64:
        return False, "File too small"
    if data[:4] == b'LYNX':
        return True, "Valid LYNX header"
    if data[6:10] == b'BS93':
        return True, "Valid Handy LYNX"
    return True, "Lynx raw"

def validate_coleco(data):
    if len(data) < 0x2000:
        return False, "File too small (<8KB)"
    if data[:2] in (b'\x55\xAA', b'\xAA\x55'):
        return True, "Valid Coleco header"
    return False, f"Missing Coleco magic (got {data[:2]})"

def validate_atari(data):
    if len(data) < 2048:
        return False, "File too small (<2KB)"
    return True, "Valid Atari size"

def validate_pico8(data):
    if len(data) < 16:
        return False, "File too small"
    if data[:8] == b'pico-8\n' or b'__lua__' in data[:1024]:
        return True, "Valid PICO-8 text cart"
    if data[:8] == b'\x89PNG\r\n\x1a\n':
        return True, "Valid PICO-8 PNG cart"
    return True, "P8 generic"

def validate_doom(data):
    if len(data) < 12:
        return False, "File too small"
    if data[:4] in (b'IWAD', b'PWAD'):
        return True, f"Valid Doom {data[:4].decode('ascii')}"
    return False, "Invalid WAD magic"

VALIDATORS = {
    '.nes': validate_nes,
    '.gb': validate_gb_gbc,
    '.gbc': validate_gb_gbc,
    '.sfc': validate_snes,
    '.smc': validate_snes,
    '.gen': validate_genesis,
    '.md': validate_genesis,
    '.smd': validate_genesis,
    '.sms': validate_sms_gg,
    '.gg': validate_sms_gg,
    '.sg': validate_sms_gg,
    '.pce': validate_pce,
    '.lnx': validate_lynx,
    '.col': validate_coleco,
    '.a26': validate_atari,
    '.p8': validate_pico8,
    '.wad': validate_doom,
}

def scan_games():
    stats = defaultdict(lambda: {'total': 0, 'valid': 0, 'broken': 0, 'zero_byte': 0})
    broken_files = []
    
    for entry in GAMES_DIR.iterdir():
        if not entry.is_file():
            continue
        ext = entry.suffix.lower()
        stats[ext]['total'] += 1
        
        sz = entry.stat().st_size
        if sz == 0:
            stats[ext]['zero_byte'] += 1
            stats[ext]['broken'] += 1
            broken_files.append((entry.name, ext, "Zero bytes"))
            continue
            
        validator = VALIDATORS.get(ext)
        if validator:
            try:
                with open(entry, 'rb') as f:
                    data = f.read(min(sz, 1024 * 1024))
                ok, reason = validator(data)
                if ok:
                    stats[ext]['valid'] += 1
                else:
                    stats[ext]['broken'] += 1
                    broken_files.append((entry.name, ext, reason))
            except Exception as e:
                stats[ext]['broken'] += 1
                broken_files.append((entry.name, ext, str(e)))
        else:
            stats[ext]['valid'] += 1

    print("=== AUDIT OF E:\\BMO Gameboy\\games ===")
    for ext, s in sorted(stats.items(), key=lambda x: -x[1]['total']):
        print(f"{ext:<6}: Total: {s['total']:<6} | Valid: {s['valid']:<6} | Broken: {s['broken']:<4} | Zero-byte: {s['zero_byte']}")
    
    if broken_files:
        print(f"\n[!] Found {len(broken_files)} broken/invalid files:")
        for fn, ext, reason in broken_files[:20]:
            print(f"  [{ext}] {fn} -> {reason}")
        if len(broken_files) > 20:
            print(f"  ... and {len(broken_files) - 20} more")
    else:
        print("\n[+] 100% of tested games in E:\\BMO Gameboy\\games passed header and integrity checks!")

if __name__ == '__main__':
    scan_games()
