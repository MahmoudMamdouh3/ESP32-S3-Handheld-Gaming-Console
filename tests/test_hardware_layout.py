# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Verification tests for hardware/layout.json, 31x46 perfboard grid snapping, Z-stackup, and generated artifacts."""

from __future__ import annotations

import csv
import importlib.util
import json
import math
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
LAYOUT_JSON = REPO_ROOT / "hardware" / "layout.json"
GEN_SCRIPT = REPO_ROOT / "scripts" / "generate_hardware_layout_artifacts.py"


def _load_generator_module():
    spec = importlib.util.spec_from_file_location("generate_hardware_layout_artifacts", GEN_SCRIPT)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class HardwareLayoutTests(unittest.TestCase):
    def setUp(self) -> None:
        self.layout = json.loads(LAYOUT_JSON.read_text(encoding="utf-8"))

    def test_enclosure_and_perfboard_grid_dimensions_in_cm(self) -> None:
        self.assertEqual(self.layout["units"], "cm")
        self.assertEqual(self.layout["evidence_level"], "NOT_TESTED")

        env = self.layout["enclosure"]["owner_reference_cm"]
        self.assertAlmostEqual(env["width_cm"], 8.80, places=2)
        self.assertAlmostEqual(env["height_cm"], 12.40, places=2)
        self.assertAlmostEqual(env["thickness_cm"], 3.20, places=2)

        pb = self.layout["perfboard"]
        self.assertEqual(pb["columns"], 31)
        self.assertEqual(pb["rows"], 46)
        self.assertAlmostEqual(pb["pitch_cm"], 0.254, places=3)
        self.assertAlmostEqual(pb["width_cm"], 31 * 0.254, places=3)
        self.assertAlmostEqual(pb["height_cm"], 46 * 0.254, places=3)

    def test_button_grid_snapping_and_switch_clearance(self) -> None:
        pb = self.layout["perfboard"]
        c1x, r1y = pb["hole_c01_r01_cm"]
        pitch = pb["pitch_cm"]

        seen_holes: set[tuple[int, int]] = set()
        for btn in self.layout["controls"]["buttons"]:
            col = btn["perfboard_col"]
            row = btn["perfboard_row"]
            self.assertGreaterEqual(col, 1)
            self.assertLessEqual(col, pb["columns"])
            self.assertGreaterEqual(row, 1)
            self.assertLessEqual(row, pb["rows"])

            hole = (col, row)
            self.assertNotIn(hole, seen_holes, f"Two buttons share the same perfboard hole {hole}")
            seen_holes.add(hole)

            expected_x = round(c1x + (col - 1) * pitch, 3)
            expected_y = round(r1y + (row - 1) * pitch, 3)
            self.assertAlmostEqual(btn["snapped_x_cm"], expected_x, places=3)
            self.assertAlmostEqual(btn["snapped_y_cm"], expected_y, places=3)

            dist = math.hypot(btn["snapped_x_cm"] - btn["target_x_cm"], btn["snapped_y_cm"] - btn["target_y_cm"])
            self.assertAlmostEqual(btn["snap_offset_cm"], round(dist, 3), places=2)
            if btn["group"] != "shoulder":
                # Front-face buttons must snap within half a diagonal hole pitch (~0.18 cm)
                self.assertLessEqual(dist, 0.18)

        # Verify ABXY adjacent center spacing and switch gap math
        adj_cm = self.layout["controls"]["abxy_adjacent_center_distance_cm"]
        self.assertAlmostEqual(adj_cm, round(math.hypot(1.0, 1.0), 3), places=3)
        opts = self.layout["controls"]["switch_options"]
        self.assertAlmostEqual(opts["omron_12x12mm"]["abxy_edge_gap_cm"], round(adj_cm - 1.20, 3), places=3)
        self.assertAlmostEqual(opts["tactile_6x6mm"]["abxy_edge_gap_cm"], round(adj_cm - 0.60, 3), places=3)

    def test_z_stackup_sums_to_3_20_cm_with_zero_collisions(self) -> None:
        z_stack = self.layout["z_stackup"]
        layers = z_stack["layers"]
        self.assertAlmostEqual(layers[0]["z_start_cm"], 0.00, places=2)
        total = 0.0
        prev_end = 0.0
        for layer in layers:
            self.assertAlmostEqual(layer["z_start_cm"], prev_end, places=2)
            self.assertAlmostEqual(layer["z_end_cm"] - layer["z_start_cm"], layer["thickness_cm"], places=2)
            total += layer["thickness_cm"]
            prev_end = layer["z_end_cm"]
        self.assertAlmostEqual(total, z_stack["total_thickness_cm"], places=2)
        self.assertAlmostEqual(total, 3.20, places=2)

        # Verify Upper Y Zone and Lower Y Zone do not overlap in Y and each component fits inside 1.74 cm rear depth
        upper = z_stack["rear_zones"]["upper_zone"]
        lower = z_stack["rear_zones"]["lower_zone"]
        self.assertLessEqual(upper["y_range_cm"][1], lower["y_range_cm"][0])

        for zone in (upper, lower):
            avail = zone["available_depth_cm"]
            self.assertAlmostEqual(avail, 1.74, places=2)
            for comp in zone["components"]:
                self.assertLess(comp["depth_cm"], avail)
                self.assertAlmostEqual(comp["clearance_cm"], round(avail - comp["depth_cm"], 2), places=2)

    def test_generated_layout_artifacts_are_in_sync(self) -> None:
        gen = _load_generator_module()
        self.assertEqual(
            gen.TOP_SVG_PATH.read_text(encoding="utf-8"),
            gen.generate_top_view_svg(self.layout),
        )
        self.assertEqual(
            gen.SIDE_SVG_PATH.read_text(encoding="utf-8"),
            gen.generate_side_section_svg(self.layout),
        )
        self.assertEqual(
            gen.PERFBOARD_DOC_PATH.read_text(encoding="utf-8"),
            gen.generate_perfboard_markdown(self.layout),
        )
        with gen.WIRE_CSV_PATH.open("r", encoding="utf-8") as fh:
            rows = list(csv.DictReader(fh))
        self.assertEqual(rows, gen.generate_wire_list_rows(self.layout))


if __name__ == "__main__":
    unittest.main()
