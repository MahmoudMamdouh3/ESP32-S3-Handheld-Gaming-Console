import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from scripts import deploy_sd_complete
from scripts.deploy_to_sd import (
    copy_single_file,
    deployment_plan,
    deployment_tasks,
    validate_target_collisions,
)
from scripts.rom_key import make_rom_key


class TestSdDeployment(unittest.TestCase):
    def test_task_plan_only_includes_firmware_rom_and_box_art_formats(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            games = root / "games"
            art = root / "art"
            destination = root / "card"
            for directory in (games, art, destination):
                directory.mkdir()
            (games / "demo.nes").write_bytes(b"rom")
            (games / "nested").mkdir()
            (games / "nested" / "other.nes").write_bytes(b"nested rom")
            (games / "notes.txt").write_text("not a ROM")
            (art / "demo.raw").write_bytes(bytes(8192))
            (art / "demo.png").write_bytes(b"not firmware box art")

            tasks = deployment_tasks(games, art, destination)

            self.assertEqual(
                [(src.name, dst.relative_to(destination).as_posix()) for src, dst in tasks],
                [
                    ("demo.nes", "demo.nes"),
                    ("other.nes", "nested/other.nes"),
                    ("demo.raw", f"boxart/{make_rom_key('demo.nes')}.raw"),
                ],
            )

    def test_rom_path_accepts_nested_long_paths_and_rejects_over_255_bytes(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            games = root / "games"
            art = root / "art"
            destination = root / "card"
            for directory in (games, art, destination):
                directory.mkdir()
            sub = games / ("a" * 40) / ("b" * 40)
            sub.mkdir(parents=True)
            nested_ok = sub / "demo.nes"
            nested_ok.write_bytes(b"rom")
            tasks = deployment_tasks(games, art, destination)
            self.assertEqual(len(tasks), 1)

            too_deep = games / ("x" * 130) / ("y" * 130)
            too_deep.mkdir(parents=True)
            (too_deep / "too_long.nes").write_bytes(b"rom")

            with self.assertRaisesRegex(ValueError, "too long"):
                deployment_tasks(games, art, destination)

    def test_task_plan_rejects_case_insensitive_filename_collisions(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            games = root / "games"
            art = root / "art"
            destination = root / "card"
            for directory in (games, art, destination):
                directory.mkdir()

            with self.assertRaisesRegex(ValueError, "collision"):
                validate_target_collisions(
                    [
                        (games / "Demo.nes", destination / "Demo.nes"),
                        (games / "demo.NES", destination / "demo.NES"),
                    ],
                    destination,
                )

    def test_identical_files_are_verified_and_skipped(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "source.bin"
            destination = root / "destination.bin"
            source.write_bytes(b"verified data")
            destination.write_bytes(b"verified data")

            copied, size = copy_single_file(source, destination)

            self.assertFalse(copied)
            self.assertEqual(size, source.stat().st_size)
            self.assertEqual(destination.read_bytes(), source.read_bytes())

    def test_plan_distinguishes_matching_files_from_missing_or_changed_files(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "source.bin"
            matching = root / "matching.bin"
            changed = root / "changed.bin"
            missing = root / "missing.bin"
            source.write_bytes(b"new source")
            matching.write_bytes(b"new source")
            changed.write_bytes(b"old")

            updates, unchanged, update_bytes = deployment_plan(
                [(source, matching), (source, changed), (source, missing)]
            )

            self.assertEqual(unchanged, 1)
            self.assertEqual({dst.name for _, dst in updates}, {"changed.bin", "missing.bin"})
            self.assertEqual(update_bytes, source.stat().st_size * 2)

    def test_different_existing_file_is_atomically_replaced_and_verified(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "source.bin"
            destination = root / "destination.bin"
            source.write_bytes(b"new payload")
            destination.write_bytes(b"old payload")

            copied, size = copy_single_file(source, destination)

            self.assertTrue(copied)
            self.assertEqual(size, source.stat().st_size)
            self.assertEqual(destination.read_bytes(), source.read_bytes())
            self.assertEqual(list(root.glob(".bmo-copy-*.tmp")), [])

    def test_deploy_sd_complete_uses_bmo_index_v3_and_rejects_unsupported_extensions(self):
        self.assertNotIn(".a78", deploy_sd_complete.EXT_MAP)
        self.assertNotIn(".sgx", deploy_sd_complete.EXT_MAP)

        with tempfile.TemporaryDirectory() as temp_dir:
            dest_root = Path(temp_dir)
            games = [("gb/Mario.gb", 2, 1024, 0xAABBCCDD), ("nes/Contra.nes", 4, 2048, 0x11223344)]
            keyed = {make_rom_key("gb/Mario.gb").lower()}
            with patch.object(deploy_sd_complete, "DEST_ROOT", dest_root):
                deploy_sd_complete.generate_binary_index(games, keyed)

            payload = (dest_root / ".bmo_index").read_bytes()
            magic, ver, count, entry_size, str_bytes, crc, ts, tot_lo, tot_hi, fp = struct.unpack(
                "<8s9I", payload[:44]
            )
            self.assertEqual(magic, b"BMOIDX03")
            self.assertEqual(ver, 3)
            self.assertEqual(count, 2)
            self.assertEqual(entry_size, 20)
            self.assertEqual(len(payload), 44 + 2 * 20 + str_bytes)
            self.assertEqual(ts, 0)
            self.assertEqual(tot_lo, 3072)
            self.assertEqual(tot_hi, 0)
            self.assertEqual(
                fp,
                deploy_sd_complete.catalog_fingerprint(2, str_bytes, 3072, 0, crc),
            )
            first_entry = struct.unpack("<IHBBIII", payload[44:64])
            # Contra.nes sorts before Mario.gb by basename.
            contra_off, contra_len, contra_type, contra_flags, contra_sz, contra_prefix, contra_res = first_entry
            self.assertEqual(contra_type, 4)
            self.assertEqual(contra_flags, 0)
            self.assertEqual(contra_sz, 2048)
            self.assertEqual(contra_prefix, 0x11223344)
            self.assertEqual(contra_res, 0)
            str_table = payload[44 + 2 * 20 :]
            self.assertEqual(
                str_table[contra_off : contra_off + contra_len].decode("utf-8"),
                "nes/Contra.nes",
            )


if __name__ == "__main__":
    unittest.main()
