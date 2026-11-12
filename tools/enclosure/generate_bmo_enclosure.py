"""Generate the parametric BMO handheld enclosure in millimetres.

The dimensions in CONFIG are design envelopes. Physical fit remains WAITING for
the caliper survey recorded in docs/hardware_measurements.md.
"""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable

import numpy as np
import trimesh


# All dimensions in this block are millimetres. Update measured hardware here.
CONFIG = {
    "physical_fit_status": "WAITING",
    "outer": {"width": 82.0, "height": 124.0, "thickness": 26.0, "corner_radius": 9.0},
    "shell": {"wall": 2.1, "split_z": 0.0, "front_depth": 13.0, "rear_depth": 13.0},
    "display": {
        "profile": "2.4_in",
        "active_window_profiles": {"2.0_in": (42.0, 31.5), "2.4_in": (50.0, 37.5), "2.8_in": (58.0, 43.5)},
        "center_y": 29.0,
        "lens_border_xy": 1.0,
        "lens_recess_clearance_xy": 0.18,
        "lens_recess_depth": 1.0,
        "lens_corner_radius": 2.0,
        "lens_thickness": 1.0,
    },
    "control": {
        "dpad_center": (-19.0, -22.0),
        "dpad_span": 28.0,
        "dpad_arm": 9.5,
        "action_centers": {"X": (18.0, -12.0), "Y": (9.0, -22.0), "A": (27.0, -22.0), "B": (18.0, -32.0)},
        "action_cap_diameter": 9.0,
        "action_hole_clearance_radial": 0.25,
        "pill_centers": {"SELECT": (-25.5, -43.5), "START": (-11.5, -43.5)},
        "pill_size": (12.0, 4.2),
        "shoulder_centers_x": (-27.0, 27.0),
        "shoulder_button_width": 18.0,
    },
    "ports": {
        "usb_c_width": 9.0,
        "usb_c_height": 3.5,
        "microsd_length": 14.0,
        "microsd_height": 2.2,
        "microsd_center_y": 20.0,
    },
    "battery": {
        "chemistry_plan": "3 x AA NiMH plus 5 V boost converter (not installed)",
        "door_center_y": -30.0,
        "door_opening": (58.0, 49.0),
        "cell_bay_envelope": (54.0, 47.0, 16.0),
        "carrier_rail_width": 2.0,
        "carrier_rail_height": 2.0,
        "carrier_rail_x": 30.0,
        "door_xy_clearance": 0.2,
        "door_tab_size": (8.0, 8.0),
        "door_screw_x": (-22.0, 22.0),
        "door_screw_y_offset": 27.5,
        "door_screw_clearance_diameter": 2.2,
        "door_post_diameter": 5.0,
        "door_post_height": 7.0,
        "door_screw_pilot_diameter": 1.6,
    },
    "perfboard": {
        "survey_status": "WAITING",
        "design_envelope": (50.0, 58.0, 8.0),
        "mount_hole_spacing": (44.0, 48.0),
        "mount_center_y": 22.0,
        "standoff_height": 7.0,
        "standoff_outer_diameter": 6.0,
        "screw_clearance_diameter": 2.2,
    },
    "fasteners": {
        "case_screw_clearance_diameter": 2.2,
        "case_screw_pilot_diameter": 1.6,
        "case_screw_edge_inset": 8.0,
        "boss_diameter": 6.2,
        "boss_height": 7.0,
    },
    "features": {
        "speaker_hole_diameter": 2.2,
        "speaker_hex_radius": 4.2,
        "speaker_center_y": 43.0,
        "side_logo_deboss_depth": 0.5,
        "rear_vent_count": 5,
        "rear_vent_width": 2.0,
        "rear_vent_length": 15.0,
    },
}

BODY_W = CONFIG["outer"]["width"]
BODY_H = CONFIG["outer"]["height"]
BODY_D = CONFIG["outer"]["thickness"]
HALF_W = BODY_W / 2
HALF_H = BODY_H / 2
WALL = CONFIG["shell"]["wall"]
CORNER = CONFIG["outer"]["corner_radius"]
FRONT_FACE_Z = BODY_D / 2
REAR_FACE_Z = -BODY_D / 2


