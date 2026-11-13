"""Host tests for tasks.py, case-sensitive #include validation, dormant guards, and CI triggers."""

import tempfile
import unittest
from pathlib import Path

import tasks

REPO_ROOT = Path(__file__).resolve().parents[1]


class BuildAndTasksTests(unittest.TestCase):
    def test_sketch_yaml_pins_core_and_libraries(self):
        cfg = tasks.parse_sketch_yaml()
        self.assertEqual(cfg["default_profile"], "esp32s3")
        self.assertIn("FlashMode=qio", str(cfg["fqbn"]))
        self.assertIn("PSRAM=opi", str(cfg["fqbn"]))
        platforms = cfg["platforms"]
        self.assertEqual(len(platforms), 1)
        self.assertEqual(platforms[0]["id"], "esp32:esp32")
        self.assertEqual(platforms[0]["version"], "3.3.11")
        libs = dict(cfg["libraries"])
        self.assertIn("Adafruit BusIO", libs)
        self.assertIn("Adafruit GFX Library", libs)
        self.assertIn("Adafruit ST7735 and ST7789 Library", libs)

    def test_include_case_sensitivity_clean_and_detects_mismatch(self):
        self.assertEqual(tasks.check_include_case_sensitivity(REPO_ROOT), [])

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src_dir = root / "firmware" / "BmoGameboy" / "src" / "cores"
            hdr_dir = root / "firmware" / "BmoGameboy" / "src" / "third_party" / "fake08"
            src_dir.mkdir(parents=True)
            hdr_dir.mkdir(parents=True)
            (hdr_dir / "graphics.h").write_text("// header\n", encoding="utf-8")
            (src_dir / "emu_pico.cpp").write_text(
                '#include "../third_party/fake08/Graphics.h"\n',
                encoding="utf-8",
            )
            issues = tasks.check_include_case_sensitivity(root)
            self.assertEqual(len(issues), 1)
            self.assertIn("Graphics.h", issues[0])
            self.assertIn("graphics.h", issues[0])

    def test_dormant_modules_require_explicit_hardware_confirmation(self):
        audio_src = (REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "hal" / "audio_i2s.cpp").read_text(
            encoding="utf-8"
        )
        batt_src = (REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "hal" / "battery.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("BMO_HARDWARE_CONFIRMED_AUDIO", audio_src)
        self.assertIn("#error", audio_src)
        self.assertIn("BMO_HARDWARE_CONFIRMED_BATTERY", batt_src)
        self.assertIn("#error", batt_src)

    def test_ci_workflow_scopes_triggers_and_concurrency(self):
        ci_text = (REPO_ROOT / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
        self.assertIn("concurrency:", ci_text)
        self.assertIn("cancel-in-progress: true", ci_text)
        self.assertIn("branches:\n      - main", ci_text)
        self.assertIn("audio-compile-only", ci_text)
        self.assertIn("battery-compile-only", ci_text)
        self.assertIn("debug-log-compile-only", ci_text)


if __name__ == "__main__":
    unittest.main()
