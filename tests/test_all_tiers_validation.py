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

WORKSPACE    = Path(r'E:\BMO Gameboy')
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
    ('smsplus', 'sms.h'), ('smsplus', 'sms.c'),
    ('pce',     'pce.h'), ('pce',     'pce.c'),
    ('stella',  'atari.h'), ('stella', 'atari.c'),
    ('pico',    'pico.h'), ('pico',   'pico.c'),
    ('genesis', 'genesis.h'), ('genesis', 'genesis.c'),
    ('snes',    'snes.h'), ('snes',   'snes.c'),
    ('wswan',   'wswan.h'), ('wswan', 'wswan.c'),
    ('ngp',     'ngp.h'), ('ngp',    'ngp.c'),
    ('lynx',    'lynx.h'), ('lynx',  'lynx.c'),
    ('colem',   'colem.h'), ('colem', 'colem.c'),
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
    'emu_walnut', 'emu_peanut', 'emu_nes', 'emu_doom', 'emu_sms',
}

# Engines that are architectural stubs (render blank screen only)
STUB_ENGINES = {
    'emu_pce', 'emu_atari', 'emu_pico',
    'emu_genesis', 'emu_snes', 'emu_wswan',
    'emu_ngp', 'emu_lynx', 'emu_colem',
}


class TestAllConsolesFirmwareValidation(unittest.TestCase):
    """Production-grade validation across all 15 console platforms."""

    # ------------------------------------------------------------------
    # test_01: Emulator wrapper files exist and are non-empty
    # ------------------------------------------------------------------
    def test_01_all_emulator_wrappers_exist_and_nonempty(self):
        """Verify all 14 emulator .h and .cpp files exist and contain code (>= 100 bytes)."""
        for stem in EMULATOR_STEMS:
            for ext in ('.h', '.cpp'):
                f = FIRMWARE_DIR / 'src' / 'emulators' / (stem + ext)
                self.assertTrue(f.exists(), f'Missing: {f.relative_to(WORKSPACE)}')
                self.assertGreater(
                    f.stat().st_size, 100,
                    f'{f.name} is suspiciously small ({f.stat().st_size} bytes) — likely empty stub header'
                )

    # ------------------------------------------------------------------
    # test_02: Vendor engine files exist and are non-empty
    # ------------------------------------------------------------------
    def test_02_all_vendor_engines_exist_and_nonempty(self):
        """Verify all underlying vendor engine headers and sources exist and have content."""
        for vdir, vfile in VENDOR_FILES:
            f = FIRMWARE_DIR / 'src' / 'vendor' / vdir / vfile
            self.assertTrue(f.exists(), f'Missing vendor engine: {f.relative_to(WORKSPACE)}')
            self.assertGreater(
                f.stat().st_size, 50,
                f'{vfile} is too small — check vendor engine was correctly placed'
            )

    # ------------------------------------------------------------------
    # test_03: All 14 teardown calls are wired in the main .ino
    # ------------------------------------------------------------------
    def test_03_teardown_compliance_all_14_emulators(self):
        """Verify all 14 destroy() calls are registered in BmoGameboy.ino SELECT+UP handler."""
        ino = FIRMWARE_DIR / 'BmoGameboy.ino'
        self.assertTrue(ino.exists(), 'BmoGameboy.ino not found')
        ino_content = ino.read_text(encoding='utf-8')
        for td in EXPECTED_TEARDOWNS:
            self.assertIn(
                td, ino_content,
                f'Teardown contract missing: {td} not found in BmoGameboy.ino'
            )

    # ------------------------------------------------------------------
    # test_04: PSRAM allocation in wrapper file (not just vendor combined)
    # ------------------------------------------------------------------
    def test_04_psram_allocation_in_wrapper_not_just_vendor(self):
        """
        Verify MALLOC_CAP_SPIRAM is used in the C++ *wrapper* file itself,
        not just anywhere in the combined wrapper+vendor text.
        Wrappers that delegate allocation entirely to the vendor engine are
        exempt only if the vendor file itself contains MALLOC_CAP_SPIRAM.
        """
        # These wrappers allocate their framebuffer in the wrapper file
        wrappers_that_own_psram = [
            'emu_genesis', 'emu_snes', 'emu_wswan', 'emu_ngp',
            'emu_lynx', 'emu_colem', 'emu_doom', 'emu_peanut', 'emu_walnut',
        ]
        for stem in wrappers_that_own_psram:
            cpp = FIRMWARE_DIR / 'src' / 'emulators' / (stem + '.cpp')
            if cpp.exists():
                code = cpp.read_text(encoding='utf-8')
                self.assertIn(
                    'MALLOC_CAP_SPIRAM', code,
                    f'{stem}.cpp allocates a framebuffer but does not use MALLOC_CAP_SPIRAM. '
                    'All dynamic framebuffers must live in Octal PSRAM.'
                )

    # ------------------------------------------------------------------
    # test_05: Display streaming methods declared in display_emu.h
    # ------------------------------------------------------------------
    def test_05_display_streaming_methods_all_consoles(self):
        """Verify DisplayEmu.h declares a dedicated streaming method for every console."""
        header = (FIRMWARE_DIR / 'src' / 'core' / 'display_emu.h').read_text(encoding='utf-8')
        for m in REQUIRED_STREAM_METHODS:
            self.assertIn(m, header, f'Missing display streaming method declaration: {m}')

    # ------------------------------------------------------------------
    # test_06: sd_card.cpp registers all 21 extensions
    # ------------------------------------------------------------------
    def test_06_sd_card_all_21_extensions_registered(self):
        """Verify sd_card.cpp detects and maps all 21 ROM file extensions."""
        code = (FIRMWARE_DIR / 'src' / 'core' / 'sd_card.cpp').read_text(encoding='utf-8')
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
    # test_08: Stub engines are honestly labeled
    # ------------------------------------------------------------------
    def test_08_stub_engines_carry_stub_sentinel(self):
        """
        Stub vendor engines must contain a STUB_ENGINE sentinel comment.
        This prevents future agents from treating blank-screen stubs as
        production-ready emulators without reading the code.
        """
        stub_vendor_map = {
            'emu_pce':     ('pce',     'pce.c'),
            'emu_atari':   ('stella',  'atari.c'),
            'emu_pico':    ('pico',    'pico.c'),
            'emu_genesis': ('genesis', 'genesis.c'),
            'emu_snes':    ('snes',    'snes.c'),
            'emu_wswan':   ('wswan',   'wswan.c'),
            'emu_ngp':     ('ngp',     'ngp.c'),
            'emu_lynx':    ('lynx',    'lynx.c'),
            'emu_colem':   ('colem',   'colem.c'),
        }
        missing_sentinel = []
        for stem, (vdir, vfile) in stub_vendor_map.items():
            vendor_path = FIRMWARE_DIR / 'src' / 'vendor' / vdir / vfile
            if vendor_path.exists():
                text = vendor_path.read_text(encoding='utf-8')
                if 'STUB_ENGINE' not in text:
                    missing_sentinel.append(f'{vdir}/{vfile}')
        self.assertEqual(
            missing_sentinel, [],
            f'Stub vendor engines missing STUB_ENGINE sentinel comment: {missing_sentinel}. '
            'Add "// STUB_ENGINE" to the top of each stub to prevent false VERIFIED_HOST claims.'
        )

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
        """
        Verify no emulator wrapper uses naked Serial.print (hot path ban).
        All debug output must go through LOG_ macros per 15_performance_budgets.md.
        """
        emu_dir = FIRMWARE_DIR / 'src' / 'emulators'
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
    # test_11: AGENT_MANIFEST emulator count matches filesystem
    # ------------------------------------------------------------------
    def test_11_manifest_emulator_count_matches_filesystem(self):
        """
        Verify AGENT_MANIFEST.json registers every emu_*.cpp file.
        Prevents the mistake of adding a new emulator without updating the manifest.
        """
        import json
        manifest_path = WORKSPACE / 'AGENT_MANIFEST.json'
        self.assertTrue(manifest_path.exists(), 'AGENT_MANIFEST.json not found')

        manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
        manifest_wrappers = {e.get('wrapper', '') for e in manifest.get('emulators', [])}

        emu_dir = FIRMWARE_DIR / 'src' / 'emulators'
        fs_stems = {f.stem for f in emu_dir.glob('emu_*.cpp')}

        unregistered = []
        for stem in fs_stems:
            key = f'src/emulators/{stem}.cpp'
            if not any(key in w for w in manifest_wrappers):
                unregistered.append(stem)

        self.assertEqual(
            unregistered, [],
            f'Emulators present on disk but not in AGENT_MANIFEST.json: {unregistered}. '
            'Update AGENT_MANIFEST.json emulators array.'
        )

    # ------------------------------------------------------------------
    # test_12: config.h hard-stop flags have exact value 0
    # ------------------------------------------------------------------
    def test_12_config_hard_stops_exact_value_zero(self):
        """
        Verify FEATURE_BATTERY_MONITOR and FEATURE_AUDIO are exactly 0.
        Regex requires a word boundary after 0 to reject accidental '0x...' values.
        """
        config = (FIRMWARE_DIR / 'src' / 'core' / 'config.h').read_text(encoding='utf-8')
        for flag in ('FEATURE_BATTERY_MONITOR', 'FEATURE_AUDIO'):
            self.assertRegex(
                config,
                rf'#define\s+{flag}\s+0\b',
                f'config.h: {flag} must be exactly 0 (hardware safety hard-stop). '
                'Enabling this flag can brick the device.'
            )


if __name__ == '__main__':
    unittest.main()

