"""
Unit tests for BMO Handheld Console Binary ROM Index Cache (.bmo_index — BMOIDX03)
Tests binary header structure, variable-length path string table, 4 KiB ROM prefix
CRC32 identity, IEEE CRC32 payload validation, and BMOIDX02 legacy detection.
"""

import struct
import unittest
import zlib

BMO_INDEX_MAGIC = b"BMOIDX03"
BMO_INDEX_LEGACY_MAGIC_V2 = b"BMOIDX02"
BMO_INDEX_VERSION = 3

# struct BmoIndexHeader (44 bytes):
# char magic[8] (8s) + 9 uint32_t fields
HEADER_FORMAT = "<8s9I"
HEADER_SIZE = 44

# struct BmoIndexEntry (20 bytes):
# uint32_t pathOffset (I), uint16_t pathLength (H), uint8_t type (B), uint8_t flags (B),
# uint32_t fileSize (I), uint32_t crc32Prefix (I), uint32_t reserved (I)
ENTRY_FORMAT = "<IHBBIII"
ENTRY_SIZE = 20

FLAG_FAVORITE = 0x01
FLAG_BOX_ART = 0x02
FLAG_POPULAR = 0x04


def compute_crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def compute_fingerprint(
    entry_count: int, string_table_bytes: int, total_bytes: int, root_stamp: int, payload_crc: int
) -> int:
    words = struct.pack(
        "<IIIIII",
        entry_count & 0xFFFFFFFF,
        string_table_bytes & 0xFFFFFFFF,
        total_bytes & 0xFFFFFFFF,
        (total_bytes >> 32) & 0xFFFFFFFF,
        root_stamp & 0xFFFFFFFF,
        payload_crc & 0xFFFFFFFF,
    )
    return compute_crc32(words)


def build_index_v3(
    records: list[tuple[str, int, bool, bool, int, int]],
    timestamp: int = 1700000000,
) -> bytes:
    entries = bytearray()
    string_table = bytearray()
    total_bytes = 0
    for rel_path, rom_type, is_fav, has_art, file_size, crc32_prefix in records:
        path_bytes = rel_path.encode("utf-8")
        path_offset = len(string_table)
        path_length = len(path_bytes)
        string_table.extend(path_bytes)
        string_table.append(0)
        flags = (FLAG_FAVORITE if is_fav else 0) | (FLAG_BOX_ART if has_art else 0)
        entries.extend(
            struct.pack(
                ENTRY_FORMAT,
                path_offset,
                path_length,
                rom_type,
                flags,
                file_size,
                crc32_prefix & 0xFFFFFFFF,
                0,
            )
        )
        total_bytes += file_size

    payload = bytes(entries) + bytes(string_table)
    payload_crc = compute_crc32(payload)
    fp = compute_fingerprint(len(records), len(string_table), total_bytes, timestamp, payload_crc)
    header = struct.pack(
        HEADER_FORMAT,
        BMO_INDEX_MAGIC,
        BMO_INDEX_VERSION,
        len(records),
        ENTRY_SIZE,
        len(string_table),
        payload_crc,
        timestamp,
        total_bytes & 0xFFFFFFFF,
        (total_bytes >> 32) & 0xFFFFFFFF,
        fp,
    )
    return header + payload


def unpack_header(data: bytes) -> dict:
    magic, ver, count, entry_size, str_bytes, crc, ts, tot_lo, tot_hi, fp = struct.unpack(
        HEADER_FORMAT, data[:HEADER_SIZE]
    )
    return {
        "magic": magic,
        "version": ver,
        "entry_count": count,
        "entry_size": entry_size,
        "string_table_bytes": str_bytes,
        "crc32": crc,
        "timestamp": ts,
        "total_bytes": (tot_hi << 32) | tot_lo,
        "fingerprint": fp,
    }


