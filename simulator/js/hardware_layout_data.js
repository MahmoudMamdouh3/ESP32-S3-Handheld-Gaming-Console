// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
// Auto-generated from hardware/layout.json by scripts/generate_hardware_layout_artifacts.py.
export const DEFAULT_HARDWARE_LAYOUT = {
  "schema_version": 1,
  "title": "BMO Handheld Physical Reference Layout & Stack-Up",
  "evidence_level": "NOT_TESTED",
  "units": "cm",
  "cad_units": "mm",
  "coordinate_system": {
    "origin": "Top-left corner of front enclosure face (0.00, 0.00, 0.00)",
    "x_axis": "+X points right across front face (0.00 to 8.80 cm)",
    "y_axis": "+Y points down along front face (0.00 to 12.40 cm)",
    "z_axis": "+Z points from front exterior face toward rear exterior face (0.00 to 3.20 cm)"
  },
  "enclosure": {
    "owner_reference_cm": {
      "width_cm": 8.8,
      "height_cm": 12.4,
      "thickness_cm": 3.2,
      "corner_radius_cm": 0.9,
      "front_wall_cm": 0.2,
      "back_wall_cm": 0.2,
      "side_wall_cm": 0.21
    },
    "legacy_cad_compact_cm": {
      "width_cm": 8.2,
      "height_cm": 12.4,
      "thickness_cm": 2.6,
      "perfboard_width_cm": 5.0,
      "perfboard_height_cm": 7.0,
      "battery_bay_cm": [
        5.4,
        4.7,
        1.6
      ],
      "note": "Legacy generate_bmo_enclosure.py starting shell; battery bay length (5.40 cm) is shorter than standard AA holder (5.80 cm). Owner Decision #3 selects between 8.80x12.40x3.20 cm reference layout and compact shell."
    }
  },
  "perfboard": {
    "pitch_cm": 0.254,
    "columns": 31,
    "rows": 46,
    "width_cm": 7.874,
    "height_cm": 11.684,
    "thickness_cm": 0.16,
    "board_top_left_cm": [
      0.463,
      0.358
    ],
    "hole_c01_r01_cm": [
      0.59,
      0.485
    ],
    "mounting_holes": [
      {
        "id": "MH_TL",
        "col": 2,
        "row": 2,
        "x_cm": 0.844,
        "y_cm": 0.739
      },
      {
        "id": "MH_TR",
        "col": 30,
        "row": 2,
        "x_cm": 7.956,
        "y_cm": 0.739
      },
      {
        "id": "MH_BL",
        "col": 2,
        "row": 45,
        "x_cm": 0.844,
        "y_cm": 11.661
      },
      {
        "id": "MH_BR",
        "col": 30,
        "row": 45,
        "x_cm": 7.956,
        "y_cm": 11.661
      }
    ]
  },
  "display": {
    "diagonal_inches": 2.4,
    "diagonal_cm": 6.096,
    "controller": "ST7789VW",
    "resolution_px": [
      320,
      240
    ],
    "bezel_rect_cm": {
      "x_min_cm": 0.8,
      "y_min_cm": 0.7,
      "x_max_cm": 8.0,
      "y_max_cm": 7.1,
      "width_cm": 7.2,
      "height_cm": 6.4
    },
    "active_area_landscape_cm": {
      "width_cm": 4.9,
      "height_cm": 3.67,
      "x_min_cm": 1.95,
      "y_min_cm": 2.065,
      "x_max_cm": 6.85,
      "y_max_cm": 5.735
    },
    "module_pcb_cm": {
      "width_cm": 4.28,
      "height_cm": 7.72,
      "thickness_cm": 0.5
    }
  },
  "controls": {
    "dpad_center_cm": [
      2.2,
      9.2
    ],
    "dpad_arm_offset_cm": 1.1,
    "abxy_center_cm": [
      6.6,
      9.2
    ],
    "abxy_arm_offset_cm": 1.0,
    "abxy_adjacent_center_distance_cm": 1.414,
    "switch_options": {
      "omron_12x12mm": {
        "part": "OMRON B3F-4055 (1.20 x 1.20 cm)",
        "body_width_cm": 1.2,
        "abxy_edge_gap_cm": 0.214,
        "verdict": "TIGHT \u2014 0.214 cm (2.14 mm) body gap on ABXY diamond; adjacent switch legs sit only 1 perfboard hole (0.254 cm) apart."
      },
      "tactile_6x6mm": {
        "part": "6x6x7 mm Through-Hole Tactile Switch (0.60 x 0.60 cm)",
        "body_width_cm": 0.6,
        "abxy_edge_gap_cm": 0.814,
        "verdict": "RECOMMENDED \u2014 0.814 cm (8.14 mm) body gap on ABXY diamond; leaves 2-3 full perfboard holes between adjacent switches for easy hand soldering."
      }
    },
    "buttons": [
      {
        "name": "BTN_UP",
        "label": "UP",
        "gpio": 4,
        "soldered": true,
        "group": "dpad",
        "target_x_cm": 2.2,
        "target_y_cm": 8.1,
        "perfboard_col": 7,
        "perfboard_row": 31,
        "snapped_x_cm": 2.114,
        "snapped_y_cm": 8.105,
        "snap_offset_cm": 0.086
      },
      {
        "name": "BTN_DOWN",
        "label": "DOWN",
        "gpio": 5,
        "soldered": true,
        "group": "dpad",
        "target_x_cm": 2.2,
        "target_y_cm": 10.3,
        "perfboard_col": 7,
        "perfboard_row": 40,
        "snapped_x_cm": 2.114,
        "snapped_y_cm": 10.391,
        "snap_offset_cm": 0.125
      },
      {
        "name": "BTN_LEFT",
        "label": "LEFT",
        "gpio": 6,
        "soldered": true,
        "group": "dpad",
        "target_x_cm": 1.1,
        "target_y_cm": 9.2,
        "perfboard_col": 3,
        "perfboard_row": 35,
        "snapped_x_cm": 1.098,
        "snapped_y_cm": 9.121,
        "snap_offset_cm": 0.079
      },
      {
        "name": "BTN_RIGHT",
        "label": "RIGHT",
        "gpio": 7,
        "soldered": true,
        "group": "dpad",
        "target_x_cm": 3.3,
        "target_y_cm": 9.2,
        "perfboard_col": 12,
        "perfboard_row": 35,
        "snapped_x_cm": 3.384,
        "snapped_y_cm": 9.121,
        "snap_offset_cm": 0.115
      },
      {
        "name": "BTN_A",
        "label": "A",
        "gpio": 16,
        "soldered": true,
        "group": "abxy",
        "target_x_cm": 7.6,
        "target_y_cm": 9.2,
        "perfboard_col": 29,
        "perfboard_row": 35,
        "snapped_x_cm": 7.702,
        "snapped_y_cm": 9.121,
        "snap_offset_cm": 0.129
      },
      {
        "name": "BTN_B",
        "label": "B",
        "gpio": 17,
        "soldered": true,
        "group": "abxy",
        "target_x_cm": 6.6,
        "target_y_cm": 10.2,
        "perfboard_col": 25,
        "perfboard_row": 39,
        "snapped_x_cm": 6.686,
        "snapped_y_cm": 10.137,
        "snap_offset_cm": 0.107
      },
      {
        "name": "BTN_X",
        "label": "X",
        "gpio": 14,
        "soldered": false,
        "group": "abxy",
        "target_x_cm": 6.6,
        "target_y_cm": 8.2,
        "perfboard_col": 25,
        "perfboard_row": 31,
        "snapped_x_cm": 6.686,
        "snapped_y_cm": 8.105,
        "snap_offset_cm": 0.128
      },
      {
        "name": "BTN_Y",
        "label": "Y",
        "gpio": 42,
        "soldered": false,
        "group": "abxy",
        "target_x_cm": 5.6,
        "target_y_cm": 9.2,
        "perfboard_col": 21,
        "perfboard_row": 35,
        "snapped_x_cm": 5.67,
        "snapped_y_cm": 9.121,
        "snap_offset_cm": 0.106
      },
      {
        "name": "BTN_SELECT",
        "label": "SELECT",
        "gpio": 21,
        "soldered": true,
        "group": "system",
        "target_x_cm": 3.6,
        "target_y_cm": 11.3,
        "perfboard_col": 13,
        "perfboard_row": 44,
        "snapped_x_cm": 3.638,
        "snapped_y_cm": 11.407,
        "snap_offset_cm": 0.114
      },
      {
        "name": "BTN_START",
        "label": "START",
        "gpio": 18,
        "soldered": true,
        "group": "system",
        "target_x_cm": 5.2,
        "target_y_cm": 11.3,
        "perfboard_col": 19,
        "perfboard_row": 44,
        "snapped_x_cm": 5.162,
        "snapped_y_cm": 11.407,
        "snap_offset_cm": 0.114
      },
      {
        "name": "BTN_L",
        "label": "L",
        "gpio": 47,
        "soldered": false,
        "group": "shoulder",
        "target_x_cm": 1.6,
        "target_y_cm": 0.0,
        "perfboard_col": 5,
        "perfboard_row": 1,
        "snapped_x_cm": 1.606,
        "snapped_y_cm": 0.485,
        "snap_offset_cm": 0.485
      },
      {
        "name": "BTN_R",
        "label": "R",
        "gpio": 48,
        "soldered": false,
        "group": "shoulder",
        "target_x_cm": 7.2,
        "target_y_cm": 0.0,
        "perfboard_col": 27,
        "perfboard_row": 1,
        "snapped_x_cm": 7.194,
        "snapped_y_cm": 0.485,
        "snap_offset_cm": 0.485
      }
    ]
  },
  "spi_and_auxiliary_pins": {
    "TFT_MOSI": 11,
    "TFT_SCK": 12,
    "TFT_CS": 10,
    "TFT_DC": 8,
    "TFT_RST": 9,
    "SD_MISO": 15,
    "SD_CS": 13,
    "I2S_BCLK": 38,
    "I2S_LRC": 39,
    "I2S_DIN": 40,
    "BATTERY_ADC_PIN": 1,
    "I2C_SDA": 43,
    "I2C_SCL": 44,
    "reserved_pins": [
      0,
      3,
      26,
      27,
      28,
      29,
      30,
      31,
      32,
      33,
      34,
      35,
      36,
      37,
      45,
      46
    ]
  },
  "z_stackup": {
    "total_thickness_cm": 3.2,
    "layers": [
      {
        "name": "Front shell wall & display lens",
        "z_start_cm": 0.0,
        "z_end_cm": 0.2,
        "thickness_cm": 0.2
      },
      {
        "name": "Tactile switches, button caps & ST7789 panel above perfboard",
        "z_start_cm": 0.2,
        "z_end_cm": 0.9,
        "thickness_cm": 0.7
      },
      {
        "name": "Perfboard FR-4 substrate (31x46 holes)",
        "z_start_cm": 0.9,
        "z_end_cm": 1.06,
        "thickness_cm": 0.16
      },
      {
        "name": "Rear solder joints & 28-30 AWG point-to-point wiring",
        "z_start_cm": 1.06,
        "z_end_cm": 1.26,
        "thickness_cm": 0.2
      },
      {
        "name": "Rear electronics & battery bay (split into Upper Y and Lower Y zones)",
        "z_start_cm": 1.26,
        "z_end_cm": 3.0,
        "thickness_cm": 1.74
      },
      {
        "name": "Back shell wall & battery door",
        "z_start_cm": 3.0,
        "z_end_cm": 3.2,
        "thickness_cm": 0.2
      }
    ],
    "rear_zones": {
      "upper_zone": {
        "y_range_cm": [
          1.0,
          7.6
        ],
        "available_depth_cm": 1.74,
        "components": [
          {
            "name": "ESP32-S3-DevKitC-1-N16R8 (44-pin carrier)",
            "x_range_cm": [
              1.098,
              3.638
            ],
            "y_range_cm": [
              1.247,
              7.577
            ],
            "perfboard_cols": [
              3,
              13
            ],
            "perfboard_rows": [
              4,
              29
            ],
            "width_cm": 2.54,
            "height_cm": 6.33,
            "depth_cm": 1.35,
            "clearance_cm": 0.39
          },
          {
            "name": "MicroSD Breakout Module (3.3V SPI)",
            "x_range_cm": [
              4.4,
              6.8
            ],
            "y_range_cm": [
              1.5,
              5.7
            ],
            "perfboard_cols": [
              16,
              25
            ],
            "perfboard_rows": [
              5,
              21
            ],
            "width_cm": 2.4,
            "height_cm": 4.2,
            "depth_cm": 0.4,
            "clearance_cm": 1.34
          },
          {
            "name": "Pololu 5V Step-Up Boost Module (U3V40F5 / S7V7F5)",
            "x_range_cm": [
              4.9,
              6.93
            ],
            "y_range_cm": [
              5.95,
              7.47
            ],
            "perfboard_cols": [
              18,
              26
            ],
            "perfboard_rows": [
              22,
              28
            ],
            "width_cm": 2.03,
            "height_cm": 1.52,
            "depth_cm": 0.5,
            "clearance_cm": 1.24
          }
        ]
      },
      "lower_zone": {
        "y_range_cm": [
          7.8,
          11.8
        ],
        "available_depth_cm": 1.74,
        "components": [
          {
            "name": "2xAA NiMH Holder (Recommended) or 3xAA Holder",
            "x_range_cm": [
              1.5,
              7.3
            ],
            "y_range_cm": [
              8.1,
              11.3
            ],
            "width_cm": 5.8,
            "height_cm": 3.2,
            "depth_cm": 1.65,
            "clearance_cm": 0.09,
            "note": "Sits behind the flat solder side of D-pad and ABXY switches. Must NOT overlap the DevKitC-1 in Y (stacking both in Z would require 4.46 cm vs 3.20 cm shell thickness)."
          }
        ]
      }
    }
  }
};

