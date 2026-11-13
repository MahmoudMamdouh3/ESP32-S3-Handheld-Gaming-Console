# BMO Handheld Console — Design System & Visual Specification

**Target Platforms:**
1. **Embedded Physical Hardware:** 2.4" ST7789VW SPI TFT (Landscape 320×240, 16-bit BGR565 byte-swapped, Adafruit_GFX in Octal PSRAM).
2. **Web Simulator & Playtest Harness:** HTML5 Canvas 2D + WebAssembly Playtest Core (`simulator/`, 60 Hz 1:1 pixel rendering).

**Status:** Canonical Design Standard  
**Token Checks:** Verified against `firmware/BmoGameboy/src/ui/theme.h` and `simulator/simulator.css` by `python scripts/verify_color_tokens.py`.

---

## 1. Persona, Philosophy & Visual Identity

The BMO Gameboy interface synthesizes two design disciplines:
1. **Physical Handheld Console Ergonomics:** Clean tactile feedback, distinct button clusters, high visual hierarchy, and instant readability under handheld ambient light.
2. **Authentic Adventure Time Character Aesthetic:** OnionUI-inspired soft mint and teal chassis tones, crisp dark forest contrast, bright d-pad yellow, and coral red accents.

### Design Principles
- **Clarity Over Clutter:** Negative space and structural hierarchy replace nested containers. Information is grouped by spacing and typography rather than card-inside-card layering.
- **Strict Color Token Discipline:** UI pixels derive from a named semantic token in [`firmware/BmoGameboy/src/ui/theme.h`](firmware/BmoGameboy/src/ui/theme.h) or `:root` in [`simulator/simulator.css`](simulator/simulator.css). The face renderer has a separate named palette registered and checked below. Zero inline hex values are permitted in UI view logic.
- **Physical Motion Mechanics:** Interface motion adheres to second-order harmonic spring-damper equations ($\zeta \approx 0.72$, $\omega_n \approx 22.0\text{ rad/s}$). Transitions convey weight and mechanical tactile feedback without artificial delay.
- **Accessibility & Contrast:** All text meets or exceeds WCAG 2.1 AA contrast requirements ($\ge 4.5:1$ for body text, $\ge 3:1$ for large text and interactive borders).

---

## 2. Color Token Matrix

The color system maintains 1:1 fidelity between the embedded ST7789 display (BGR565 byte-swapped for SPI DMA) and the Web Simulator CSS custom properties.

### Primary Character & Chassis Palette

| Token Name | Semantic Role | Hex | RGB888 | Wire BGR565 (`Theme::*`) | Direct BGR565 | CSS Variable (`:root`) | WCAG Contrast vs BG |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `BMO_SCREEN_MINT` | Primary Screen Background | `#CEF5E4` | `(206, 245, 228)` | `0xB9E7` | `0xE7B9` | `--screen-mint` | 13.5:1 (vs Dark Forest) |
| `BMO_BODY_TEAL` | Chassis Shell & Primary Accent | `#5FB49C` | `(95, 180, 156)` | `0xAB9D` | `0x9DAB` | `--chassis-color` | 6.4:1 (vs Dark Forest) |
| `BMO_DEEP_TEAL` | Bezel Frame & Outer Card Surface | `#1A4B42` | `(26, 75, 66)` | `0x4342` | `0x4243` | `--bezel-color` | 9.6:1 (vs White Text) |
| `BMO_DARK_FOREST`| Mascot Eyes, Text & Deep Lens | `#0F2620` | `(15, 38, 32)` | `0x2121` | `0x2121` | `--bezel-lens` | 13.5:1 (vs Screen Mint) |
| `BMO_DPAD_YELLOW` | D-Pad, Active Badges & Highlights | `#FFE033` | `(255, 224, 51)` | `0x1F37` | `0x371F` | `--dpad-color` | 7.5:1 (vs Deep Teal) |
| `BMO_CORAL_RED` | B button fill, error accents and badges; use dark text on the fill | `#FF91A5` | `(255, 145, 165)` | `0x9FA4` | `0xA49F` | `--btn-b-color` | 4.6:1 (vs Deep Teal); 7.5:1 dark text |
| `BMO_BLUE_BUTTON` | A Button & Interactive Highlights | `#52A6BA` | `(82, 166, 186)` | `0x2ABD` | `0xBD2A` | `--btn-a-color` | 5.7:1 (vs Dark Forest) |
| `BMO_WHITE` | Primary Body Text & Icons | `#F8FDFA` | `(248, 253, 250)` | `0xFFFF` | `0xFFFF` | `--text-main` | 9.6:1 (vs Deep Teal) |
| `BMO_MOUTH_GREEN` | Mascot Expression & Success HUD | `#4CAF50` | `(76, 175, 80)` | `0x6955` | `0x5569` | `--btn-x-color` | 5.7:1 (vs Dark Forest) |
| `BMO_TONGUE_PINK` | Mascot Expression Accent | `#F48FB1` | `(244, 143, 177)` | `0x7EB4` | `0xB47E` | `--btn-y-color` | 7.1:1 (vs Dark Forest) |

