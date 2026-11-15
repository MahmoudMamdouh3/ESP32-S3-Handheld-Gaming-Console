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
HARDWARE_MD = REPO_ROOT / "docs" / "hardware.md"

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


def parse_defines(text: str) -> dict[str, int]:
    defines: dict[str, int] = {}
    for match in re.finditer(r"^\s*#define\s+([A-Z0-9_]+)\s+(-?\d+)\b", text, re.MULTILINE):
        defines[match.group(1)] = int(match.group(2))
    return defines


class PinSnapshotTests(unittest.TestCase):
    def test_config_h_matches_pin_snapshot_and_hard_stops(self) -> None:
        text = CONFIG_H.read_text(encoding="utf-8")
        self.assertIn('#include "pins_check.h"', text)
        self.assertTrue(PINS_CHECK_H.is_file(), "pins_check.h must exist beside config.h")

        defines = parse_defines(text)
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


if __name__ == "__main__":
    unittest.main()