export function resolveLayoutDimensions(layout = DEFAULT_HARDWARE_LAYOUT) {
  const ref = layout.enclosure.owner_reference_cm;
  const pb = layout.perfboard;
  const disp = layout.display;
  const sideWall = Number(ref.side_wall_cm ?? 0.21);
  const frontWall = Number(ref.front_wall_cm ?? 0.20);
  const backWall = Number(ref.back_wall_cm ?? 0.20);
  const rearUpper = layout.z_stackup?.rear_zones?.upper_zone;
  const rearAvail = Number(rearUpper?.available_depth_cm ?? 1.74);
  return {
    shellWidthCm: Number(ref.width_cm),
    shellHeightCm: Number(ref.height_cm),
    shellDepthCm: Number(ref.thickness_cm),
    cornerRadiusCm: Number(ref.corner_radius_cm ?? 0.90),
    shellSideWallCm: sideWall,
    shellFrontWallCm: frontWall,
    shellBackWallCm: backWall,
    innerWidthCm: Number((Number(ref.width_cm) - 2 * sideWall).toFixed(2)),
    innerHeightCm: Number((Number(ref.height_cm) - 2 * sideWall).toFixed(2)),
    innerDepthCm: Number((Number(ref.thickness_cm) - frontWall - backWall).toFixed(2)),
    perfboardCols: Number(pb.columns),
    perfboardRows: Number(pb.rows),
    perfboardPitchCm: Number(pb.pitch_cm),
    perfboardWidthCm: Number(pb.width_cm),
    perfboardHeightCm: Number(pb.height_cm),
    perfboardThicknessCm: Number(pb.thickness_cm),
    totalVias: Number(pb.columns) * Number(pb.rows),
    displayWidthCm: Number(disp.active_area_landscape_cm.width_cm),
    displayHeightCm: Number(disp.active_area_landscape_cm.height_cm),
    rearAvailDepthCm: rearAvail,
  };
}

export async function loadHardwareLayout() {
  if (typeof fetch === "function" && typeof window !== "undefined" && window.location?.protocol !== "file:") {
    try {
      const resp = await fetch("/hardware/layout.json");
      if (resp.ok) {
        return await resp.json();
      }
    } catch {
      // Offline fallback to synchronized SSOT snapshot.
    }
  }
  return DEFAULT_HARDWARE_LAYOUT;
}