### Face renderer palette

These renderer-local colors are packed by `bmo_face.cpp` for the face canvas. `COL_TONGUE` is the renderer's coral-pink tongue shade (`#FA7F8F`), distinct from the theme tongue pink (`#F48FB1`). The source RGB arguments, source hex comments, and registrations are checked by `python scripts/verify_color_tokens.py`.

| Source token | Registered RGB |
| :--- | :--- |
| `COL_MINT_BG` | `#8AD5C3` |
| `COL_DUSK_BG` | `#6FB8A7` |
| `COL_CHARCOAL` | `#101E2B` |
| `COL_CAVITY` | `#121C26` |
| `COL_BLUSH` | `#FF8BA7` |
| `COL_TONGUE` | `#FA7F8F` |
| `COL_GLINT` | `#FFFFFF` |
| `COL_STAR` | `#FFE066` |
| `COL_DREAM_Z` | `#D2F5EE` |
| `COL_ERROR_RED` | `#E12430` |

### Game Boy DMG 4-Shade Runtime Palettes

The Game Boy wrappers (`Peanut-GB` and `Walnut-CGB` in DMG mode) render indexed 2-bit DMG pixels mapped through 5 selectable hardware palettes:

1. **Classic Green (1989 Pea-Soup STN LCD):**
   - Shade 0: `#9BBC0F` (`0xD7EE`)
   - Shade 1: `#8BAC0F` (`0x2EAE`)
   - Shade 2: `#306230` (`0x444E`)
   - Shade 3: `#0F380F` (`0x0806`)
2. **BMO Mint / Teal:**
   - Shade 0: `#CEF5E4` (Mint Screen)
   - Shade 1: `#5FB49C` (Body Teal)
   - Shade 2: `#1A4B42` (Deep Forest)
   - Shade 3: `#0F2620` (Dark Forest)
3. **Pocket Gray (High-Contrast Silver FSTN):**
   - Shade 0: `#F5F5F5` (White)
   - Shade 1: `#AAAAAA` (Light Gray)
   - Shade 2: `#555555` (Dark Gray)
   - Shade 3: `#0A0A0A` (Black)
4. **Light Cyan (Backlit Indigo/Aqua):**
   - Shade 0: `#B4FFF0` (Bright Aqua)
   - Shade 1: `#50C8D2` (Vivid Cyan)
   - Shade 2: `#146478` (Deep Cyan)
   - Shade 3: `#021E28` (Abyss)
5. **Amber Phosphor (CRT Monochrome):**
   - Shade 0: `#FFD250` (Bright Amber)
   - Shade 1: `#DC9614` (Golden Amber)
   - Shade 2: `#784600` (Dark Amber)
   - Shade 3: `#140A00` (Obsidian)

---

## 3. Spacing, Grid & Sizing System

All spatial dimensions across both embedded C++ layouts and web stylesheets strictly align to an **8px base grid** with a **4px half-grid sub-unit**.

```
+-----------------------------------------------------------------+
| SPACING SYSTEM                                                  |
| 4px  (SPACING_XS)  : Pill interior padding, icon gaps, borders  |
| 8px  (SPACING_SM)  : Base grid unit, list item row padding      |
| 16px (SPACING_MD)  : Card padding, component separators         |
| 24px (SPACING_LG)  : Screen margins, carousel card gaps         |
| 32px (SPACING_XL)  : Outer layout gutters, header padding       |
| 48px (SPACING_2XL) : Section breaks                             |
+-----------------------------------------------------------------+
```

### Physical & Target Constraints
- **Web Simulator Pointer / Touch Minimum:** $44 \times 44\text{ pt}$ for all click targets (`.control-pill`, `.round-btn`, `.pill-btn`, `.dpad-btn`).
- **Embedded Screen Button Pills:** Minimum 24px height on the 320×240 display with at least 8px horizontal padding.
- **Container Interior Margin:** Minimum 8px interior padding between any outer border and contained text. Bleed to border is forbidden.

---

## 4. Typography Ladder

### Web Simulator Font Hierarchy
- **Header Badges & Arcade Branding:** `'Press Start 2P', monospace`
  - Sizes: `7px` (Pill button engravings), `8px` (HUD counters), `12px` (Core labels), `16px` (Title banners).
- **Interface & Control Text:** `'Outfit', sans-serif`
  - Weights: `400` (Regular), `600` (Semi-bold), `700` (Bold), `900` (Heavy engraved).
  - Sizes: `11px` (Engravings), `13px` (Pills & badges), `15px` (Body text), `18px` (Section headers), `24px` (Workspace titles).
- **Telemetry & Memory Debug:** `'JetBrains Mono', monospace`
  - Weights: `400` (Regular), `600` (Medium).
  - Sizes: `11px` (Hex dumps, register values), `12px` (Log entries), `14px` (FPS counters).

