#!/usr/bin/env python3
"""
test_all_tiers_validation.py
Master test suite for all 15 console platforms.

Design principle: tests must assert *behavior contracts*, not just file existence.
A test that checks whether a file *exists* but not whether its content is valid
is a useless test — it passes even when the file is empty or wrong.

Tests are numbered to make failure output easy to trace in CI logs.
"""

import os
import re
import unittest
from pathlib import Path

WORKSPACE    = Path(__file__).resolve().parents[1]
FIRMWARE_DIR = WORKSPACE / 'firmware' / 'BmoGameboy'
GAMES_DIR    = WORKSPACE / 'games'

# Canonical lists — single source of truth for the test suite
EMULATOR_STEMS = [
    'emu_walnut', 'emu_peanut', 'emu_nes',     'emu_doom',
    'emu_sms',    'emu_pce',    'emu_atari',    'emu_pico',
    'emu_genesis','emu_snes',   'emu_wswan',    'emu_ngp',
    'emu_lynx',   'emu_colem',
]

EXPECTED_TEARDOWNS = [
    'WalnutEmu::destroy()', 'PeanutEmu::destroy()',
    'NesEmu::destroy()',    'DoomEmu::destroy()',
    'SmsEmu::destroy()',    'PceEmu::destroy()',
    'AtariEmu::destroy()',  'PicoEmu::destroy()',
    'GenesisEmu::destroy()', 'SNESEmu::destroy()',
    'WSwanEmu::destroy()',  'NGPEmu::destroy()',
    'LynxEmu::destroy()',   'ColemEmu::destroy()',
]

VENDOR_FILES = [
    ('peanut_gb',  'peanut_gb.h'),
    ('walnut_cgb', 'walnut_cgb.h'),
    ('agnes',      'agnes.h'), ('agnes', 'agnes.c'),
    ('doom',       'src/doomgeneric.h'), ('doom', 'src/doomgeneric.c'),
    ('smsplus',    'sms.h'), ('smsplus', 'sms.c'), ('smsplus', 'tms.c'), ('smsplus', 'smsplus_bmo.c'),
    ('huexpress',  'pce.h'), ('huexpress', 'pce.c'),
    ('stella2014', 'src/Console.h'), ('stella2014', 'src/Console.cpp'),
    ('fake08',     'src/vm.h'), ('fake08', 'src/vm.cpp'),
    ('gwenesis',   'bus/gwenesis_bus.h'), ('gwenesis', 'bus/gwenesis_bus.c'),
    ('snes9x',     'snes9x.h'), ('snes9x', 'src/cpuexec.c'),
    ('oswan',      'WS.h'), ('oswan', 'WS.c'),
    ('race',       'tlcs900h.h'), ('race', 'tlcs900h.c'),
    ('handy',      'handy.h'), ('handy', 'system.cpp'),
]

EXPECTED_EXTENSIONS = [
    '.gb', '.gbc', '.nes', '.wad',
    '.sms', '.gg', '.pce', '.a26', '.p8',
    '.gen', '.md', '.smd', '.sfc', '.smc',
    '.ws', '.wsc', '.ngp', '.ngc',
    '.lnx', '.col', '.sg',
]

REQUIRED_STREAM_METHODS = [
    'streamPixelRow', 'streamNESFrame', 'streamDoomFrame',
    'streamSMSFrame', 'streamPCEFrame', 'streamAtariFrame',
    'streamPicoFrame', 'streamGenesisFrame', 'streamSNESFrame',
    'streamWSwanFrame', 'streamNGPFrame', 'streamLynxFrame',
    'streamColemFrame',
]

# Engines known to be production-quality (real emulation logic)
PRODUCTION_ENGINES = {
    'emu_walnut', 'emu_peanut', 'emu_nes', 'emu_doom', 'emu_sms', 'emu_pce',
    'emu_atari', 'emu_pico', 'emu_genesis', 'emu_snes', 'emu_wswan',
    'emu_ngp', 'emu_lynx', 'emu_colem',
}

# Engines that are architectural stubs (render blank screen only)
STUB_ENGINES = set()


