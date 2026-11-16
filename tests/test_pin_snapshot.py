# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Pin-map snapshot and cross-file parity test across config.h, pins_check.h, bringup.ino, layout.json, and docs."""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
CONFIG_H = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "hal" / "config.h"
PINS_CHECK_H = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "hal" / "pins_check.h"
BRINGUP_INO = REPO_ROOT / "firmware" / "bringup" / "bringup.ino"
LAYOUT_JSON = REPO_ROOT / "hardware" / "layout.json"
DIAGRAM_JSON = REPO_ROOT / "hardware" / "diagram.json"
HARDWARE_MD = REPO_ROOT / "docs" / "hardware.md"
WIRING_DISPLAY_MD = REPO_ROOT / "docs" / "wiring" / "01-display-and-buttons.md"
WIRING_SD_MD = REPO_ROOT / "docs" / "wiring" / "02-sd-card.md"
AGENTS_MD = REPO_ROOT / "AGENTS.md"
FW_AGENTS_MD = REPO_ROOT / "firmware" / "AGENTS.md"

EXPECTED_ACTIVE_AND_EXPANSION_PINS = {
    "TFT_MOSI": 11,
    "TFT_SCK": 12,
    "TFT_CS": 10,
    "TFT_DC": 8,
    "TFT_RST": 9,
    "SD_MISO": 15,
    "SD_CS": 13,
    "BTN_UP": 4,
    "BTN_DOWN": 5,
    "BTN_LEFT": 6,
    "BTN_RIGHT": 7,
    "BTN_A": 16,
    "BTN_B": 17,
    "BTN_START": 18,
    "BTN_SELECT": 21,
    "BTN_X": 14,
    "BTN_Y": 42,
    "BTN_L": 47,
    "BTN_R": 48,
}

EXPECTED_AUX_PINS = {
    "I2S_BCLK": 38,
    "I2S_LRC": 39,
    "I2S_DIN": 40,
    "BATTERY_ADC_PIN": 1,
    "I2C_SDA": 43,
    "I2C_SCL": 44,
}

RESERVED_PINS = {0, 3, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 45, 46}


def read_config_bundle(config_path: Path) -> str:
    visited: set[Path] = set()
    chunks: list[str] = []

    def _visit(path: Path) -> None:
        resolved = path.resolve()
        if resolved in visited or not resolved.is_file():
            return
        visited.add(resolved)
        text = resolved.read_text(encoding="utf-8")
        chunks.append(text)
        for match in re.finditer(r'^\s*#include\s+"([^"]+)"', text, re.MULTILINE):
            _visit(resolved.parent / match.group(1))

    _visit(config_path)
    return "\n".join(chunks)


def parse_defines(text: str) -> dict[str, int]:
    defines: dict[str, int] = {}
    for match in re.finditer(r"^\s*#define\s+([A-Z0-9_]+)\s+(-?\d+)\b", text, re.MULTILINE):
        defines[match.group(1)] = int(match.group(2))
    return defines