def _box(extents: tuple[float, float, float], center: tuple[float, float, float]) -> trimesh.Trimesh:
    mesh = trimesh.creation.box(extents=extents)
    mesh.apply_translation(center)
    return mesh


def _cylinder(radius: float, height: float, center: tuple[float, float, float], axis: str = "z", sections: int = 48) -> trimesh.Trimesh:
    mesh = trimesh.creation.cylinder(radius=radius, height=height, sections=sections)
    if axis == "x":
        mesh.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [0, 1, 0]))
    elif axis == "y":
        mesh.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [1, 0, 0]))
    mesh.apply_translation(center)
    return mesh


def _rounded_prism(width: float, height: float, depth: float, radius: float, center: tuple[float, float, float]) -> trimesh.Trimesh:
    """Create a rounded rectangle extruded along Z from manifold primitives."""
    radius = min(radius, width / 2, height / 2)
    if radius <= 0:
        return _box((width, height, depth), center)
    x, y, z = center
    pieces: list[trimesh.Trimesh] = []
    if width - 2 * radius > 1e-6:
        pieces.append(_box((width - 2 * radius, height, depth), center))
    if height - 2 * radius > 1e-6:
        pieces.append(_box((width, height - 2 * radius, depth), center))
    for sx in (-1, 1):
        for sy in (-1, 1):
            pieces.append(_cylinder(radius, depth, (x + sx * (width / 2 - radius), y + sy * (height / 2 - radius), z), sections=64))
    return _union(pieces)


def _rounded_prism_xz(width: float, height_y: float, depth_z: float, radius: float,
                      center: tuple[float, float, float]) -> trimesh.Trimesh:
    """Create a rounded rectangle in X/Z and extrude it along Y."""
    mesh = _rounded_prism(width, depth_z, height_y, radius, (0, 0, 0))
    mesh.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [1, 0, 0]))
    mesh.apply_translation(center)
    return mesh


def _union(meshes: Iterable[trimesh.Trimesh]) -> trimesh.Trimesh:
    solids = [mesh for mesh in meshes if mesh is not None and len(mesh.faces)]
    if len(solids) == 1:
        return solids[0]
    result = trimesh.boolean.union(solids, engine="manifold", check_volume=True)
    if result is None or not len(result.faces):
        raise RuntimeError("manifold union returned an empty mesh")
    result.remove_unreferenced_vertices()
    return result


def _difference(mesh: trimesh.Trimesh, cutters: Iterable[trimesh.Trimesh]) -> trimesh.Trimesh:
    result = trimesh.boolean.difference([mesh, *cutters], engine="manifold", check_volume=True)
    if result is None or not len(result.faces):
        raise RuntimeError("manifold difference returned an empty mesh")
    result.remove_unreferenced_vertices()
    return result


def _centered_shell(half: str) -> trimesh.Trimesh:
    if half == "front":
        outer = _rounded_prism(BODY_W, BODY_H, BODY_D / 2, CORNER, (0, 0, BODY_D / 4))
        cavity_center_z = (BODY_D / 2 - WALL) / 2
        cavity = _rounded_prism(BODY_W - 2 * WALL, BODY_H - 2 * WALL, BODY_D / 2 - WALL + 0.2,
                                CORNER - WALL, (0, 0, cavity_center_z))
    else:
        outer = _rounded_prism(BODY_W, BODY_H, BODY_D / 2, CORNER, (0, 0, -BODY_D / 4))
        cavity_center_z = -(BODY_D / 2 - WALL) / 2
        cavity = _rounded_prism(BODY_W - 2 * WALL, BODY_H - 2 * WALL, BODY_D / 2 - WALL + 0.2,
                                CORNER - WALL, (0, 0, cavity_center_z))
    return _difference(outer, [cavity])


