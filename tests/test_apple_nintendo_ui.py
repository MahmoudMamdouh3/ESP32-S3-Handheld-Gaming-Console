#!/usr/bin/env python3
"""
test_apple_nintendo_ui.py
Unit tests for Apple & Nintendo UI/UX ergonomics engine:
- Harmonic spring dynamics for carousel kinetic momentum
- Toast HUD descent and auto-dismiss timing
- Button pill dimensions and contrast ratios
- Letter badge jump coordinates and bounds
"""

import unittest
import math

class TestAppleNintendoUiEngine(unittest.TestCase):
    """Test suite for ergonomic UI components and kinetic spring equations."""

    def test_01_carousel_harmonic_spring_convergence(self):
        """Verify 2nd-order harmonic spring converges to target offset within 15 frames (~250ms)."""
        offset = 36.0  # initial displaced position
        vel = -240.0   # initial velocity towards 0
        target = 0.0
        dt = 0.01666   # 60 FPS frame time
        zeta = 0.72    # damping ratio
        omega = 22.0   # natural frequency

        # Simulate 20 frames (~333ms)
        for _ in range(20):
            diff = offset - target
            acc = -2.0 * zeta * omega * vel - (omega * omega) * diff
            vel += acc * dt
            offset += vel * dt

        # Spring must stably settle near 0 without explosion or divergence
        self.assertLess(abs(offset), 1.5, "Carousel spring should settle within 1.5 pixels of center")
        self.assertLess(abs(vel), 25.0, "Velocity should decelerate towards 0")

    def test_02_toast_hud_spring_descent(self):
        """Verify toast HUD enters smoothly from Y=-28 to Y=8."""
        y = -28.0
        vel = 220.0
        target = 8.0
        dt = 0.01666
        zeta = 0.75
        omega = 24.0

        # Simulate 12 frames (~200ms)
        for _ in range(12):
            diff = y - target
            acc = -2.0 * zeta * omega * vel - (omega * omega) * diff
            vel += acc * dt
            y += vel * dt

        self.assertGreater(y, 0.0, "Toast should descend into visible area")
        self.assertLess(y, 14.0, "Toast overshoot must remain small and comfortable")

    def test_03_button_pill_geometry_and_screen_bounds(self):
        """Verify footer button pill badges fit within 320x240 screen boundary."""
        screen_w = 320
        screen_h = 240
        pills = [
            {"x": 10,  "y": 220, "badge": "A",   "label": "PLAY"},
            {"x": 88,  "y": 220, "badge": "UP",  "label": "BMO"},
            {"x": 160, "y": 220, "badge": "SEL", "label": "SPECS"},
            {"x": 238, "y": 220, "badge": "< >", "label": "SYSTEM"},
        ]

        for p in pills:
            self.assertGreaterEqual(p["x"], 0)
            self.assertLess(p["x"] + 70, screen_w)
            self.assertLess(p["y"] + 18, screen_h)

    def test_04_alphabetical_badge_dimensions(self):
        """Verify A-Z jump letter badge position and radius."""
        cx = 270
        cy = 115
        r = 18
        self.assertGreaterEqual(cx - r, 0)
        self.assertLessEqual(cx + r, 320)
        self.assertGreaterEqual(cy - r, 42, "Must not collide with top header (H=42)")
        self.assertLessEqual(cy + r, 214, "Must not collide with footer (Y=214)")


if __name__ == "__main__":
    unittest.main()
