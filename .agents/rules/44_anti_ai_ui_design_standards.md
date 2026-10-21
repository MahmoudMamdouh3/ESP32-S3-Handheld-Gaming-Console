# 44. Anti-AI UI & Visual Craft Standards

**Purpose:** Eliminate generic AI-generated visual patterns, nested container sprawl, low-contrast text, missing interactive states, and ungridded layout drift. Enforces strict physical console ergonomics, token parity, and accessible design across both the embedded ST7789 display and the Web Simulator.

---

## 1. Scope & Authority

This rule governs all user interface development in the repository, including:
1. **Firmware Display Logic:** [`firmware/BmoGameboy/src/core/display_emu.cpp`](file:///firmware/BmoGameboy/src/core/display_emu.cpp), [`display_emu.h`](file:///firmware/BmoGameboy/src/core/display_emu.h), [`bmo_face.cpp`](file:///firmware/BmoGameboy/src/core/bmo_face.cpp), [`theme.h`](file:///firmware/BmoGameboy/src/core/theme.h).
2. **Web Simulator Frontend:** [`tools/bmo_simulator/index.html`](file:///tools/bmo_simulator/index.html), [`simulator.css`](file:///tools/bmo_simulator/simulator.css), [`simulator.js`](file:///tools/bmo_simulator/simulator.js).
3. **Design Specification:** [`DESIGN.md`](file:///DESIGN.md).

---

## 2. Prohibited AI Visual Anti-Patterns

Any code review or commit containing the following anti-patterns will be rejected:

### Anti-Pattern 1: Card-in-Card Nesting Syndrome
- **Prohibited:** Placing container cards inside other container cards (e.g., a card with borders inside a card with borders).
- **Required:** Use whitespace, structural typography, or subtle single-pixel divider lines to delineate sub-sections.

### Anti-Pattern 2: Generic Purple / Indigo Gradient Backgrounds
- **Prohibited:** Applying generic multi-stop purple/indigo radial or angular gradient meshes.
- **Required:** Surfaces must be grounded in the intentional BMO palette tokens (`Theme::BMO_DARK_FOREST`, `Theme::BMO_DEEP_TEAL`, or `--bg-primary: #0b0f19`).

### Anti-Pattern 3: Low-Contrast Muted Text (WCAG Violation)
- **Prohibited:** Dark gray text on black, or light mint text on bright yellow backgrounds that fail WCAG 2.1 AA.
- **Required:** Every text element must achieve at least $4.5:1$ contrast ratio against its direct background ($3:1$ for headers $> 18\text{ px}$).

### Anti-Pattern 4: Missing Keyboard Focus & Tactile Active States
- **Prohibited:** Clickable buttons, pills, and d-pads that lack `:focus-visible` outline rings or lack tactile mechanical depression on `:active`.
- **Required:** 
  - Keyboard focus: `outline: 2px solid var(--accent-gold); outline-offset: 2px;`.
  - Active press: `transform: translateY(2px)` with shadow collapse.

### Anti-Pattern 5: Arbitrary Off-Grid Dimensions
- **Prohibited:** Margins, paddings, gaps, and widths using arbitrary odd pixel amounts like `7px`, `11px`, `13px`, `17px`, `19px`.
- **Required:** All layout dimensions must strictly adhere to the 8px base grid or 4px sub-grid (`4px`, `8px`, `12px`, `16px`, `24px`, `32px`, `48px`).

### Anti-Pattern 6: Inlined Raw Hex Color Literals
- **Prohibited:** Writing raw hex literals (`0x1A4B`, `0xFFE0`, `#5FB49C`) directly inside view drawing methods or component rules.
- **Required:** Consume named tokens from `Theme::` in `theme.h` for C++ or CSS variables from `:root` in `simulator.css`.

---

## 3. Physical Handheld Performance Invariants

### 1. Selection State Outline Rule (Rule 08)
On the physical ST7789 TFT display, list and menu selection cursors MUST use `drawRoundRect()` outline rectangles. `fillRoundRect()` is strictly forbidden for selection highlighting.
- **Measured Cost:** Full rectangular fill blocks the SPI bus for $8.4\text{ ms}$; outline stroke takes $2.1\text{ ms}$, preserving the 60 Hz frame budget.

### 2. Damped Spring Motion Mechanics
Dynamic animations (carousel slides, toast HUD drops) MUST be driven by second-order harmonic spring-damper equations:
$$\ddot{x} + 2\zeta\omega_n \dot{x} + \omega_n^2 (x - x_{\text{target}}) = 0$$
- Damping ratio $\zeta = 0.72$
- Natural frequency $\omega_n = 22.0\text{ rad/s}$
- Blocking `delay()` inside any animation loop is strictly forbidden.

---

## 4. Verification Checklist Before Concluding Any UI Task

1. Run the dedicated UI auditor:
   ```bash
   python -m tools.guardian ui-audit
   ```
2. Verify token parity between `firmware/BmoGameboy/src/core/theme.h` and `tools/bmo_simulator/simulator.css`.
3. Check all interactive controls in the Web Simulator for `:focus-visible` accessibility.
4. Verify that no raw hex color literals were introduced into C++ display drawing functions.
5. Execute the full repository CI validator:
   ```bash
   python scripts/validate_repo.py
   ```
