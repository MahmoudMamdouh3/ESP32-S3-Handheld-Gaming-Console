import json
import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "core" / "config.h"
DIAGRAM_PATH = REPO_ROOT / "diagram.json"


def load_config_pins():
    source = CONFIG_PATH.read_text(encoding="utf-8")
    return {
        name: int(value)
        for name, value in re.findall(r"^#define\s+([A-Z][A-Z0-9_]*)\s+(\d+)\s*$", source, re.MULTILINE)
    }


class TestWokwiHardwareParity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config_pins = load_config_pins()
        cls.diagram = json.loads(DIAGRAM_PATH.read_text(encoding="utf-8"))
        cls.connections = {
            (connection[0], connection[1])
            for connection in cls.diagram["connections"]
        }

    def assert_connection(self, source, target):
        self.assertTrue(
            (source, target) in self.connections or (target, source) in self.connections,
            f"Missing Wokwi connection: {source} <-> {target}",
        )

    def test_spi_peripherals_match_firmware_pins(self):
        for macro, part_pin in (
            ("TFT_MOSI", "tft:MOSI"),
            ("TFT_SCK", "tft:SCK"),
            ("TFT_CS", "tft:CS"),
            ("TFT_DC", "tft:D/C"),
            ("SD_MISO", "sd:DO"),
            ("SD_CS", "sd:CS"),
        ):
            self.assert_connection(f"esp:{self.config_pins[macro]}", part_pin)

        self.assert_connection(f"esp:{self.config_pins['TFT_MOSI']}", "sd:DI")
        self.assert_connection(f"esp:{self.config_pins['TFT_SCK']}", "sd:SCK")

    def test_all_twelve_buttons_are_active_low_and_use_firmware_pins(self):
        buttons = {part["id"]: part for part in self.diagram["parts"]}
        for name in ("UP", "DOWN", "LEFT", "RIGHT", "A", "B", "START", "SELECT", "X", "Y", "L", "R"):
            part = f"btn_{name.lower()}"
            self.assertEqual(buttons[part]["type"], "wokwi-pushbutton")
            self.assert_connection(f"esp:{self.config_pins[f'BTN_{name}']}", f"{part}:1.l")
            self.assert_connection("esp:GND.1", f"{part}:2.l")

    def test_disabled_peripherals_and_reserved_gpio_are_not_wired(self):
        source = CONFIG_PATH.read_text(encoding="utf-8")
        self.assertRegex(source, r"(?m)^#define FEATURE_BATTERY_MONITOR 0$")
        self.assertRegex(source, r"(?m)^#define FEATURE_AUDIO 0$")

        wired_gpio = {
            int(match.group(1))
            for connection in self.diagram["connections"]
            for endpoint in connection[:2]
            if (match := re.fullmatch(r"esp:(\d+)", endpoint))
        }
        self.assertTrue(wired_gpio.isdisjoint({0, 1, 3, *range(33, 38), 38, 39, 40, 45, 46}))

    def test_required_simulator_parts_are_present(self):
        part_types = {part["type"] for part in self.diagram["parts"]}
        self.assertIn("board-esp32-s3-devkitc-1", part_types)
        self.assertIn("wokwi-ili9341", part_types)
        self.assertIn("wokwi-microsd-card", part_types)
        self.assertEqual(sum(part["type"] == "wokwi-pushbutton" for part in self.diagram["parts"]), 12)


if __name__ == "__main__":
    unittest.main()
