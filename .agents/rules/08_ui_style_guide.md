# UI & Visual Style Guide
Purpose: stop each new screen/menu from reinventing colors, spacing, and
widget style from scratch, and avoid the "default Adafruit_GFX look"
(flat black background, default font, filled rectangles) creeping back in
after hardware-notes.md section 8's UI performance lesson already pushed
the project away from it once.

See [DESIGN.md](file:///DESIGN.md) for the complete design system specification,
token matrix, and typography scale, and [.agents/rules/44_anti_ai_ui_design_standards.md](file:///e:/BMO%20Gameboy/.agents/rules/44_anti_ai_ui_design_standards.md)
for anti-AI visual guidelines.

## Central theme file
- All colors, spacing constants, and font references MUST live in one
  file (`src/core/theme.h` — create it if it doesn't exist yet, and
  migrate any hardcoded `0xXXXX` RGB565 literals found elsewhere into it
  as named constants, e.g. `THEME_BG`, `THEME_ACCENT`, `THEME_TEXT_DIM`).
- No new screen may introduce a raw hex color literal. If a new semantic
  color is genuinely needed, add it to theme.h with a name, don't inline
  it.

## Layout
- Define and use a consistent spacing unit (e.g. an 8px grid) for margins
  and padding between UI elements, instead of ad hoc pixel offsets per
  screen. Put the constant in theme.h.
- Menu selection state: use outline (`drawRoundRect`), never filled
  (`fillRoundRect`), per the existing measured performance finding in
  hardware-notes.md section 8 (UI Rendering Performance) — this is a
  standing rule now, not a one-off optimization.
- Avoid animated background elements (grids, moving decorations) behind
  menus — same section, same reasoning: measured cost, not a guess.

## Typography
- Pick ONE primary UI font and ONE monospace/debug font for the whole
  project; name both explicitly in theme.h comments. Don't let different
  screens silently use different default GFX fonts.
- Define standard text sizes as named constants (e.g. `TEXT_SIZE_TITLE`,
  `TEXT_SIZE_BODY`, `TEXT_SIZE_SMALL`) instead of literal `setTextSize(2)`
  calls scattered around.

## Motion & feedback
- Any animation (blink timing, transitions, loading indicators) must be
  driven by the existing non-blocking `millis()` delta pattern already
  established for BmoFace — never a blocking `delay()` inside a
  UI-drawing path, since that stalls button polling.
- New screens should have an explicit idle/loading/error visual state,
  not just a happy path — mirroring the crash/low-battery states BmoFace
  already defines.

## When adding a new screen
Checklist: uses theme.h constants only; uses the shared spacing grid;
reuses an existing menu/list widget if one exists rather than writing a
new one; documented in docs/ if it introduces a new navigable state in
the state machine.
