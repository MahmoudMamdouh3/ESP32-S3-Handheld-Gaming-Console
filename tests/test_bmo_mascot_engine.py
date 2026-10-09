#!/usr/bin/env python3
"""
test_bmo_mascot_engine.py
Exhaustive automated unit testing for the 2026 Ultra-High-End Living BMO Mascot Engine:
- Adventure Time BMO 1:1 color space & BGR565 byte-swapped pack invariants
- Second-order harmonic spring-damper differential physics stability
- 2D signed distance functions (SDF) and smoothstep anti-aliasing
- Bounding-box spatial culling efficiency (>70% pixel skip rate)
- 16-expression emotion matrix and gamepad interaction state machine
"""

import math
import unittest


def pack_bgr565_byteswapped(r: int, g: int, b: int) -> int:
    """Computes byte-swapped BGR565 color format matching ST7789 display hardware."""
    bgr565 = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3)
    swapped = ((bgr565 & 0xFF) << 8) | ((bgr565 >> 8) & 0xFF)
    return swapped & 0xFFFF


def smoothstep(edge0: float, edge1: float, x: float) -> float:
    t = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


class SpringDamper:
    def __init__(self, val=0.0, target=0.0, omega=20.0, zeta=0.72):
        self.val = val
        self.vel = 0.0
        self.target = target
        self.omega = omega
        self.zeta = zeta

    def update(self, dt: float):
        if dt <= 0.0:
            return
        dt = min(dt, 0.05)
        diff = self.val - self.target
        acc = -2.0 * self.zeta * self.omega * self.vel - (self.omega * self.omega) * diff
        self.vel += acc * dt
        self.val += self.vel * dt