def _glyph_strokes(letter: str, y_center: float, side: int) -> list[trimesh.Trimesh]:
    """Block lettering cut 0.5 mm into both side walls."""
    depth = CONFIG["features"]["side_logo_deboss_depth"] + 0.12
    wall_x = side * HALF_W
    cutter_x = wall_x - side * (depth / 2 - 0.03)
    stroke = 2.0
    letter_h = 12.0
    letter_w = 8.0
    out: list[trimesh.Trimesh] = []

    def bar(y: float, z: float, h: float, w: float, angle: float = 0.0) -> None:
        mesh = _box((depth, h, w), (cutter_x, y_center + y, z))
        if angle:
            matrix = trimesh.transformations.rotation_matrix(np.radians(angle), [1, 0, 0], point=(cutter_x, y_center, 0))
            mesh.apply_transform(matrix)
        out.append(mesh)

    if letter == "B":
        for yy in (-letter_h / 2 + stroke / 2, 0, letter_h / 2 - stroke / 2):
            bar(yy, 0, stroke, letter_w)
        bar(0, -letter_w / 2 + stroke / 2, letter_h, stroke)
        bar(letter_h / 4, letter_w / 2 - stroke / 2, letter_h / 2, stroke)
        bar(-letter_h / 4, letter_w / 2 - stroke / 2, letter_h / 2, stroke)
    elif letter == "M":
        bar(0, -letter_w / 2 + stroke / 2, letter_h, stroke)
        bar(0, letter_w / 2 - stroke / 2, letter_h, stroke)
        bar(0, 0, letter_h, stroke, 24)
        bar(0, 0, letter_h, stroke, -24)
    elif letter == "O":
        for yy in (-letter_h / 2 + stroke / 2, letter_h / 2 - stroke / 2):
            bar(yy, 0, stroke, letter_w)
        for zz in (-letter_w / 2 + stroke / 2, letter_w / 2 - stroke / 2):
            bar(0, zz, letter_h, stroke)
    return out


def _side_features_cuts() -> list[trimesh.Trimesh]:
    cutters: list[trimesh.Trimesh] = []
    radius = CONFIG["features"]["speaker_hex_radius"]
    speaker_y = CONFIG["features"]["speaker_center_y"]
    hole_r = CONFIG["features"]["speaker_hole_diameter"] / 2
    for side in (-1, 1):
        outer_x = side * (HALF_W - 0.05)
        for i in range(7):
            angle = (2 * np.pi * i / 6) if i < 6 else 0
            if i == 6:
                y, z = speaker_y, 0.0
            else:
                y = speaker_y + radius * np.sin(angle)
                z = radius * np.cos(angle)
            cutters.append(_cylinder(hole_r, 4.0, (outer_x, y, z), axis="x", sections=32))
        for letter, y in (("B", 22.0), ("M", 6.0), ("O", -10.0)):
            cutters.extend(_glyph_strokes(letter, y, side))
        # Shallow circular arm socket centered inside the O counter.
        cutters.append(_cylinder(3.0, 1.1, (side * (HALF_W - 0.48), -10.0, 0.0), axis="x", sections=48))
    return cutters


def _button_bore(center: tuple[float, float], cap_radius: float, shell: str) -> list[trimesh.Trimesh]:
    x, y = center
    face_z = FRONT_FACE_Z if shell == "front" else REAR_FACE_Z
    # A stepped guide keeps the cap shoulder captive while leaving 0.25 mm radial travel.
    counterbore = _cylinder(cap_radius + 0.25, 1.45, (x, y, face_z - 0.55), sections=48)
    shaft = _cylinder(2.6, 2.2, (x, y, face_z - 2.35), sections=32)
    key = _box((1.1, 1.5, 2.2), (x + 2.5, y, face_z - 2.35))
    return [counterbore, shaft, key]


