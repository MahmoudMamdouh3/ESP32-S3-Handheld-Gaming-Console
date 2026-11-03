import tempfile
import unittest
from pathlib import Path

from scripts.deploy_to_sd import (
    copy_single_file,
    deployment_plan,
    deployment_tasks,
    validate_target_collisions,
)


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
            (games / "notes.txt").write_text("not a ROM")
            (art / "demo.raw").write_bytes(bytes(8192))
            (art / "demo.png").write_bytes(b"not firmware box art")

            tasks = deployment_tasks(games, art, destination)

            self.assertEqual(
                [(src.name, dst.relative_to(destination).as_posix()) for src, dst in tasks],
                [("demo.nes", "demo.nes"), ("demo.raw", "boxart/demo.raw")],
            )

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


if __name__ == "__main__":
    unittest.main()
