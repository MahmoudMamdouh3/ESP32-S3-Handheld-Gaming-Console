"""Export manifold CAD enclosure shells to deterministic Three.js mesh data.

Generates `simulator/bmo_enclosure_meshes.js` directly from
`hardware/enclosure/generate_bmo_enclosure.py` so the browser 3D view renders
exact through-punched speaker bores, split shells, port cutouts, and recessed
BMO side glyphs under both `file://` and local HTTP serving.
"""

from __future__ import annotations

import argparse
import base64
import json
import sys
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np
import trimesh

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from hardware.enclosure.generate_bmo_enclosure import CONFIG, build_parts  # noqa: E402


DEFAULT_OUT_PATH = REPO_ROOT / "simulator" / "bmo_enclosure_meshes.js"
CREASE_ANGLE_DEG = 38.0
CM_SCALE = 0.1  # CAD runs in millimetres; Three.js simulator uses centimetres.


def _classify_shell_face_materials(mesh: trimesh.Trimesh, part_name: str) -> np.ndarray:
    """Assign material group 1 to recessed side-wall BMO letter faces."""
    face_count = len(mesh.faces)
    mat_ids = np.zeros(face_count, dtype=np.uint8)
    if part_name not in ("bmo_front_shell", "bmo_back_shell"):
        return mat_ids

    verts_cm = mesh.vertices * CM_SCALE
    tri_verts = verts_cm[mesh.faces]  # (F, 3, 3)
    centroids = tri_verts.mean(axis=1)
    max_abs_x = np.abs(tri_verts[:, :, 0]).max(axis=1)

    half_w_cm = CONFIG["outer"]["width"] * 0.5 * CM_SCALE
    deboss_cm = CONFIG["features"]["side_logo_deboss_depth"] * CM_SCALE

    # Outer flat side wall sits at |x| = 4.100 cm; glyph pockets sit inward around 4.04..4.098 cm.
    abs_cx = np.abs(centroids[:, 0])
    in_pocket_x = (abs_cx >= (half_w_cm - deboss_cm - 0.035)) & (abs_cx <= (half_w_cm - 0.002))
    not_outer_wall = max_abs_x <= (half_w_cm + 0.002)
    in_glyph_y = (centroids[:, 1] >= -1.72) & (centroids[:, 1] <= 2.92)
    in_glyph_z = np.abs(centroids[:, 2]) <= 0.46

    # Cylindrical interior walls of the 7 through-punched side speaker bores sit strictly
    # inside the wall thickness (3.905..4.095 cm) around y = 4.30 cm.
    in_speaker_bore_x = (abs_cx >= (half_w_cm - 0.195)) & (abs_cx <= (half_w_cm - 0.003))
    in_speaker_y = (centroids[:, 1] >= 3.65) & (centroids[:, 1] <= 4.95)
    in_speaker_z = np.abs(centroids[:, 2]) <= 0.65

    mat_ids[in_pocket_x & not_outer_wall & in_glyph_y & in_glyph_z] = 1
    mat_ids[in_speaker_bore_x & not_outer_wall & in_speaker_y & in_speaker_z] = 1
    return mat_ids


def _build_crease_split_buffers(
    mesh: trimesh.Trimesh,
    mat_ids: np.ndarray,
    crease_angle_deg: float = CREASE_ANGLE_DEG,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray, List[List[int]]]:
    """Split vertices across sharp creases and sort triangles by material group."""
    verts_cm = np.asarray(mesh.vertices, dtype=np.float64) * CM_SCALE
    faces = np.asarray(mesh.faces, dtype=np.int64)
    face_normals = np.asarray(mesh.face_normals, dtype=np.float64)
    face_areas = np.maximum(np.asarray(mesh.area_faces, dtype=np.float64), 1e-12)
    cos_thresh = float(np.cos(np.radians(crease_angle_deg)))

    # Order triangles deterministically by material group, then original face index.
    ordered_face_indices = sorted(range(len(faces)), key=lambda idx: (int(mat_ids[idx]), idx))

    # Build deterministic vertex -> incident face list sorted by face index.
    vert_to_faces: List[List[int]] = [[] for _ in range(len(verts_cm))]
    for f_idx in range(len(faces)):
        i0, i1, i2 = faces[f_idx]
        vert_to_faces[i0].append(f_idx)
        vert_to_faces[i1].append(f_idx)
        vert_to_faces[i2].append(f_idx)

    # Partition each vertex's incident faces into smooth shading clusters.
    face_corner_to_new_vert: Dict[Tuple[int, int], int] = {}
    out_positions: List[Tuple[float, float, float]] = []
    out_normals: List[Tuple[float, float, float]] = []

    for v_idx, inc_faces in enumerate(vert_to_faces):
        if not inc_faces:
            continue
        inc_sorted = sorted(inc_faces)
        clusters: List[List[int]] = []
        visited = set()

        for seed_face in inc_sorted:
            if seed_face in visited:
                continue
            cluster = [seed_face]
            visited.add(seed_face)
            queue = [seed_face]
            seed_n = face_normals[seed_face]

            while queue:
                curr = queue.pop(0)
                curr_n = face_normals[curr]
                for cand in inc_sorted:
                    if cand in visited:
                        continue
                    cand_n = face_normals[cand]
                    # Prevent cascading across > 55 deg total bend at saddle corners.
                    if float(np.dot(curr_n, cand_n)) >= cos_thresh and float(np.dot(seed_n, cand_n)) >= 0.55:
                        visited.add(cand)
                        cluster.append(cand)
                        queue.append(cand)
            clusters.append(cluster)

        px, py, pz = verts_cm[v_idx]
        for cluster in clusters:
            accum_n = np.zeros(3, dtype=np.float64)
            for f_idx in cluster:
                accum_n += face_normals[f_idx] * face_areas[f_idx]
            norm_len = float(np.linalg.norm(accum_n))
            if norm_len > 1e-12:
                accum_n /= norm_len
            else:
                accum_n = face_normals[cluster[0]]

            new_v_idx = len(out_positions)
            out_positions.append((float(px), float(py), float(pz)))
            out_normals.append((float(accum_n[0]), float(accum_n[1]), float(accum_n[2])))
            for f_idx in cluster:
                face_corner_to_new_vert[(f_idx, v_idx)] = new_v_idx

    out_indices: List[int] = []
    groups: List[List[int]] = []
    current_mat = None
    group_start = 0

    for f_idx in ordered_face_indices:
        mat = int(mat_ids[f_idx])
        if current_mat is None:
            current_mat = mat
            group_start = 0
        elif mat != current_mat:
            groups.append([group_start, len(out_indices) - group_start, current_mat])
            current_mat = mat
            group_start = len(out_indices)

        i0, i1, i2 = faces[f_idx]
        out_indices.extend((
            face_corner_to_new_vert[(f_idx, int(i0))],
            face_corner_to_new_vert[(f_idx, int(i1))],
            face_corner_to_new_vert[(f_idx, int(i2))],
        ))

    if current_mat is not None and len(out_indices) > group_start:
        groups.append([group_start, len(out_indices) - group_start, current_mat])

    pos_arr = np.asarray(out_positions, dtype="<f4")
    norm_arr = np.clip(np.rint(np.asarray(out_normals, dtype=np.float64) * 32767.0), -32767, 32767).astype("<i2")
    idx_dtype = "<u2" if len(out_positions) <= 65535 else "<u4"
    idx_arr = np.asarray(out_indices, dtype=idx_dtype)
    return pos_arr, norm_arr, idx_arr, groups