def _front_shell() -> trimesh.Trimesh:
    shell = _centered_shell("front")
    cutters: list[trimesh.Trimesh] = []
    disp_w, disp_h = CONFIG["display"]["active_window_profiles"][CONFIG["display"]["profile"]]
    disp_y = CONFIG["display"]["center_y"]
    # The active window passes through the face; a wider shallow rebate retains flush glass.
    cutters.append(_rounded_prism(disp_w, disp_h, 4.0, 1.2, (0, disp_y, FRONT_FACE_Z - 1.0)))
    lens_border = CONFIG["display"]["lens_border_xy"]
    recess_clearance = CONFIG["display"]["lens_recess_clearance_xy"]
    recess_w = disp_w + 2 * (lens_border + recess_clearance)
    recess_h = disp_h + 2 * (lens_border + recess_clearance)
    recess_depth = CONFIG["display"]["lens_recess_depth"]
    recess_radius = CONFIG["display"]["lens_corner_radius"] + recess_clearance
    cutters.append(_rounded_prism(recess_w, recess_h, recess_depth, recess_radius,
                                  (0, disp_y, FRONT_FACE_Z - recess_depth / 2)))
    dpad_x, dpad_y = CONFIG["control"]["dpad_center"]
    span = CONFIG["control"]["dpad_span"] + 0.5
    arm = CONFIG["control"]["dpad_arm"] + 0.5
    cutters.extend([
        _rounded_prism(arm, span, 4.0, 3.0, (dpad_x, dpad_y, FRONT_FACE_Z - 1.0)),
        _rounded_prism(span, arm, 4.0, 3.0, (dpad_x, dpad_y, FRONT_FACE_Z - 1.0)),
    ])
    for center in CONFIG["control"]["action_centers"].values():
        cutters.extend(_button_bore(center, CONFIG["control"]["action_cap_diameter"] / 2, "front"))
    pill_w, pill_h = CONFIG["control"]["pill_size"]
    for center in CONFIG["control"]["pill_centers"].values():
        cutters.append(_rounded_prism(pill_w + 0.5, pill_h + 0.5, 1.5, (pill_h + 0.5) / 2,
                                      (center[0], center[1], FRONT_FACE_Z - 0.55)))
        cutters.append(_rounded_prism(5.5, 2.0, 2.2, 0.7,
                                      (center[0], center[1], FRONT_FACE_Z - 2.1)))
    # Two top-edge actuator windows; the CAD dimensions are calibration parameters.
    for sx in CONFIG["control"]["shoulder_centers_x"]:
        cutters.append(_rounded_prism_xz(CONFIG["control"]["shoulder_button_width"] - 1.0,
                                         1.5, 6.0, 1.5, (sx, 61.25, 7.5)))
        cutters.append(_rounded_prism_xz(8.5, 2.2, 3.5, 1.0, (sx + 0.4, 60.9, 7.5)))
    # Top USB-C and left microSD openings cross the shell split and are cut in both halves.
    cutters.append(_box((CONFIG["ports"]["usb_c_width"], 8.0, CONFIG["ports"]["usb_c_height"]), (0, HALF_H - 1.0, 0)))
    cutters.append(_box((8.0, CONFIG["ports"]["microsd_length"], CONFIG["ports"]["microsd_height"]),
                        (-HALF_W + 1.0, CONFIG["ports"]["microsd_center_y"], 0)))
    cutters.extend(_side_features_cuts())
    # Case screw receivers align with the rear-shell clearance passages.
    case_x = HALF_W - CONFIG["fasteners"]["case_screw_edge_inset"]
    case_y = HALF_H - CONFIG["fasteners"]["case_screw_edge_inset"]
    receiver_height = CONFIG["fasteners"]["boss_height"]
    receiver_diameter = CONFIG["fasteners"]["boss_diameter"]
    pilot_diameter = CONFIG["fasteners"]["case_screw_pilot_diameter"]
    for sx in (-case_x, case_x):
        for sy in (-case_y, case_y):
            receiver = _cylinder(receiver_diameter / 2, receiver_height,
                                 (sx, sy, FRONT_FACE_Z - WALL - receiver_height / 2), sections=48)
            shell = _union([shell, receiver])
            cutters.append(_cylinder(pilot_diameter / 2, receiver_height + 0.2,
                                     (sx, sy, FRONT_FACE_Z - WALL - receiver_height / 2), sections=32))
    spacing_x, spacing_y = CONFIG["perfboard"]["mount_hole_spacing"]
    board_y = CONFIG["perfboard"]["mount_center_y"]
    for sx in (-spacing_x / 2, spacing_x / 2):
        for sy in (board_y - spacing_y / 2, board_y + spacing_y / 2):
            boss = _cylinder(CONFIG["perfboard"]["standoff_outer_diameter"] / 2,
                             CONFIG["perfboard"]["standoff_height"],
                             (sx, sy, FRONT_FACE_Z - WALL - CONFIG["perfboard"]["standoff_height"] / 2), sections=48)
            shell = _union([shell, boss])
            cutters.append(_cylinder(CONFIG["perfboard"]["screw_clearance_diameter"] / 2,
                                     CONFIG["perfboard"]["standoff_height"] + 0.4,
                                     (sx, sy, FRONT_FACE_Z - WALL - CONFIG["perfboard"]["standoff_height"] / 2), sections=32))
    return _difference(shell, cutters)