class PinSnapshotTests(unittest.TestCase):
    def test_config_h_matches_pin_snapshot_and_hard_stops(self) -> None:
        raw_config = CONFIG_H.read_text(encoding="utf-8")
        self.assertIn('#include "pins_check.h"', raw_config)
        self.assertTrue(PINS_CHECK_H.is_file(), "pins_check.h must exist beside config.h")

        defines = parse_defines(read_config_bundle(CONFIG_H))
        self.assertEqual(defines.get("FEATURE_BATTERY_MONITOR"), 0)
        self.assertEqual(defines.get("FEATURE_AUDIO"), 0)

        for name, expected_gpio in {**EXPECTED_ACTIVE_AND_EXPANSION_PINS, **EXPECTED_AUX_PINS}.items():
            self.assertEqual(
                defines.get(name),
                expected_gpio,
                f"Pin {name} in config.h changed from {expected_gpio} to {defines.get(name)}",
            )

        # Verify uniqueness and reserved pin separation for all active + expansion pins
        used_gpios = list(EXPECTED_ACTIVE_AND_EXPANSION_PINS.values())
        self.assertEqual(len(used_gpios), len(set(used_gpios)), "Duplicate GPIO assignment in active/expansion pins")
        for name, gpio in EXPECTED_ACTIVE_AND_EXPANSION_PINS.items():
            self.assertNotIn(gpio, RESERVED_PINS, f"Pin {name} (GPIO {gpio}) collides with reserved strapping/flash/PSRAM pin")

    def test_bringup_sketch_pins_match_config_h(self) -> None:
        self.assertTrue(BRINGUP_INO.is_file(), "firmware/bringup/bringup.ino must exist")
        bringup_defines = parse_defines(BRINGUP_INO.read_text(encoding="utf-8"))
        for name, expected_gpio in EXPECTED_ACTIVE_AND_EXPANSION_PINS.items():
            self.assertEqual(
                bringup_defines.get(name),
                expected_gpio,
                f"bringup.ino pin {name} ({bringup_defines.get(name)}) does not match config.h ({expected_gpio})",
            )

    def test_layout_json_and_docs_match_pin_snapshot(self) -> None:
        layout = json.loads(LAYOUT_JSON.read_text(encoding="utf-8"))
        btn_map = {b["name"]: b["gpio"] for b in layout["controls"]["buttons"]}
        for name, expected_gpio in EXPECTED_ACTIVE_AND_EXPANSION_PINS.items():
            if name.startswith("BTN_"):
                self.assertEqual(btn_map.get(name), expected_gpio, f"layout.json button {name} mismatch")
            else:
                self.assertEqual(
                    layout["spi_and_auxiliary_pins"].get(name),
                    expected_gpio,
                    f"layout.json SPI pin {name} mismatch",
                )
        for name, expected_gpio in EXPECTED_AUX_PINS.items():
            self.assertEqual(
                layout["spi_and_auxiliary_pins"].get(name),
                expected_gpio,
                f"layout.json auxiliary pin {name} mismatch",
            )
        self.assertEqual(set(layout["spi_and_auxiliary_pins"]["reserved_pins"]), RESERVED_PINS)

        hw_doc = HARDWARE_MD.read_text(encoding="utf-8")
        for token in ("TFT_SCK", "TFT_MOSI", "TFT_CS", "TFT_DC", "TFT_RST", "SD_CS", "SD_MISO"):
            self.assertIn(token, hw_doc)

    def test_wokwi_diagram_agents_and_wiring_docs_match_config_h(self) -> None:
        diagram = json.loads(DIAGRAM_JSON.read_text(encoding="utf-8"))
        pin_to_target = {}
        for src, dst, *_ in diagram["connections"]:
            if src.startswith("esp:") and src[4:].isdigit():
                pin_to_target[dst] = int(src[4:])
        expected_diagram = {
            "tft:CS": EXPECTED_ACTIVE_AND_EXPANSION_PINS["TFT_CS"],
            "tft:D/C": EXPECTED_ACTIVE_AND_EXPANSION_PINS["TFT_DC"],
            "tft:RST": EXPECTED_ACTIVE_AND_EXPANSION_PINS["TFT_RST"],
            "tft:MOSI": EXPECTED_ACTIVE_AND_EXPANSION_PINS["TFT_MOSI"],
            "tft:SCK": EXPECTED_ACTIVE_AND_EXPANSION_PINS["TFT_SCK"],
            "sd:CS": EXPECTED_ACTIVE_AND_EXPANSION_PINS["SD_CS"],
            "sd:DO": EXPECTED_ACTIVE_AND_EXPANSION_PINS["SD_MISO"],
            "btn_up:1.l": EXPECTED_ACTIVE_AND_EXPANSION_PINS["BTN_UP"],
            "btn_down:1.l": EXPECTED_ACTIVE_AND_EXPANSION_PINS["BTN_DOWN"],
            "btn_left:1.l": EXPECTED_ACTIVE_AND_EXPANSION_PINS["BTN_LEFT"],
            "btn_right:1.l": EXPECTED_ACTIVE_AND_EXPANSION_PINS["BTN_RIGHT"],
            "btn_a:1.l": EXPECTED_ACTIVE_AND_EXPANSION_PINS["BTN_A"],
            "btn_b:1.l": EXPECTED_ACTIVE_AND_EXPANSION_PINS["BTN_B"],
            "btn_start:1.l": EXPECTED_ACTIVE_AND_EXPANSION_PINS["BTN_START"],
            "btn_select:1.l": EXPECTED_ACTIVE_AND_EXPANSION_PINS["BTN_SELECT"],
            "btn_x:1.l": EXPECTED_ACTIVE_AND_EXPANSION_PINS["BTN_X"],
            "btn_y:1.l": EXPECTED_ACTIVE_AND_EXPANSION_PINS["BTN_Y"],
            "btn_l:1.l": EXPECTED_ACTIVE_AND_EXPANSION_PINS["BTN_L"],
            "btn_r:1.l": EXPECTED_ACTIVE_AND_EXPANSION_PINS["BTN_R"],
        }
        for net, gpio in expected_diagram.items():
            self.assertEqual(pin_to_target.get(net), gpio, f"diagram.json {net} GPIO mismatch")

        agents_txt = AGENTS_MD.read_text(encoding="utf-8")
        for key, gpio in EXPECTED_ACTIVE_AND_EXPANSION_PINS.items():
            short = key.replace("BTN_", "")
            self.assertIn(f"{short}={gpio}", agents_txt, f"AGENTS.md missing {short}={gpio}")

        fw_agents_txt = FW_AGENTS_MD.read_text(encoding="utf-8")
        self.assertIn("0, 3, 26–37, 45, 46", fw_agents_txt)

        disp_txt = WIRING_DISPLAY_MD.read_text(encoding="utf-8")
        for key, gpio in EXPECTED_ACTIVE_AND_EXPANSION_PINS.items():
            if key.startswith("BTN_"):
                self.assertIn(f"`{key}` | {gpio}", disp_txt)
            elif key.startswith("TFT_"):
                self.assertIn(f"GPIO{gpio} / `{key}`", disp_txt)
        self.assertIn(f"MISO GPIO{EXPECTED_ACTIVE_AND_EXPANSION_PINS['SD_MISO']}", disp_txt)
        self.assertIn(f"CS GPIO{EXPECTED_ACTIVE_AND_EXPANSION_PINS['SD_CS']}", disp_txt)

        sd_txt = WIRING_SD_MD.read_text(encoding="utf-8")
        for key in ("TFT_SCK", "TFT_MOSI", "SD_MISO", "TFT_CS", "TFT_DC", "TFT_RST", "SD_CS"):
            self.assertIn(f"`GPIO {EXPECTED_ACTIVE_AND_EXPANSION_PINS[key]}` | `{key}`", sd_txt)


if __name__ == "__main__":
    unittest.main()
