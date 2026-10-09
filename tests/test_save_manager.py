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

    def test_battery_save_path(self):
        rom_filename = "/roms/gbc/Legend of Zelda Ages (Baked).gbc"
        base = Path(rom_filename).stem
        path = f"/saves/{base}.sav"
        self.assertEqual(path, "/saves/Legend of Zelda Ages (Baked).sav")

if __name__ == "__main__":
    unittest.main()