def unpack_entry(data: bytes, string_table: bytes) -> dict:
    off, length, rom_type, flags, file_size, prefix_crc, reserved = struct.unpack(
        ENTRY_FORMAT, data[:ENTRY_SIZE]
    )
    name = string_table[off : off + length].decode("utf-8", errors="strict")
    return {
        "path_offset": off,
        "path_length": length,
        "filename": name,
        "type": rom_type,
        "is_favorite": bool(flags & FLAG_FAVORITE),
        "has_box_art": bool(flags & FLAG_BOX_ART),
        "is_popular": bool(flags & FLAG_POPULAR),
        "file_size": file_size,
        "crc32_prefix": prefix_crc,
        "reserved": reserved,
    }


class RomIndexBinaryTests(unittest.TestCase):
    def test_header_and_entry_sizes(self):
        self.assertEqual(struct.calcsize(HEADER_FORMAT), HEADER_SIZE)
        self.assertEqual(struct.calcsize(ENTRY_FORMAT), ENTRY_SIZE)

    def test_full_v3_index_with_nested_long_paths_and_prefix_crc(self):
        long_path = "nes/action/classics/" + ("sub_" * 20) + "Contra (USA).nes"
        self.assertGreater(len(long_path), 90)
        blob = build_index_v3(
            [
                ("gb/Super Mario Land.gb", 2, True, True, 65536, 0x11223344),
                (long_path, 4, False, True, 262144, 0xAABBCCDD),
                ("wad/doom1.wad", 5, True, False, 4196020, 0xCAFEBABE),
            ]
        )
        hdr = unpack_header(blob)
        self.assertEqual(hdr["magic"], BMO_INDEX_MAGIC)
        self.assertEqual(hdr["version"], BMO_INDEX_VERSION)
        self.assertEqual(hdr["entry_count"], 3)
        self.assertEqual(hdr["entry_size"], ENTRY_SIZE)
        self.assertEqual(hdr["total_bytes"], 65536 + 262144 + 4196020)

        payload = blob[HEADER_SIZE:]
        self.assertEqual(compute_crc32(payload), hdr["crc32"])

        entries_end = hdr["entry_count"] * ENTRY_SIZE
        string_table = payload[entries_end:]
        self.assertEqual(len(string_table), hdr["string_table_bytes"])

        second = unpack_entry(payload[ENTRY_SIZE : 2 * ENTRY_SIZE], string_table)
        self.assertEqual(second["filename"], long_path)
        self.assertEqual(second["crc32_prefix"], 0xAABBCCDD)
        self.assertTrue(second["has_box_art"])
        self.assertFalse(second["is_favorite"])

    def test_corrupted_payload_detection(self):
        blob = bytearray(
            build_index_v3([("gb/Test.gb", 2, False, False, 32768, 0x01020304)])
        )
        hdr = unpack_header(bytes(blob))
        blob[HEADER_SIZE + 5] ^= 0xFF
        self.assertNotEqual(compute_crc32(bytes(blob[HEADER_SIZE:])), hdr["crc32"])

    def test_mismatched_magic_and_legacy_v2_recognition(self):
        blob = build_index_v3([("gb/Test.gb", 2, False, False, 32768, 1)])
        legacy_header = BMO_INDEX_LEGACY_MAGIC_V2 + blob[8:HEADER_SIZE]
        parsed = unpack_header(legacy_header)
        self.assertEqual(parsed["magic"], BMO_INDEX_LEGACY_MAGIC_V2)
        self.assertNotEqual(parsed["magic"], BMO_INDEX_MAGIC)

    def test_catalog_fingerprint_encoding_matches_cpp(self):
        fp = compute_fingerprint(3, 96, 0x0102030405, 1700000000, 0x12345678)
        self.assertEqual(fp, 0x348E1C58)

    def test_alphabetical_sorting(self):
        titles = ["Zelda.gb", "Aladdin.gbc", "Mario.gb", "Batman.nes"]
        sorted_titles = sorted(titles, key=lambda s: s.lower())
        self.assertEqual(sorted_titles, ["Aladdin.gbc", "Batman.nes", "Mario.gb", "Zelda.gb"])


if __name__ == "__main__":
    unittest.main()
