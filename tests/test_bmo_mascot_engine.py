import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "ui"


class BmoMascotSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = (FIRMWARE / "bmo_face.h").read_text(encoding="utf-8")
        cls.source = (FIRMWARE / "bmo_face.cpp").read_text(encoding="utf-8")

    def test_framebuffer_dimensions_match_psram_allocation(self):
        width = int(re.search(r"#define\s+FACE_FB_W\s+(\d+)", self.header).group(1))
        height = int(re.search(r"#define\s+FACE_FB_H\s+(\d+)", self.header).group(1))
        allocation = re.search(r"heap_caps_malloc\(FACE_FB_W\s*\*\s*FACE_FB_H\s*\*\s*sizeof\(uint16_t\)", self.source)
        self.assertIsNotNone(allocation)
        self.assertEqual((width, height, width * height * 2), (320, 240, 153600))

    def test_expression_enum_and_target_rows_match_in_order(self):
        enum_body = re.search(r"enum BmoExpression\s*\{(.*?)\};", self.header, re.S).group(1)
        names = re.findall(r"^\s*([A-Z][A-Z0-9_]*)\s*(?:=\s*\d+)?\s*,?\s*(?://.*)?$", enum_body, re.M)
        table_body = re.search(r"static const ExprTarget EXPR_TABLE\[\]\s*=\s*\{(.*?)\n\};", self.source, re.S).group(1)
        rows = re.findall(r"/\*\s*([A-Z][A-Z0-9_]*)\s*\*/", table_body)
        self.assertEqual(len(names), 18)
        self.assertEqual(rows, names)

    def test_spring_implementation_uses_backward_euler_and_guards_dt(self):
        spring = (FIRMWARE / "harmonic_spring.h").read_text(encoding="utf-8")
        self.assertIn("!std::isfinite(dt) || dt <= 0.0f", spring)
        self.assertIn("if (dt > 0.05f) dt = 0.05f", spring)
        self.assertIn("const float inverseDeterminant = 1.0f / (f + dtSquaredOmegaSquared)", spring)
        self.assertIn("const float nextValue =", spring)
        self.assertIn("const float nextVelocity =", spring)

    def test_speech_particle_and_dance_limits_match_source(self):
        self.assertIn("static char          s_speechText[64]", self.source)
        self.assertIn("strncpy(s_speechText, quote, sizeof(s_speechText) - 1)", self.source)
        self.assertIn("s_speechText[sizeof(s_speechText) - 1] = '\\0'", self.source)
        self.assertIn("nowMs - s_speechLastCharMs >= 25", self.source)
        self.assertIn("static const int MAX_PARTICLES = 8", self.source)
        self.assertIn("nowMs - lastZSpawn > 1800", self.source)
        self.assertIn("(bpm > 40.0f && bpm < 240.0f) ? bpm : 125.0f", self.source)


if __name__ == "__main__":
    unittest.main()
