# Agent Instructions
This repo is agent-developed firmware for a physical, soldered ESP32-S3
handheld. Mistakes here can brick real hardware, not just fail a build.

**Before any task:** read `.agents/rules/README.md`, then every file it
marks "Always." Do not rely on this AGENTS.md file alone — it's a pointer,
not the ruleset.

**The two habits that matter most in this repo:**
1. Never state a code fact, test result, or symbol name you haven't
   verified in THIS session — see `.agents/rules/06_verification_standards.md`.
2. Physical hardware state (`.agents/rules/01_hardware.md`) always beats
   any design document's aspirational description — see
   `software-design-document.md`'s own warning banner.