class TestAllConsolesFirmwareValidation(unittest.TestCase):
    """Validation across all 15 console platforms."""

    # ------------------------------------------------------------------
    # test_01: Emulator wrapper files exist and are non-empty
    # ------------------------------------------------------------------
    def test_01_all_emulator_wrappers_exist_and_nonempty(self):
        """Verify all 14 emulator .h and .cpp files exist and contain code (>= 100 bytes)."""
        for stem in EMULATOR_STEMS:
            for ext in ('.h', '.cpp'):
                f = FIRMWARE_DIR / 'src' / 'cores' / (stem + ext)
                self.assertTrue(f.exists(), f'Missing: {f.relative_to(WORKSPACE)}')
                self.assertGreater(
                    f.stat().st_size, 100,
                    f'{f.name} is suspiciously small ({f.stat().st_size} bytes) — likely empty stub header'
                )

    # ------------------------------------------------------------------
    # test_02: Genuine third-party engine files exist and are non-empty
    # ------------------------------------------------------------------
    def test_02_all_vendor_engines_exist_and_nonempty(self):
        """Verify all genuine third-party engine headers and sources exist and no legacy stub dir remains."""
        self.assertFalse(
            (FIRMWARE_DIR / 'src' / 'engines').exists(),
            'Legacy stub directory src/engines must be deleted',
        )
        for vdir, vfile in VENDOR_FILES:
            f = FIRMWARE_DIR / 'src' / 'third_party' / vdir / vfile
            self.assertTrue(f.exists(), f'Missing third-party engine: {f.relative_to(WORKSPACE)}')
            self.assertGreater(
                f.stat().st_size, 50,
                f'{vfile} is too small — check third-party engine was correctly placed'
            )

    # ------------------------------------------------------------------
    # test_03: All 14 teardown calls are wired via CoreRegistry + BmoGameboy.ino
    # ------------------------------------------------------------------
    def test_03_teardown_compliance_all_14_emulators(self):
        """Verify all 14 destroy symbols are registered in CoreRegistry and dispatched in BmoGameboy.ino."""
        ino = FIRMWARE_DIR / 'BmoGameboy.ino'
        reg = FIRMWARE_DIR / 'src' / 'cores' / 'core_registry.h'
        self.assertTrue(ino.exists(), 'BmoGameboy.ino not found')
        self.assertTrue(reg.exists(), 'core_registry.h not found')
        ino_content = ino.read_text(encoding='utf-8')
        reg_content = reg.read_text(encoding='utf-8')
        self.assertIn('CoreRegistry::findByIndex', ino_content)
        self.assertIn('core->destroy()', ino_content)
        for td in EXPECTED_TEARDOWNS:
            sym = td[:-2] if td.endswith('()') else td
            self.assertIn(
                sym, reg_content,
                f'Teardown contract missing: {sym} not found in core_registry.h'
            )

    # ------------------------------------------------------------------
    # test_04: PSRAM allocation in wrapper file (not just vendor combined)
    # ------------------------------------------------------------------
    def test_04_psram_allocation_in_wrapper_not_just_vendor(self):
        """
        Verify dynamic framebuffers in C++ wrappers allocate via MALLOC_CAP_SPIRAM or bmo_psram_*.
        """
        psram_h = (FIRMWARE_DIR / 'src' / 'hal' / 'psram_alloc.h').read_text(encoding='utf-8')
        self.assertIn('MALLOC_CAP_SPIRAM', psram_h)
        wrappers_that_own_psram = [
            'emu_genesis', 'emu_snes', 'emu_wswan', 'emu_ngp',
            'emu_lynx', 'emu_colem', 'emu_doom', 'emu_peanut', 'emu_walnut',
        ]
        for stem in wrappers_that_own_psram:
            cpp = FIRMWARE_DIR / 'src' / 'cores' / (stem + '.cpp')
            if cpp.exists():
                code = cpp.read_text(encoding='utf-8')
                uses_psram = (
                    'MALLOC_CAP_SPIRAM' in code
                    or 'bmo_psram_malloc' in code
                    or 'bmo_psram_calloc' in code
                )
                self.assertTrue(
                    uses_psram,
                    f'{stem}.cpp allocates a framebuffer but does not use MALLOC_CAP_SPIRAM or bmo_psram_*. '
                    'All dynamic framebuffers must live in Octal PSRAM.'
                )

    # ------------------------------------------------------------------
    # test_05: Display streaming methods declared in display_emu.h
    # ------------------------------------------------------------------
    def test_05_display_streaming_methods_all_consoles(self):
        """Verify DisplayEmu.h declares a dedicated streaming method for every console."""
        header = (FIRMWARE_DIR / 'src' / 'ui' / 'display_emu.h').read_text(encoding='utf-8')
        for m in REQUIRED_STREAM_METHODS:
            self.assertIn(m, header, f'Missing display streaming method declaration: {m}')

    # ------------------------------------------------------------------
    # test_06: sd_card.cpp registers all 21 extensions
    # ------------------------------------------------------------------
    def test_06_sd_card_all_21_extensions_registered(self):
        """Verify sd_card.cpp detects and maps all 21 ROM file extensions."""
        code = (FIRMWARE_DIR / 'src' / 'storage' / 'sd_card.cpp').read_text(encoding='utf-8')
        for ext in EXPECTED_EXTENSIONS:
            self.assertIn(ext, code, f'Extension "{ext}" not registered in sd_card.cpp')

    # ------------------------------------------------------------------
    # test_07: Installed game library health (if present)
    # ------------------------------------------------------------------
    def test_07_installed_games_catalogue_health(self):
        """If games/ exists, verify it contains healthy non-zero files."""
        if not GAMES_DIR.exists():
            return
        files = [f for f in GAMES_DIR.iterdir() if f.is_file()]
        self.assertGreater(len(files), 100,
                           'Games directory seems unexpectedly small')
        zero_byte = [f.name for f in files if f.stat().st_size == 0]
        self.assertEqual(len(zero_byte), 0,
                         f'Found 0-byte corrupt files in games/: {zero_byte[:5]}')

    # ------------------------------------------------------------------
    # test_08: All third-party engines are real production implementations
    # ------------------------------------------------------------------
    def test_08_stub_engines_carry_stub_sentinel(self):
        """
        Verify no remaining third-party engine is a blank-screen stub, and that all
        13 cartridge-based emulator wrappers implement the full persistence contract
        (saveBatteryRam, loadBatteryRam, saveState, loadState).
        """
        vendor_map = {
            'emu_sms':     ('smsplus',    'sms.c'),
            'emu_pce':     ('huexpress',  'pce.c'),
            'emu_atari':   ('stella2014', 'src/Console.cpp'),
            'emu_pico':    ('fake08',     'src/vm.cpp'),
            'emu_genesis': ('gwenesis',   'bus/gwenesis_bus.c'),
            'emu_snes':    ('snes9x',     'src/cpuexec.c'),
            'emu_wswan':   ('oswan',      'WS.c'),
            'emu_ngp':     ('race',       'tlcs900h.c'),
            'emu_lynx':    ('handy',      'system.cpp'),
            'emu_colem':   ('smsplus',    'tms.c'),
        }
        for stem, (vdir, vfile) in vendor_map.items():
            vendor_path = FIRMWARE_DIR / 'src' / 'third_party' / vdir / vfile
            self.assertTrue(vendor_path.exists(), f'Missing {vdir}/{vfile}')
            text = vendor_path.read_text(encoding='utf-8', errors='ignore')
            self.assertNotIn('STUB_ENGINE', text, f'{vdir}/{vfile} still marked STUB_ENGINE')
            self.assertGreater(len(text.splitlines()), 150, f'{vdir}/{vfile} too short to be a real engine')

        # Verify unified persistence contract across all 13 cartridge emulators
        cart_emulators = [s for s in EMULATOR_STEMS if s != 'emu_doom']
        for stem in cart_emulators:
            h_text = (FIRMWARE_DIR / 'src' / 'cores' / (stem + '.h')).read_text(encoding='utf-8')
            for method in ('saveBatteryRam', 'loadBatteryRam', 'saveState', 'loadState'):
                self.assertIn(method, h_text, f'{stem}.h missing persistence method {method}')

    # ------------------------------------------------------------------
    # test_09: Partition table math is valid
    # ------------------------------------------------------------------
    def test_09_partition_math_valid(self):
        """Parse partitions.csv and verify app0 >= 8MB and no partition overlaps."""
        csv_path = FIRMWARE_DIR / 'partitions.csv'
        self.assertTrue(csv_path.exists(), 'partitions.csv not found in sketch directory')

        partitions = {}
        for line in csv_path.read_text(encoding='utf-8').splitlines():
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            cols = [c.strip() for c in line.split(',')]
            if len(cols) < 5:
                continue
            name, raw_size = cols[0], cols[4]
            try:
                offset = int(cols[3], 16) if cols[3].startswith('0x') else int(cols[3])
                if raw_size.upper().endswith('M'):
                    size = int(raw_size[:-1]) * 1024 * 1024
                elif raw_size.startswith('0x') or raw_size.startswith('0X'):
                    size = int(raw_size, 16)
                else:
                    size = int(raw_size)
                partitions[name] = {'offset': offset, 'size': size}
            except ValueError:
                continue

        self.assertIn('app0', partitions, 'app0 partition not found in partitions.csv')
        app0_size = partitions['app0']['size']
        self.assertGreaterEqual(
            app0_size, 0x800000,
            f'app0 partition is {app0_size:,} bytes ({app0_size/1024/1024:.2f} MB) — '
            f'must be >= 8 MB (0x800000). '
            f'Select PartitionScheme=custom in Arduino IDE to avoid 158% flash overflow.'
        )

        # Check no overlaps
        sorted_parts = sorted(partitions.values(), key=lambda p: p['offset'])
        for i in range(len(sorted_parts) - 1):
            a, b = sorted_parts[i], sorted_parts[i + 1]
            self.assertLessEqual(
                a['offset'] + a['size'], b['offset'],
                f'Partition overlap: entry ending at 0x{a["offset"]+a["size"]:06X} '
                f'overlaps next entry at 0x{b["offset"]:06X}'
            )

    # ------------------------------------------------------------------
    # test_10: No naked Serial.print in emulator wrappers
    # ------------------------------------------------------------------
    def test_10_no_serial_print_in_emulator_wrappers(self):
        """Verify no emulator wrapper uses naked Serial.print (hot path ban)."""
        emu_dir = FIRMWARE_DIR / 'src' / 'cores'
        offenders = []
        for cpp in sorted(emu_dir.glob('*.cpp')):
            text = cpp.read_text(encoding='utf-8')
            if re.search(r'\bSerial\.print', text):
                offenders.append(cpp.name)
        self.assertEqual(
            offenders, [],
            f'Naked Serial.print found in emulator wrappers: {offenders}. '
            'Use LOG_INFO/LOG_ERROR macros instead.'
        )

    # ------------------------------------------------------------------
    # test_11: All wrappers on disk match EMULATOR_STEMS and docs/compatibility.md
    # ------------------------------------------------------------------
    def test_11_wrapper_count_and_compatibility_doc_match_filesystem(self):
        """Verify docs/compatibility.md lists every emu_*.cpp file on disk."""
        compat_text = (WORKSPACE / 'docs' / 'compatibility.md').read_text(encoding='utf-8')
        emu_dir = FIRMWARE_DIR / 'src' / 'cores'
        fs_stems = sorted(f.stem for f in emu_dir.glob('emu_*.cpp'))
        self.assertEqual(sorted(EMULATOR_STEMS), fs_stems)
        missing = [stem for stem in fs_stems if f'{stem}.cpp' not in compat_text]
        self.assertEqual(
            missing, [],
            f'Emulators on disk missing from docs/compatibility.md: {missing}',
        )

    # ------------------------------------------------------------------
    # test_12: config.h hard-stop flags have exact value 0
    # ------------------------------------------------------------------
    def test_12_config_hard_stops_exact_value_zero(self):
        """
        Verify FEATURE_BATTERY_MONITOR and FEATURE_AUDIO are exactly 0.
        Regex requires a word boundary after 0 to reject accidental '0x...' values.
        """
        config = (FIRMWARE_DIR / 'src' / 'hal' / 'config.h').read_text(encoding='utf-8')
        for flag in ('FEATURE_BATTERY_MONITOR', 'FEATURE_AUDIO'):
            self.assertRegex(
                config,
                rf'#define\s+{flag}\s+0\b',
                f'config.h: {flag} must be exactly 0 (hardware safety hard-stop). '
                'Enabling this flag can brick the device.'
            )


if __name__ == '__main__':
    unittest.main()

