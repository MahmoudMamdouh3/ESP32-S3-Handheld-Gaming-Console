"""Geometry and export checks for the parametric BMO enclosure."""

from __future__ import annotations

import itertools
import tempfile
import unittest
from pathlib import Path

import numpy as np
import trimesh

from tools.enclosure.generate_bmo_enclosure import CONFIG, build_parts, export_all, validate_mesh


EXPECTED_PARTS = {
    "bmo_front_shell",
    "bmo_back_shell",
    "bmo_battery_door",
    "bmo_dpad",
    "bmo_buttons_xyab",
    "bmo_buttons_start_select",
    "bmo_buttons_lr",
}


class EnclosureGeneratorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.parts = build_parts()

    def test_parametric_design_envelope_and_survey_state(self):
        self.assertEqual(CONFIG["physical_fit_status"], "WAITING")
        self.assertEqual(CONFIG["perfboard"]["survey_status"], "WAITING")
        self.assertEqual(
            (CONFIG["outer"]["width"], CONFIG["outer"]["height"], CONFIG["outer"]["thickness"]),
            (82.0, 124.0, 26.0),
        )
        self.assertAlmostEqual(CONFIG["shell"]["wall"], 2.1)
        self.assertEqual(set(CONFIG["display"]["active_window_profiles"]), {"2.0_in", "2.4_in", "2.8_in"})

    def test_all_parts_are_closed_positive_manifold_volumes(self):
        self.assertEqual(set(self.parts), EXPECTED_PARTS)
        for name, mesh in self.parts.items():
            with self.subTest(part=name):
                validate_mesh(mesh, name)
                self.assertTrue(mesh.is_watertight)
                self.assertTrue(mesh.is_winding_consistent)
                self.assertTrue(mesh.is_volume)
                self.assertGreater(mesh.volume, 0)
                self.assertTrue(np.isfinite(mesh.vertices).all())

    def test_parts_do_not_intersect_in_the_assembled_positions(self):
        pairs = itertools.combinations(self.parts.items(), 2)
        for (name_a, mesh_a), (name_b, mesh_b) in pairs:
            with self.subTest(parts=(name_a, name_b)):
                overlap = trimesh.boolean.intersection([mesh_a, mesh_b], engine="manifold", check_volume=True)
                volume = 0.0 if overlap is None else float(overlap.volume)
                self.assertLessEqual(volume, 1e-5, f"{name_a} intersects {name_b}: {volume} mm^3")

    def test_shell_bounds_and_split_match_the_configured_size(self):
        front = self.parts["bmo_front_shell"]
        back = self.parts["bmo_back_shell"]
        np.testing.assert_allclose(front.bounds, [[-41, -62, 0], [41, 62, 13]], atol=1e-5)
        np.testing.assert_allclose(back.bounds, [[-41, -62, -13], [41, 62, 0]], atol=1e-5)
        self.assertAlmostEqual(front.bounds[1, 2] - back.bounds[0, 2], 26.0)

    def test_exports_are_repeatable_and_importable(self):
        with tempfile.TemporaryDirectory() as first_dir, tempfile.TemporaryDirectory() as second_dir:
            first = export_all(Path(first_dir))
            second = export_all(Path(second_dir))
            expected_names = {f"{part}.stl" for part in EXPECTED_PARTS} | {"bmo_enclosure_assembly.glb"}
            self.assertEqual({path.name for path in first.values()}, expected_names)
            self.assertEqual({path.name for path in second.values()}, expected_names)
            for filename in sorted(expected_names):
                with self.subTest(file=filename):
                    path_a = Path(first_dir) / filename
                    path_b = Path(second_dir) / filename
                    if filename.endswith(".stl"):
                        mesh = trimesh.load_mesh(path_a, file_type="stl", process=True)
                        mesh_b = trimesh.load_mesh(path_b, file_type="stl", process=True)
                        validate_mesh(mesh, filename)
                        validate_mesh(mesh_b, filename)
                        self.assertEqual(len(mesh.faces), len(mesh_b.faces))
                        np.testing.assert_allclose(mesh.bounds, mesh_b.bounds, atol=1e-6)
                        self.assertAlmostEqual(mesh.volume, mesh_b.volume, delta=max(1e-5, mesh.volume * 1e-7))
            assembly = trimesh.load(Path(first_dir) / "bmo_enclosure_assembly.glb", force="scene")
            assembly_b = trimesh.load(Path(second_dir) / "bmo_enclosure_assembly.glb", force="scene")
            self.assertEqual(len(assembly.geometry), 8)  # seven printable meshes plus the glass preview
            self.assertEqual(set(assembly.geometry), set(assembly_b.geometry))
            for name in assembly.geometry:
                np.testing.assert_allclose(assembly.geometry[name].bounds, assembly_b.geometry[name].bounds, atol=1e-6)
                self.assertAlmostEqual(assembly.geometry[name].volume, assembly_b.geometry[name].volume, delta=1e-9)
            shell = assembly.geometry["bmo_front_shell"]
            np.testing.assert_allclose(shell.bounds, [[-0.041, -0.062, 0], [0.041, 0.062, 0.013]], atol=1e-5)
            glass = assembly.geometry["bmo_display_lens_preview"]
            overlap = trimesh.boolean.intersection([shell, glass], engine="manifold", check_volume=True)
            overlap_volume = 0.0 if overlap is None else float(overlap.volume)
            self.assertLessEqual(overlap_volume, 1e-12, "display glass intersects the front-shell rebate")


if __name__ == "__main__":
    unittest.main()
