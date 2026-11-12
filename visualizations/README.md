# Codebase Visualizations & Mental Maps

This folder contains interactive, zero-install visualizations for vibe coders to visually inspect and understand the BMO Gameboy handheld console repository.

## Available Visualizers

1. **[GitDiagram](./gitdiagram/index.html)**
   - Interactive system architecture diagram matching the viral GitDiagram tool.
   - Shows physical pins, firmware drivers, the 14 emulator cores, and safety rails.

2. **[Understand-Anything](./understand_anything/index.html)**
   - High-level domain concept clusters (Hardware, Emulators, Main Loop, Guardian).
   - Step-by-step guided walkthrough tours ("Boot sequence", "Button press to pixel", "Why tests exist").

3. **[CodeGraphContext (CGC)](./codegraphcontext/index.html)**
   - Physics-based interactive force-directed graph (D3.js).
   - Node circles represent source files (radius = lines of code); lines represent code dependencies.

4. **[CodeCharta 3D City](./codecharta/index.html)**
   - 3D code metropolis in Three.js where files appear as skyscrapers.
   - Height = Lines of Code; District Colors = Subsystem (Drivers, Cores, Simulator, Tools).
   - Includes standard `bmo_gameboy.cc.json` export for CodeCharta tools.

## How to View
Double-click `visualizations/index.html` in your file explorer to open the master portal in any web browser.