def _battery_door_cutters() -> list[trimesh.Trimesh]:
    width, height = CONFIG["battery"]["door_opening"]
    y = CONFIG["battery"]["door_center_y"]
    return [_rounded_prism(width, height, 4.0, 3.0, (0, y, REAR_FACE_Z + 1.0))]


def _battery_door_mounts() -> tuple[list[trimesh.Trimesh], list[trimesh.Trimesh]]:
    """Return the shell pockets and internal posts for the door screw tabs."""
    config = CONFIG["battery"]
    tab_w, tab_h = config["door_tab_size"]
    y_centers = (config["door_center_y"] - config["door_screw_y_offset"],
                 config["door_center_y"] + config["door_screw_y_offset"])
    recesses: list[trimesh.Trimesh] = []
    posts: list[trimesh.Trimesh] = []
    for x in config["door_screw_x"]:
        for y in y_centers:
            recesses.append(_rounded_prism(tab_w + 0.2, tab_h + 0.2, WALL + 0.3, 1.0,
                                           (x, y, REAR_FACE_Z + (WALL + 0.3) / 2)))
            posts.append(_cylinder(config["door_post_diameter"] / 2, config["door_post_height"],
                                   (x, y, REAR_FACE_Z + WALL + config["door_post_height"] / 2), sections=48))
    return recesses, posts