### Embedded Firmware Typography (ST7789 TFT)
- **Primary Menu & Item Text:** Adafruit_GFX default font with standard sizing constants:
  - `Theme::TEXT_SIZE_SMALL` (`1`): 6×8 px per glyph (up to 53 chars per 320px row).
  - `Theme::TEXT_SIZE_BODY`  (`1`): Standard list items and metadata.
  - `Theme::TEXT_SIZE_TITLE` (`2`): 12×16 px per glyph (up to 26 chars per 320px row).
- **Banners, Card Titles & Warnings:** `FreeSans9pt7b` and `FreeSans12pt7b` vector fonts included from Adafruit GFX (`src/ui/display_emu.cpp`).

---

## 5. Component Specifications & Interaction States

### A. Selection Indicator (Embedded Menus & Lists)
- **Invariant:** Menu selection states should use outline rectangles (`drawRoundRect`) rather than large solid fills for list cursors.
- **Architectural Rationale:** Avoids unnecessary pixel fills on selection highlights and separates item borders cleanly (`UI_YELLOW` / `UI_TEAL`).
- **Visual Spec:** Radius 4px, border color `Theme::BMO_DPAD_YELLOW` or `Theme::BMO_BODY_TEAL`, line thickness 1px or 2px.

### B. Interactive Control Pills & Buttons (Web Simulator)
Every interactive control MUST implement four distinct visual states:
1. **Default:** Subdued surface fill, subtle border (`1px solid var(--border-color)`).
2. **Hover:** Accent highlight (`var(--accent-cyan)` or `var(--accent-gold)`).
3. **Focus-Visible:** High-contrast focus ring (`outline: 2px solid var(--accent-gold); outline-offset: 2px;`) for keyboard navigation.
4. **Active / Pressed:** Physical depression transform (`transform: translateY(2px)`) with shadow collapse to simulate tactile mechanical switch travel.

### C. Floating Toast HUD
- **Dimensions:** $230 \times 22\text{ px}$ pill centered horizontally at $X = 45$, target $Y = 8$.
- **Surface:** `Theme::BMO_DARK_FOREST` background with 11px corner radius.
- **Border:** 1px `Theme::BMO_DPAD_YELLOW` for notifications, `Theme::BMO_CORAL_RED` for errors.
- **Typography:** `Theme::BMO_WHITE` centered single-line text.

### D. 1:1 Console Bezel & Lens
- **Aspect Ratio:** Exact 4:3 (320×240) active screen window surrounded by a 12px radius deep teal bezel (`#1A4B42`).
- **Screen Lens Glass:** Subtle top-to-bottom specular gradient (`rgba(255,255,255,0.08)` to `transparent`) positioned above the canvas layer with `pointer-events: none`.

---

## 6. Motion Laws & Dynamics

Transitions must not feel robotic (linear) or sluggish. All UI motion adheres to **second-order harmonic spring-damper dynamics**:

$$\ddot{x} + 2\zeta\omega_n \dot{x} + \omega_n^2 (x - x_{\text{target}}) = 0$$

- **Carousel Spring:** $\zeta = 0.72$, $\omega_n = 22.0\text{ rad/s}$ (`display_emu.cpp`).
- **Toast HUD Spring:** $\zeta = 0.75$, $\omega_n = 24.0\text{ rad/s}$ (`display_emu.cpp`).
- **Face Springs (`HarmonicSpring`):** Per-feature parameters in `bmo_face.cpp` ($\zeta \in [0.65, 0.75]$, $\omega_n \in [16, 24]\text{ rad/s}$).
- **Delta-Time Guard:** Carousel/Toast fall back to `0.01666 s` when `dt <= 0` or `dt > 0.050 s`; `HarmonicSpring::update()` caps `dt` at `0.05 s`.
- **Execution Invariant:** Firmware animations are stepped in non-blocking loop iterations using `millis()`/`micros()` delta math. Blocking calls (`delay()`) inside drawing routines are prohibited.

---

## 7. Anti-AI-Slop Visual Standards

This repository strictly forbids the visual clichés characteristic of untuned AI generation:

1. **No Nested Cards ("Card Nesting Syndrome"):** Never wrap a card inside another card. Use clean whitespace, hairline rule dividers, or distinct surface tones.
2. **No Arbitrary Purple/Indigo Meshes:** Never apply generic gradient wallpapers. Surfaces must be grounded in the intentional BMO palette (`#0B0F19`, `#1A4B42`, `#0F2620`).
3. **No Low-Contrast Text:** Gray-on-dark-gray text is prohibited. All text must pass WCAG AA $\ge 4.5:1$ contrast against its direct background.
4. **No Arbitrary Spacing:** Never use odd off-grid pixel margins (`7px`, `11px`, `13px`, `17px`, `19px`). Every dimension must be a multiple of 4px.
5. **No Missing Focus Indicators:** Every button and interactive control must provide an explicit `:focus-visible` ring.
6. **No Inlined Raw Hex Values:** UI code must consume semantic tokens from `theme.h` or `simulator.css`.

---

## 8. Verification & Tooling

Color parity, contrast calculations, and registered face-palette checks run via:
- `python scripts/verify_color_tokens.py`
- `python scripts/validate_repo.py`