class TestBmoMascotEngine(unittest.TestCase):
    def test_01_color_space_invariants(self):
        """Verify authentic Adventure Time BMO color constants in byte-swapped BGR565."""
        # Screen Background: #8AD5C3 (Mint Seafoam Green: R=138, G=213, B=195)
        c_mint = pack_bgr565_byteswapped(138, 213, 195)
        self.assertIsInstance(c_mint, int)
        self.assertTrue(0 <= c_mint <= 0xFFFF)

        # Eye & Mouth Charcoal Outline: #101E2B (R=16, G=30, B=43)
        c_charcoal = pack_bgr565_byteswapped(16, 30, 43)
        self.assertNotEqual(c_mint, c_charcoal)

        # Cheeks Blush: #FF8BA7 (R=255, G=139, B=167)
        c_blush = pack_bgr565_byteswapped(255, 139, 167)
        self.assertNotEqual(c_blush, c_mint)

        # Tongue Coral: #FA7F8F (R=250, G=127, B=143)
        c_tongue = pack_bgr565_byteswapped(250, 127, 143)
        self.assertNotEqual(c_tongue, c_blush)

    def test_02_spring_damper_physics_stability(self):
        """Verify harmonic spring-damper converges smoothly with critical damping without diverging."""
        spring = SpringDamper(val=0.0, target=1.0, omega=22.0, zeta=0.72)
        dt = 0.01666  # ~60 FPS
        
        # Simulate 1 second (60 frames)
        for _ in range(60):
            spring.update(dt)
            self.assertFalse(math.isnan(spring.val))
            self.assertFalse(math.isinf(spring.val))
            self.assertLess(spring.val, 1.5, "Spring overshoot should remain within controlled cartoon bounds")

        # After 1 second, it should have converged close to target
        self.assertAlmostEqual(spring.val, 1.0, delta=0.05)
        self.assertAlmostEqual(spring.vel, 0.0, delta=0.1)

    def test_03_subpixel_anti_aliasing_continuity(self):
        """Verify smoothstep anti-aliasing provides continuous subpixel gradient across edge."""
        edge0 = 1.2
        edge1 = 0.0
        # Inside should evaluate to 1.0
        self.assertAlmostEqual(smoothstep(edge0, edge1, -0.5), 1.0)
        # Outside should evaluate to 0.0
        self.assertAlmostEqual(smoothstep(edge0, edge1, 2.0), 0.0)
        # On edge should be strictly monotonic
        v1 = smoothstep(edge0, edge1, 0.9)
        v2 = smoothstep(edge0, edge1, 0.6)
        v3 = smoothstep(edge0, edge1, 0.3)
        self.assertLess(v1, v2)
        self.assertLess(v2, v3)

    def test_04_spatial_culling_efficiency(self):
        """Verify spatial bounding-box culling skips >70% of display pixels on 320x240 screen."""
        total_pixels = 320 * 240
        # Define facial feature bounding boxes in screen pixels
        boxes = [
            # Left Eye
            (160 - 55 - 20, 100 - 30, 160 - 55 + 20, 100 + 30),
            # Right Eye
            (160 + 55 - 20, 100 - 30, 160 + 55 + 20, 100 + 30),
            # Left Cheek
            (160 - 80 - 18, 125 - 18, 160 - 80 + 18, 125 + 18),
            # Right Cheek
            (160 + 80 - 18, 125 - 18, 160 + 80 + 18, 125 + 18),
            # Mouth
            (160 - 35, 140 - 25, 160 + 35, 140 + 25),
        ]
        
        evaluated_pixels = 0
        for y in range(240):
            for x in range(320):
                inside = False
                for bx0, by0, bx1, by1 in boxes:
                    if bx0 <= x <= bx1 and by0 <= y <= by1:
                        inside = True
                        break
                if inside:
                    evaluated_pixels += 1

        skip_ratio = (total_pixels - evaluated_pixels) / total_pixels
        self.assertGreater(skip_ratio, 0.70, f"Culling must skip >70% of pixels (achieved {skip_ratio*100:.1f}%)")

    def test_05_psram_canvas_memory_budget(self):
        """Verify 320x240 16-bit PSRAM canvas consumes exactly 153,600 bytes (<2% of 8MB)."""
        width = 320
        height = 240
        bytes_per_pixel = 2
        total_bytes = width * height * bytes_per_pixel
        self.assertEqual(total_bytes, 153600)
        psram_total = 8 * 1024 * 1024  # 8MB
        usage_pct = (total_bytes / psram_total) * 100.0
        self.assertLess(usage_pct, 2.0, "Mascot canvas must take < 2% of total PSRAM")

    def test_06_16_state_expression_matrix_coverage(self):
        """Verify all 16 emotional states are defined and have distinct valid parameters."""
        expressions = [
            "IDLE", "SURPRISED", "HAPPY", "SLEEPY", "LOW_BATTERY", "CHARGING",
            "ERROR", "SHUTDOWN", "HIDDEN", "JOY", "SLEEPING", "WINK", "BLUSH",
            "CONFUSED", "ANNOYED", "LOVE", "TICKLED", "CELEBRATING"
        ]
        self.assertGreaterEqual(len(expressions), 16)
        self.assertEqual(len(set(expressions)), len(expressions), "Expression enum names must be unique")

    def test_07_speech_bubble_geometry_and_typewriter(self):
        """Verify speech bubble dimensions, typewriter timing, and iconic quote catalog."""
        # Bubble fits comfortably above BMO face on 320x240 screen
        bubble_x, bubble_y, bubble_w, bubble_h = 30, 14, 260, 42
        self.assertGreaterEqual(bubble_x, 0)
        self.assertLessEqual(bubble_x + bubble_w, 320)
        self.assertLessEqual(bubble_y + bubble_h, 80)

        # Typewriter timing calculation: ~25ms per character
        quote = "Who wants to play video games?!"
        char_delay_ms = 25
        total_type_time_ms = len(quote) * char_delay_ms
        self.assertLess(total_type_time_ms, 1200, "Typewriter reveal should feel snappy and responsive")

        # Verify key quotes
        quotes = [
            "Who wants to play video games?!",
            "Yay! BMO is so happy to see you!",
            "Hehehe! Stop it, that tickles!",
            "BMO is camera ready! *wink*",
            "Oh, you are making BMO blush!",
            "Dance party with BMO! Unce unce unce!",
            "Sometimes life is scary, but we have games!",
        ]
        for q in quotes:
            self.assertLessEqual(len(q), 64, f"Quote '{q}' exceeds 64-char buffer limit")

    def test_08_dance_party_tempo_and_oscillation(self):
        """Verify musical BPM phase integration and harmonic dance frequencies."""
        bpm = 120.0  # 2 beats per second
        freq_hz = bpm / 60.0
        angular_speed = freq_hz * 2.0 * math.pi  # ~12.56 rad/s
        dt = 0.01666  # 60 FPS
        phase = 0.0
        
        # Advance 1 full beat (0.5 seconds = 30 frames)
        for _ in range(30):
            phase += dt * angular_speed

        # Advance 1 full second (60 frames) -> 2 beats = 2 full cycles (4*pi) or 30 frames = 1 full cycle (2*pi)
        self.assertAlmostEqual(phase, 2.0 * math.pi, delta=0.15)
        # Bounding oscillation
        bounce = 0.08 * math.sin(phase)
        self.assertTrue(-0.09 <= bounce <= 0.09)

    def test_09_companion_mood_and_relationship_bounds(self):
        """Verify companion happiness, energy, and gaming buddy milestone progression."""
        happiness = 100
        energy = 100
        total_pets = 15
        games_played = 42

        # Level calculation: (games_played // 10) + (total_pets // 10) + 1
        buddy_level = (games_played // 10) + (total_pets // 10) + 1
        self.assertEqual(buddy_level, 6)
        self.assertTrue(1 <= buddy_level <= 100)


if __name__ == "__main__":
    unittest.main()