def export_web_meshes(out_path: Path = DEFAULT_OUT_PATH) -> dict[str, dict[str, int]]:
    """Build enclosure CAD parts and write `window.BMO_ENCLOSURE_MESHES` JS bundle."""
    parts = build_parts()
    target_names = ("bmo_front_shell", "bmo_back_shell", "bmo_battery_door")
    payload: dict[str, object] = {
        "units": "cm",
        "creaseAngleDeg": CREASE_ANGLE_DEG,
        "dimensionsMm": {
            "outerWidth": CONFIG["outer"]["width"],
            "outerHeight": CONFIG["outer"]["height"],
            "outerThickness": CONFIG["outer"]["thickness"],
            "wall": CONFIG["shell"]["wall"],
            "usbCWidth": CONFIG["ports"]["usb_c_width"],
            "usbCHeight": CONFIG["ports"]["usb_c_height"],
            "microsdLength": CONFIG["ports"]["microsd_length"],
            "microsdCenterY": CONFIG["ports"]["microsd_center_y"],
            "speakerCenterY": CONFIG["features"]["speaker_center_y"],
            "speakerHexRadius": CONFIG["features"]["speaker_hex_radius"],
            "speakerHoleDiameter": CONFIG["features"]["speaker_hole_diameter"],
        },
        "parts": {},
    }
    stats: dict[str, dict[str, int]] = {}

    parts_dict: dict[str, object] = {}
    for part_name in target_names:
        mesh = parts[part_name]
        mat_ids = _classify_shell_face_materials(mesh, part_name)
        pos_arr, norm_arr, idx_arr, groups = _build_crease_split_buffers(mesh, mat_ids)
        index_type = "u16" if idx_arr.dtype == np.dtype("<u2") else "u32"

        parts_dict[part_name] = {
            "vertexCount": int(len(pos_arr)),
            "triangleCount": int(len(idx_arr) // 3),
            "indexType": index_type,
            "groups": groups,
            "posB64": base64.b64encode(pos_arr.tobytes()).decode("ascii"),
            "normI16B64": base64.b64encode(norm_arr.tobytes()).decode("ascii"),
            "idxB64": base64.b64encode(idx_arr.tobytes()).decode("ascii"),
        }
        stats[part_name] = {
            "vertices": int(len(pos_arr)),
            "triangles": int(len(idx_arr) // 3),
            "inkTriangles": int((mat_ids == 1).sum()),
        }

    payload["parts"] = parts_dict
    serialized = json.dumps(payload, separators=(",", ":"), sort_keys=False)

    js_lines = [
        "// Auto-generated from hardware/enclosure/generate_bmo_enclosure.py by",
        "// scripts/export_enclosure_web_meshes.py. Do not edit manually.",
        "(function (globalScope) {",
        "  'use strict';",
        f"  globalScope.BMO_ENCLOSURE_MESHES = {serialized};",
        "})(typeof window !== 'undefined' ? window : globalThis);",
        "",
    ]
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(js_lines), encoding="utf-8")
    return stats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT_PATH)
    args = parser.parse_args()
    stats = export_web_meshes(args.out)
    print(f"Wrote {args.out} ({args.out.stat().st_size} bytes)")
    for name, info in stats.items():
        print(
            f"  {name}: {info['vertices']} verts, {info['triangles']} tris, "
            f"{info['inkTriangles']} glyph-ink tris"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
