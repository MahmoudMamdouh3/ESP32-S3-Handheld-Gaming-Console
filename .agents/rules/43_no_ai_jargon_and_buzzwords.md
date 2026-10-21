# 43. Mandatory Ban on AI Jargon, Buzzwords, and Marketing Filler

**Purpose:** Eliminate AI-generated marketing filler, brand-name vanity adjectives, and hyperbolic buzzwords across every artifact in the repository—including C/C++ comments, Python docstrings, Markdown documentation, rule files, CLI outputs, and git commit messages.

---

## 1. Zero-Tolerance Scope

Every piece of text written by an agent MUST use plain, measurable engineering language. This rule applies to:
1. **Source Code Comments & Docstrings** (`firmware/`, `tools/`, `scripts/`, `tests/`)
2. **Documentation & Architecture Specs** (`README.md`, `CHANGELOG.md`, `docs/*.md`)
3. **Agent Rules & Handoff Logs** (`AGENTS.md`, `.agents/rules/*.md`)
4. **Git Commit Messages & PR Descriptions** (both subject line and bullet body)
5. **User-Facing Responses & Reports**

---

## 2. Banned Words & Phrases (Enforced by Automated Scanner)

The following buzzwords and marketing phrases are strictly banned across the repository and will fail `python -m tools.guardian verify`, `python scripts/validate_repo.py` (Phase 7), and `.git/hooks/commit-msg`:

- **Brand / Tier Vanity Labels:** `apple-grade`, `nintendo-grade`, `nintendo-style`, `apple/nintendo`, `museum-grade`, `reviewer-grade`, `production-grade`, `ultra-high-end`, `state-of-the-art`, `world-class`, `god-tier`, `next-gen`, `industry-leading`, `best-in-class`
- **Speed / Motion Hyperbole:** `buttery-smooth`, `blazing-fast`, `lightning-fast`, `supercharge`, `supercharged`
- **AI Prose Clichés:** `delve`, `tapestry`, `testament`, `paradigm`, `synergy`, `holistic`, `meticulously`, `bespoke`, `seamlessly`, `flawlessly`, `unparalleled`, `unmatched`, `groundbreaking`, `revolutionary`, `cutting-edge`, `game-changing`, `elevate`, `elevated`, `empower`, `empowered`, `living companion`, `authoritative living`, `glanceable`

---

## 3. Concrete Replacement Table

Always replace vague adjectives with exact technical facts, measurements, or algorithm names:

| Instead of writing (BANNED) | Write the exact engineering fact (REQUIRED) |
| :--- | :--- |
| "buttery-smooth carousel animation" | "60 Hz second-order spring-damper carousel transition ($\zeta = 0.72$, $\omega_n = 22.0\text{ rad/s}$)" |
| "blazing-fast ROM loading" | "64 KB chunked SPI read at 25 MHz (~1.5 s for 4 MB ROM)" |
| "Nintendo-grade glanceable button pills" | "rounded button badges (`drawButtonPill`) with contrasting text" |
| "Apple-grade floating HUD" | "230×22 px rounded status overlay (`showToast`) at $Y = 8$" |
| "Ultra-high-end living companion" | "320×240 2D SDF mascot renderer in Octal PSRAM (`BmoFace`)" |
| "meticulously gated hardware flags" | "compile-time `#if FEATURE_*` guards" |
| "seamlessly streams scanlines" | "streams 240-pixel RGB565 scanlines via Core 0 FreeRTOS queue" |
| "production-grade / reviewer-grade spec" | "technical specification" |

---

## 4. Automated Enforcement

1. **Repository Linter (`tools/guardian/core/doc_verifier.py`):**
   - Scans tracked `.md`, `.c`, `.cpp`, `.h`, `.ino`, `.py`, and `.js` files for banned buzzwords and emits `BUZZWORD_VIOLATION` errors.
2. **Git Commit Hook (`.git/hooks/commit-msg`):**
   - Rejects any commit message containing banned buzzwords before the commit is recorded.