def _back_shell() -> trimesh.Trimesh:
    shell = _centered_shell("back")
    cutters = _battery_door_cutters()
    door_recesses, door_posts = _battery_door_mounts()
    cutters.extend(door_recesses)
    cutters.extend(_side_features_cuts())
    # Five rear intake/exhaust slots; actual airflow depends on fitted components.
    count = CONFIG["features"]["rear_vent_count"]
    pitch = 7.5
    vent_y = 48.0
    for i in range(count):
        x = (i - (count - 1) / 2) * pitch
        cutters.append(_rounded_prism(CONFIG["features"]["rear_vent_width"],
                                      CONFIG["features"]["rear_vent_length"], 4.0,
                                      CONFIG["features"]["rear_vent_width"] / 2,
                                      (x, vent_y, REAR_FACE_Z + 1.0)))
    # Port access is cut through the top edge and left side wall.
    cutters.append(_box((CONFIG["ports"]["usb_c_width"], 8.0, CONFIG["ports"]["usb_c_height"]),
                        (0, HALF_H - 1.0, 0)))
    cutters.append(_box((8.0, CONFIG["ports"]["microsd_length"], CONFIG["ports"]["microsd_height"]),
                        (-HALF_W + 1.0, CONFIG["ports"]["microsd_center_y"], 0)))
    # Case screw bosses pass through the rear wall and align with front receivers.
    screw_x = HALF_W - CONFIG["fasteners"]["case_screw_edge_inset"]
    screw_y = HALF_H - CONFIG["fasteners"]["case_screw_edge_inset"]
    bosses = []
    for sx in (-screw_x, screw_x):
        for sy in (-screw_y, screw_y):
            bosses.append(_cylinder(CONFIG["fasteners"]["boss_diameter"] / 2,
                                    CONFIG["fasteners"]["boss_height"],
                                    (sx, sy, REAR_FACE_Z + WALL + CONFIG["fasteners"]["boss_height"] / 2), sections=48))
            passage_height = WALL + CONFIG["fasteners"]["boss_height"] + 0.5
            cutters.append(_cylinder(CONFIG["fasteners"]["case_screw_clearance_diameter"] / 2,
                                     passage_height,
                                     (sx, sy, REAR_FACE_Z + (CONFIG["fasteners"]["boss_height"] + WALL) / 2),
                                     sections=32))
    door_pilot_height = CONFIG["battery"]["door_post_height"] + 0.2
    door_y_centers = (CONFIG["battery"]["door_center_y"] - CONFIG["battery"]["door_screw_y_offset"],
                      CONFIG["battery"]["door_center_y"] + CONFIG["battery"]["door_screw_y_offset"])
    for sx in CONFIG["battery"]["door_screw_x"]:
        for sy in door_y_centers:
            cutters.append(_cylinder(CONFIG["battery"]["door_screw_pilot_diameter"] / 2,
                                     door_pilot_height,
                                     (sx, sy, REAR_FACE_Z + WALL + CONFIG["battery"]["door_post_height"] / 2),
                                     sections=32))
    # Raised board mounts are design placeholders until the board survey is complete.
    spacing_x, spacing_y = CONFIG["perfboard"]["mount_hole_spacing"]
    board_y = CONFIG["perfboard"]["mount_center_y"]
    standoffs = []
    for sx in (-spacing_x / 2, spacing_x / 2):
        for sy in (board_y - spacing_y / 2, board_y + spacing_y / 2):
            post = _cylinder(CONFIG["perfboard"]["standoff_outer_diameter"] / 2,
                             CONFIG["perfboard"]["standoff_height"],
                             (sx, sy, -BODY_D / 2 + WALL + CONFIG["perfboard"]["standoff_height"] / 2), sections=48)
            standoffs.append(post)
            cutters.append(_cylinder(CONFIG["perfboard"]["screw_clearance_diameter"] / 2,
                                     CONFIG["perfboard"]["standoff_height"] + 0.4,
                                     (sx, sy, -BODY_D / 2 + WALL + CONFIG["perfboard"]["standoff_height"] / 2), sections=32))
    battery_rails = []
    rail_width = CONFIG["battery"]["carrier_rail_width"]
    rail_height = CONFIG["battery"]["carrier_rail_height"]
    _, cell_height, _ = CONFIG["battery"]["cell_bay_envelope"]
    for sx in (-CONFIG["battery"]["carrier_rail_x"], CONFIG["battery"]["carrier_rail_x"]):
        battery_rails.append(_box((rail_width, cell_height, rail_height),
                                  (sx, CONFIG["battery"]["door_center_y"], REAR_FACE_Z + WALL + rail_height / 2)))
    shell = _union([shell, *bosses, *standoffs, *battery_rails, *door_posts])
    return _difference(shell, cutters)


def _button_cap(radius: float = 4.5, top_z: float = 0.0, keyed: bool = True) -> trimesh.Trimesh:
    # Crown, keyed stem, and a retaining flange wider than the 9.5 mm opening.
    cap = _cylinder(radius, 1.45, (0, 0, top_z - 0.725), sections=64)
    stem = _cylinder(2.2, 1.6, (0, 0, top_z - 2.275), sections=48)
    if keyed:
        stem = _union([stem, _box((1.0, 1.2, 1.6), (2.5, 0, top_z - 2.275))])
    flange = _cylinder(5.15, 0.8, (0, 0, top_z - 2.7), sections=64)
    result = _union([cap, stem, flange])
    return result


def _pill_cap(center: tuple[float, float]) -> trimesh.Trimesh:
    width, height = CONFIG["control"]["pill_size"]
    top = _rounded_prism(width - 0.5, height - 0.5, 1.6, (height - 0.5) / 2, (0, 0, -0.2))
    stem = _rounded_prism(5.0, 2.0, 1.6, 0.7, (0, 0, -1.8))
    flange = _rounded_prism(width - 1.0, height - 1.0, 0.8, (height - 1.0) / 2, (0, 0, -2.8))
    part = _union([top, stem, flange])
    part.apply_translation((center[0], center[1], FRONT_FACE_Z + 0.2))
    return part


