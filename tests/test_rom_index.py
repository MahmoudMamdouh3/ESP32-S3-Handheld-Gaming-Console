#!/usr/bin/env python3
"""
Unit tests for BMO Handheld Console Binary ROM Index Cache (.bmo_index)
Tests binary header structure, entry packing, CRC32 verification,
and index serialization/deserialization.
"""

import struct
import zlib
import unittest

BMO_INDEX_MAGIC = b"BMOIDX01"
BMO_INDEX_VERSION = 1
HEADER_FORMAT = "<8s6I"      # 8s (magic), version, entryCount, entrySize, crc32, timestamp, reserved[0]... (36 bytes: 8s + 6*4)
# Wait, struct BmoIndexHeader:
# char magic[8] (8s)
# uint32_t version (I)
# uint32_t entryCount (I)
# uint32_t entrySize (I)
# uint32_t crc32 (I)
# uint32_t timestamp (I)
# uint32_t reserved[2] (2I)
# Total: 8 + 4*7 = 36 bytes.
HEADER_FORMAT = "<8s7I"
HEADER_SIZE = 36

# struct BmoIndexEntry:
# char filename[64] (64s)
# uint8_t type (B)
# uint8_t isFavorite (B)
# uint8_t hasBoxArt (B)
# uint8_t reserved (B)
# uint32_t fileSize (I)
# Total: 64 + 4 + 4 = 72 bytes.
ENTRY_FORMAT = "<64s4BI"
ENTRY_SIZE = 72


def compute_crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def pack_header(entry_count: int, crc32: int, version: int = BMO_INDEX_VERSION, timestamp: int = 0) -> bytes:
    return struct.pack(
        HEADER_FORMAT,
        BMO_INDEX_MAGIC,
        version,
        entry_count,
        ENTRY_SIZE,
        crc32,
        timestamp,
        0,  # reserved[0]
        0   # reserved[1]
    )


def pack_entry(filename: str, rom_type: int, is_favorite: bool, has_box_art: bool, file_size: int = 0) -> bytes:
    encoded_name = filename.encode('utf-8')[:63]
    return struct.pack(
        ENTRY_FORMAT,
        encoded_name,
        rom_type,
        1 if is_favorite else 0,
        1 if has_box_art else 0,
        0,  # reserved
        file_size
    )


def unpack_header(data: bytes):
    if len(data) != HEADER_SIZE:
        raise ValueError(f"Invalid header size: {len(data)} != {HEADER_SIZE}")
    magic, ver, count, entry_size, crc, ts, r0, r1 = struct.unpack(HEADER_FORMAT, data)
    return {
        "magic": magic,
        "version": ver,
        "entry_count": count,
        "entry_size": entry_size,
        "crc32": crc,
        "timestamp": ts,
    }


def unpack_entry(data: bytes):
    if len(data) != ENTRY_SIZE:
        raise ValueError(f"Invalid entry size: {len(data)} != {ENTRY_SIZE}")
    raw_name, rom_type, is_fav, has_art, reserved, file_size = struct.unpack(ENTRY_FORMAT, data)
    name = raw_name.split(b'\x00')[0].decode('utf-8', errors='replace')
    return {
        "filename": name,
        "type": rom_type,
        "is_favorite": bool(is_fav),
        "has_box_art": bool(has_art),
        "file_size": file_size
    }


class TestRomIndexBinary(unittest.TestCase):
    def test_header_size(self):
        self.assertEqual(struct.calcsize(HEADER_FORMAT), 36)

    def test_entry_size(self):
        self.assertEqual(struct.calcsize(ENTRY_FORMAT), 72)

    def test_header_pack_unpack(self):
        header_bytes = pack_header(entry_count=100, crc32=0x12345678, timestamp=1690000000)
        self.assertEqual(len(header_bytes), 36)
        parsed = unpack_header(header_bytes)
        self.assertEqual(parsed["magic"], BMO_INDEX_MAGIC)
        self.assertEqual(parsed["version"], 1)
        self.assertEqual(parsed["entry_count"], 100)
        self.assertEqual(parsed["entry_size"], 72)
        self.assertEqual(parsed["crc32"], 0x12345678)
        self.assertEqual(parsed["timestamp"], 1690000000)

    def test_entry_pack_unpack(self):
        entry_bytes = pack_entry("Pokemon - Red Version.gb", rom_type=2, is_favorite=True, has_box_art=True, file_size=1048576)
        self.assertEqual(len(entry_bytes), 72)
        parsed = unpack_entry(entry_bytes)
        self.assertEqual(parsed["filename"], "Pokemon - Red Version.gb")
        self.assertEqual(parsed["type"], 2)
        self.assertTrue(parsed["is_favorite"])
        self.assertTrue(parsed["has_box_art"])
        self.assertEqual(parsed["file_size"], 1048576)

    def test_full_index_crc_validation(self):
        entries = [
            pack_entry("Aladdin.gbc", rom_type=3, is_favorite=False, has_box_art=True),
            pack_entry("Doom.wad", rom_type=5, is_favorite=True, has_box_art=False),
            pack_entry("Super Mario Land.gb", rom_type=2, is_favorite=True, has_box_art=True),
        ]
        entries_data = b"".join(entries)
        crc = compute_crc32(entries_data)
        header = pack_header(len(entries), crc)
        full_index = header + entries_data

        # Parse header
        hdr = unpack_header(full_index[:36])
        self.assertEqual(hdr["magic"], BMO_INDEX_MAGIC)
        self.assertEqual(hdr["entry_count"], 3)

        # Validate CRC
        payload = full_index[36:]
        self.assertEqual(compute_crc32(payload), hdr["crc32"])

    def test_corrupted_payload_detection(self):
        entries = [pack_entry("Test.gb", rom_type=2, is_favorite=False, has_box_art=False)]
        entries_data = b"".join(entries)
        crc = compute_crc32(entries_data)
        full_index = bytearray(pack_header(1, crc) + entries_data)

        # Corrupt 1 byte in payload
        full_index[50] ^= 0xFF
        corrupted_payload = bytes(full_index[36:])
        self.assertNotEqual(compute_crc32(corrupted_payload), crc)

    def test_mismatched_magic_rejection(self):
        corrupted_header = b"BADMAGIC" + pack_header(10, 0)[8:]
        parsed = unpack_header(corrupted_header)
        self.assertNotEqual(parsed["magic"], BMO_INDEX_MAGIC)

    def test_version_validation(self):
        header_v2 = pack_header(10, 0, version=2)
        parsed = unpack_header(header_v2)
        self.assertEqual(parsed["version"], 2)
        self.assertNotEqual(parsed["version"], BMO_INDEX_VERSION)

    def test_alphabetical_sorting(self):
        titles = ["Zelda.gb", "Aladdin.gbc", "Mario.gb", "Batman.nes"]
        sorted_titles = sorted(titles, key=lambda s: s.lower())
        self.assertEqual(sorted_titles, ["Aladdin.gbc", "Batman.nes", "Mario.gb", "Zelda.gb"])


if __name__ == "__main__":
    unittest.main()
