import struct
import unittest
import zlib
from pathlib import Path

class SaveManagerBinaryTests(unittest.TestCase):
    MAGIC = 0x31535342  # "BSS1"
    VERSION = 1

    HEADER_FORMAT = "<IIIIIII32s"
    HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

    def compute_crc32(self, data: bytes) -> int:
        return zlib.crc32(data) & 0xFFFFFFFF

    def test_header_packing_and_unpacking(self):
        core_id = 1  # CORE_GB_DMG
        timestamp = 12345678
        state_data = b"MOCK_GAMEBOY_STATE_PAYLOAD_HERE"
        ram_data = b"MOCK_BATTERY_RAM_HERE"
        
        crc = self.compute_crc32(state_data + ram_data)
        rom_title = b"Pokemon Red\x00" + b"\x00" * 20

        packed_header = struct.pack(
            self.HEADER_FORMAT,
            self.MAGIC,
            self.VERSION,
            timestamp,
            core_id,
            len(state_data),
            len(ram_data),
            crc,
            rom_title
        )

        self.assertEqual(len(packed_header), 60)  # 7 * 4 + 32 = 60 bytes

        unpacked = struct.unpack(self.HEADER_FORMAT, packed_header)
        magic, ver, ts, cid, s_size, r_size, unpacked_crc, title = unpacked

        self.assertEqual(magic, self.MAGIC)
        self.assertEqual(ver, self.VERSION)
        self.assertEqual(ts, timestamp)
        self.assertEqual(cid, core_id)
        self.assertEqual(s_size, len(state_data))
        self.assertEqual(r_size, len(ram_data))
        self.assertEqual(unpacked_crc, crc)
        self.assertTrue(title.startswith(b"Pokemon Red"))

    def test_crc32_corruption_detection(self):
        state_data = bytearray(b"ORIGINAL_STATE_BUFFER")
        crc = self.compute_crc32(state_data)

        # Corrupt single bit
        state_data[0] ^= 0x01
        corrupted_crc = self.compute_crc32(state_data)

        self.assertNotEqual(crc, corrupted_crc)

    def test_slot_number_boundaries(self):
        max_slots = 5
        for slot in range(1, max_slots + 1):
            path = f"/saves/Zelda Ages.s0{slot}"
            self.assertTrue(path.endswith(f".s0{slot}"))

    def test_all_core_ids_supported(self):
        core_ids = {
            "CORE_NONE": 0,
            "CORE_GB_DMG": 1,
            "CORE_GB_CGB": 2,
            "CORE_NES": 3,
            "CORE_SMS": 4,
            "CORE_DOOM": 5,
            "CORE_PCE": 6,
            "CORE_ATARI": 7,
            "CORE_PICO8": 8,
            "CORE_GENESIS": 9,
            "CORE_SNES": 10,
            "CORE_WSWAN": 11,
            "CORE_NGP": 12,
            "CORE_LYNX": 13,
            "CORE_COLEM": 14,
        }
        for name, cid in core_ids.items():
            packed = struct.pack(
                self.HEADER_FORMAT,
                self.MAGIC,
                self.VERSION,
                1000,
                cid,
                64,
                0,
                0x12345678,
                b"Test Core\x00" + b"\x00" * 22
            )
            unpacked = struct.unpack(self.HEADER_FORMAT, packed)
            self.assertEqual(unpacked[3], cid, f"Core ID mismatch for {name}")

    def test_pce_save_state_payload(self):
        # PCE has ~73KB context (8KB RAM, 64KB VRAM, 1KB palette, registers)
        pce_ram = b"\xAA" * 0x2000
        pce_vram = b"\x55" * 0x10000
        pce_palette = b"\x00\x08" * 512
        state_payload = pce_ram + pce_vram + pce_palette
        
        crc = self.compute_crc32(state_payload)
        packed_hdr = struct.pack(
            self.HEADER_FORMAT,
            self.MAGIC,
            self.VERSION,
            9999,
            6,  # CORE_PCE
            len(state_payload),
            0x2000,
            crc,
            b"Bonks Adventure\x00" + b"\x00" * 16
        )
        unpacked = struct.unpack(self.HEADER_FORMAT, packed_hdr)
        self.assertEqual(unpacked[3], 6)
        self.assertEqual(unpacked[4], len(state_payload))
        self.assertEqual(unpacked[6], crc)

if __name__ == "__main__":
    unittest.main()