def _dpad_part() -> trimesh.Trimesh:
    cx, cy = CONFIG["control"]["dpad_center"]
    span = CONFIG["control"]["dpad_span"] - 0.6
    arm = CONFIG["control"]["dpad_arm"] - 0.6
    top_z = FRONT_FACE_Z + 0.2
    top = _union([
        _rounded_prism(arm, span, 2.0, 2.8, (cx, cy, top_z - 1.0)),
        _rounded_prism(span, arm, 2.0, 2.8, (cx, cy, top_z - 1.0)),
    ])
    neck = _union([
        _rounded_prism(8.0, span - 0.5, 0.5, 2.5, (cx, cy, top_z - 2.15)),
        _rounded_prism(span - 0.5, 8.0, 0.5, 2.5, (cx, cy, top_z - 2.15)),
    ])
    retainer = _rounded_prism(span - 1.2, span - 1.2, 1.0, 4.0, (cx, cy, top_z - 2.8))
    pivot = _cylinder(3.0, 2.0, (cx, cy, top_z - 4.0), sections=48)
    return _union([top, neck, retainer, pivot])


def _lr_cap(x: float) -> trimesh.Trimesh:
    # The cap presses down through the top wall; its front face carries the L/R legend.
    cap = _rounded_prism_xz(16.0, 5.0, 6.0, 1.8, (x, 63.95, 7.5))
    stem = _rounded_prism_xz(8.0, 1.6, 3.2, 0.7, (x + 0.4, 60.65, 7.5))
    side = 1.0 if x > 0 else -1.0
    flange = _rounded_prism_xz(18.0, 0.8, 6.6, 2.0, (x - side * 4.5, 59.45, 7.5))
    return _union([cap, stem, flange])


def _battery_door() -> trimesh.Trimesh:
    opening_w, opening_h = CONFIG["battery"]["door_opening"]
    clearance = CONFIG["battery"]["door_xy_clearance"]
    width, height = opening_w - 2 * clearance, opening_h - 2 * clearance
    y = CONFIG["battery"]["door_center_y"]
    parts = [_rounded_prism(width, height, 2.0, 2.7, (0, y, REAR_FACE_Z + 1.0))]
    tab_w, tab_h = CONFIG["battery"]["door_tab_size"]
    y_centers = (y - CONFIG["battery"]["door_screw_y_offset"],
                 y + CONFIG["battery"]["door_screw_y_offset"])
    holes = []
    for sx in CONFIG["battery"]["door_screw_x"]:
        for sy in y_centers:
            parts.append(_rounded_prism(tab_w, tab_h, 2.0, 1.0, (sx, sy, REAR_FACE_Z + 1.0)))
            holes.append(_cylinder(CONFIG["battery"]["door_screw_clearance_diameter"] / 2,
                                   2.4, (sx, sy, REAR_FACE_Z + 1.0), sections=32))
    door = _union(parts)
    return _difference(door, holes)


def build_parts() -> dict[str, trimesh.Trimesh]:
    """Construct all printable bodies in the assembled console coordinates."""
    front = _front_shell()
    back = _back_shell()
    xyab: list[trimesh.Trimesh] = []
    for center in CONFIG["control"]["action_centers"].values():
        cap = _button_cap()
        cap.apply_translation((center[0], center[1], FRONT_FACE_Z + 0.2))
        xyab.append(cap)
    pills = [_pill_cap(center) for center in CONFIG["control"]["pill_centers"].values()]
    shoulders = [_lr_cap(x) for x in CONFIG["control"]["shoulder_centers_x"]]
    parts = {
        "bmo_front_shell": front,
        "bmo_back_shell": back,
        "bmo_battery_door": _battery_door(),
        "bmo_dpad": _dpad_part(),
        "bmo_buttons_xyab": trimesh.util.concatenate(xyab),
        "bmo_buttons_start_select": trimesh.util.concatenate(pills),
        "bmo_buttons_lr": trimesh.util.concatenate(shoulders),
    }
    return parts


def _set_preview_colors(mesh: trimesh.Trimesh, part_name: str) -> None:
    if part_name == "bmo_buttons_xyab":
        centers = np.asarray(list(CONFIG["control"]["action_centers"].values()), dtype=np.float64)
        colors = np.asarray([[76, 175, 80, 255], [244, 143, 177, 255], [82, 166, 186, 255], [255, 145, 165, 255]], dtype=np.uint8)
        nearest = np.square(mesh.vertices[:, None, :2] - centers[None, :, :]).sum(axis=2).argmin(axis=1)
        mesh.visual.vertex_colors = colors[nearest]
        return
    color = {
        "bmo_front_shell": [99, 197, 184, 255],
        "bmo_back_shell": [90, 181, 169, 255],
        "bmo_battery_door": [86, 174, 163, 255],
        "bmo_dpad": [244, 208, 63, 255],
        "bmo_buttons_xyab": [244, 143, 177, 255],
        "bmo_buttons_start_select": [38, 75, 66, 255],
        "bmo_buttons_lr": [38, 75, 66, 255],
    }[part_name]
    mesh.visual.vertex_colors = np.tile(np.asarray(color, dtype=np.uint8), (len(mesh.vertices), 1))


def validate_mesh(mesh: trimesh.Trimesh, name: str) -> None:
    """Reject empty, open, inconsistently wound, or non-positive-volume meshes."""
    if not len(mesh.faces) or not np.isfinite(mesh.vertices).all():
        raise ValueError(f"{name}: mesh is empty or contains non-finite vertices")
    if not mesh.is_watertight:
        raise ValueError(f"{name}: mesh is not watertight")
    if not mesh.is_winding_consistent:
        raise ValueError(f"{name}: mesh winding is inconsistent")
    if not mesh.is_volume or mesh.volume <= 0:
        raise ValueError(f"{name}: mesh must enclose positive volume")
    # Every part is produced by the manifold CSG kernel; validate that the
    # exported triangles can be imported back into the same manifold kernel.
    import manifold3d

    kernel_mesh = manifold3d.Mesh(vert_properties=mesh.vertices.astype(np.float32), tri_verts=mesh.faces.astype(np.uint32))
    solid = manifold3d.Manifold(kernel_mesh)
    if solid.status() != manifold3d.Error.NoError:
        raise ValueError(f"{name}: manifold validation failed: {solid.status()}")


def export_all(output_dir: Path) -> dict[str, Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    parts = build_parts()
    scene = trimesh.Scene()
    exported: dict[str, Path] = {}
    for name, mesh in parts.items():
        validate_mesh(mesh, name)
        _set_preview_colors(mesh, name)
        stl_path = output_dir / f"{name}.stl"
        mesh.export(stl_path, file_type="stl")
        exported[name] = stl_path
        preview_mesh = mesh.copy()
        preview_mesh.apply_scale(0.001)  # glTF/GLB uses metres; STL files are millimetres by convention.
        scene.add_geometry(preview_mesh, geom_name=name, node_name=name)
    display_width, display_height = CONFIG["display"]["active_window_profiles"][CONFIG["display"]["profile"]]
    lens_border = CONFIG["display"]["lens_border_xy"]
    lens = _rounded_prism(display_width + 2 * lens_border, display_height + 2 * lens_border,
                          CONFIG["display"]["lens_thickness"], CONFIG["display"]["lens_corner_radius"],
                          (0, CONFIG["display"]["center_y"], FRONT_FACE_Z - CONFIG["display"]["lens_thickness"] / 2))
    lens.visual.vertex_colors = np.tile(np.asarray([7, 8, 10, 255], dtype=np.uint8), (len(lens.vertices), 1))
    lens.apply_scale(0.001)
    scene.add_geometry(lens, geom_name="bmo_display_lens_preview", node_name="bmo_display_lens_preview")
    glb_path = output_dir / "bmo_enclosure_assembly.glb"
    scene.export(glb_path, file_type="glb")
    exported["bmo_enclosure_assembly"] = glb_path
    return exported


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parent / "build")
    args = parser.parse_args()
    paths = export_all(args.output)
    print(f"Physical fit status: {CONFIG['physical_fit_status']}")
    for name, path in paths.items():
        print(f"{path.name}: {path.stat().st_size} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
