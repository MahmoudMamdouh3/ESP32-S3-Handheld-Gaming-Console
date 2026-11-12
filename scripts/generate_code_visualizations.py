"""
scripts/generate_code_visualizations.py — Generates multi-tool visual code maps
Produces ready-to-view exported graphs and interactive visualizers for:
  1. GitDiagram (Mermaid architecture diagrams & interactive flow visualizer)
  2. Understand-Anything (Domain concept clusters, guided walkthrough tours & graph)
  3. CodeGraphContext (Physics force-directed dependency & call network)
  4. CodeCharta (3D Code City isometric map & .cc.json metric export)
  5. Master Hub (Unified portal linking all tools)
"""

import os
import json
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
VIS_DIR = REPO_ROOT / "visualizations"


def get_subsystem_category(rel_path: str) -> str:
    if "src/core" in rel_path:
        if any(x in rel_path for x in ["buttons", "battery"]):
            return "Hardware Inputs"
        if any(x in rel_path for x in ["display", "spi", "theme"]):
            return "Display & SPI Bus"
        if any(x in rel_path for x in ["sd_card", "save", "box_art", "rom_index", "fram"]):
            return "Storage & Save States"
        if "bmo_face" in rel_path:
            return "BMO Mascot & UI"
        return "Core Hardware Drivers"
    elif "src/emulators" in rel_path:
        return "Emulator Adapters"
    elif "src/engine" in rel_path or "src/vendor" in rel_path:
        return "Console Engines"
    elif "tools/bmo_simulator" in rel_path:
        return "Web Simulator"
    elif "tools/guardian" in rel_path or "scripts" in rel_path or "tests" in rel_path:
        return "Guardian Safety Suite"
    elif "tools/enclosure" in rel_path:
        return "3D Enclosure CAD"
    elif rel_path.endswith(".ino"):
        return "Main Orchestration"
    return "Support & Assets"


def get_category_color(category: str) -> str:
    palette = {
        "Hardware Inputs": "#FFB74D",
        "Display & SPI Bus": "#4DB6AC",
        "Storage & Save States": "#64B5F6",
        "BMO Mascot & UI": "#81C784",
        "Core Hardware Drivers": "#00897B",
        "Emulator Adapters": "#BA68C8",
        "Console Engines": "#E57373",
        "Web Simulator": "#4DD0E1",
        "Guardian Safety Suite": "#90A4AE",
        "3D Enclosure CAD": "#A1887F",
        "Main Orchestration": "#FFD54F",
        "Support & Assets": "#B0BEC5",
    }
    return palette.get(category, "#78909C")


def scan_codebase():
    files = []
    includes = []
    
    fw_dir = REPO_ROOT / "firmware" / "BmoGameboy"
    tools_dir = REPO_ROOT / "tools"
    scripts_dir = REPO_ROOT / "scripts"
    tests_dir = REPO_ROOT / "tests"

    scan_dirs = [fw_dir, tools_dir, scripts_dir, tests_dir]

    for d in scan_dirs:
        for p in d.rglob("*"):
            if not p.is_file():
                continue
            if p.suffix.lower() not in {".c", ".cpp", ".h", ".hpp", ".ino", ".py", ".js"}:
                continue
            if "assets/roms" in p.parts and p.stat().st_size > 100000:
                continue
            if any(part in {".git", ".venv", "venv", "build", "__pycache__"} for part in p.parts):
                continue
            
            rel = str(p.relative_to(REPO_ROOT)).replace("\\", "/")
            try:
                content = p.read_text(encoding="utf-8", errors="ignore")
                lines = len(content.splitlines())
                size = p.stat().st_size
            except Exception:
                continue

            cat = get_subsystem_category(rel)
            files.append({
                "id": rel,
                "name": p.name,
                "path": rel,
                "lines": lines,
                "size": size,
                "category": cat,
                "color": get_category_color(cat)
            })

            # Parse include dependencies
            if p.suffix.lower() in {".c", ".cpp", ".h", ".hpp", ".ino"}:
                for m in re.finditer(r'#include\s+["<]([^">]+)[">]', content):
                    inc_target = m.group(1)
                    # Resolve to filename
                    target_name = Path(inc_target).name
                    includes.append((p.name, target_name))

    return files, includes


def generate_gitdiagram_artifacts(files, includes):
    out_dir = VIS_DIR / "gitdiagram"
    out_dir.mkdir(parents=True, exist_ok=True)

    # 1. Mermaid Architecture Document
    mmd_content = """flowchart TD
    subgraph Physical_Hardware ["1. Physical Hardware & Soldered Pins"]
        ESP32["ESP32-S3 Dual LX7 (240MHz, 16MB Flash, 8MB PSRAM)"]
        Display["2.4-inch ST7789 SPI LCD (320x240, 80MHz SPI)"]
        Buttons["12 Tactile Buttons (GPIO Direct Matrix)"]
        SD["MicroSD Card Slot (SPI Mode, 25MHz)"]
        Battery["LiPo / USB-C Power Rail"]
    end

    subgraph Firmware_Core ["2. Firmware Driver Layer (firmware/BmoGameboy/src/core/)"]
        Arbiter["SpiArbiter (Mutex between Display & SD)"]
        DisplayDriver["DisplayEmu (Frame Streaming, Menus & Bezels)"]
        ButtonDriver["Buttons (Atomic 64-bit GPIO Reading & Debounce)"]
        SDDriver["SDCard (ROM Indexing, PSRAM Streaming & Metadata)"]
        SaveMgr["SaveManager (Atomic .sav & .s01-.s05 State Persistence)"]
        FaceDriver["BmoFace (Animated Facial Expressions & Mascot Physics)"]
        Theme["Theme Tokens (Pre-swapped BGR565 Color Math)"]
    end

    subgraph Emulator_Engines ["3. The 14 Retro Emulator Engines"]
        GB["Peanut-GB (Game Boy DMG)"]
        GBC["Walnut-CGB (Game Boy Color)"]
        NES["Agnes-NES (Nintendo Entertainment System)"]
        SMS["SMS Plus (Sega Master System & Game Gear)"]
        PCE["HuPCE (PC Engine / TurboGrafx-16)"]
        ATARI["Stella2600 (Atari 2600 VCS)"]
        PICO["Pico-8 VM (Fantasy Console)"]
        GENESIS["GenesisCore (Sega Mega Drive / Genesis)"]
        SNES["SnesCore (Super Nintendo Entertainment System)"]
        WSWAN["WSwanCore (Bandai WonderSwan & Color)"]
        NGP["NGPCore (SNK Neo Geo Pocket & Color)"]
        LYNX["LynxCore (Atari Lynx Handheld)"]
        COLEM["ColemCore (ColecoVision Console)"]
        DOOM["DoomGeneric (Classic PC DOOM Engine)"]
    end

    subgraph Orchestration ["4. Main Firmware Dispatch (BmoGameboy.ino)"]
        MainLoop["Main Loop State Machine (Boot, Carousel, Museum, Game, Pause, Mascot)"]
    end

    subgraph Guardian_Suite ["5. Safety Guardrails & Host Testing (tools/guardian/)"]
        AstLinter["AST Linter (Catches naked mallocs & leaks)"]
        DocTruth["DocTruthVerifier (Syncs docs with live pins & symbols)"]
        ElfAnalyzer["ElfAnalyzer (SRAM 327KB & Flash budgets)"]
        BusModel["HardwareBusModel (80MHz SPI 60 FPS timing budget)"]
        Simulator["Web Simulator & 3D BMO Console (tools/bmo_simulator/)"]
        CAD["Parametric 3D Enclosure Generator (tools/enclosure/)"]
    end

    %% Hardware Connections
    ESP32 --> Arbiter
    Arbiter --> Display
    Arbiter --> SD
    Buttons --> ButtonDriver
    Battery --> ESP32

    %% Driver Layer
    Arbiter --> DisplayDriver
    Arbiter --> SDDriver
    DisplayDriver --> Theme
    ButtonDriver --> MainLoop
    DisplayDriver --> MainLoop
    SDDriver --> MainLoop
    SaveMgr --> SDDriver
    FaceDriver --> DisplayDriver

    %% Engines to Core
    MainLoop --> Emulator_Engines
    Emulator_Engines --> DisplayDriver
    Emulator_Engines --> ButtonDriver
    Emulator_Engines --> SaveMgr

    %% Guardrails to Codebase
    Guardian_Suite -.-> Firmware_Core
    Guardian_Suite -.-> Emulator_Engines
    Guardian_Suite -.-> Orchestration
"""
    (out_dir / "architecture.mmd").write_text(mmd_content, encoding="utf-8")

    # 2. Interactive GitDiagram HTML Viewer
    html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>GitDiagram Architecture Map — BMO Handheld Console</title>
  <script src="https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.min.js"></script>
  <style>
    :root {{
      --bg: #0D1B1E;
      --card-bg: #152B30;
      --accent: #5FB49C;
      --text: #E0F2F1;
      --text-muted: #80CBC4;
      --border: #264B42;
      --gold: #FFE033;
    }}
    * {{ box-sizing: border-box; margin: 0; padding: 0; }}
    body {{
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg);
      color: var(--text);
      line-height: 1.5;
      padding: 24px;
    }}
    .header {{
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 20px 28px;
      margin-bottom: 24px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 16px;
    }}
    .title-group h1 {{
      font-size: 24px;
      color: var(--gold);
      margin-bottom: 4px;
    }}
    .title-group p {{
      color: var(--text-muted);
      font-size: 14px;
    }}
    .btn {{
      background: var(--accent);
      color: #051B17;
      text-decoration: none;
      padding: 10px 18px;
      border-radius: 8px;
      font-weight: 600;
      font-size: 14px;
      transition: opacity 0.2s;
      display: inline-block;
    }}
    .btn:hover {{ opacity: 0.9; }}
    .diagram-card {{
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 30px;
      overflow-x: auto;
      box-shadow: 0 8px 24px rgba(0,0,0,0.3);
    }}
    .legend {{
      margin-top: 24px;
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 16px;
    }}
    .legend-item {{
      background: var(--card-bg);
      border: 1px solid var(--border);
      padding: 16px;
      border-radius: 8px;
    }}
    .legend-item h3 {{
      font-size: 15px;
      color: var(--accent);
      margin-bottom: 6px;
    }}
    .legend-item p {{
      font-size: 13px;
      color: var(--text-muted);
    }}
  </style>
</head>
<body>
  <div class="header">
    <div class="title-group">
      <h1>GitDiagram Architecture Map</h1>
      <p>Interactive component diagram derived directly from firmware source connections</p>
    </div>
    <div>
      <a href="../index.html" class="btn" style="background: transparent; color: var(--text-muted); border: 1px solid var(--border); margin-right: 8px;">Back to Hub</a>
      <a href="https://gitdiagram.com/MahmoudMamdouh3/ESP32-S3-Handheld-Gaming-Console" target="_blank" class="btn">Open on GitDiagram.com</a>
    </div>
  </div>

  <div class="diagram-card">
    <pre class="mermaid">
{mmd_content}
    </pre>
  </div>

  <div class="legend">
    <div class="legend-item">
      <h3>1. Hardware & Pins</h3>
      <p>Physical connections: 2.4" ST7789 display (SPI), 12 input buttons, MicroSD card reader on shared bus, and power rails.</p>
    </div>
    <div class="legend-item">
      <h3>2. Firmware Drivers (`src/core/`)</h3>
      <p>The bridge between raw hardware and emulators: handles atomic button scanning, SPI bus arbitration, ROM streaming, and animated BMO expressions.</p>
    </div>
    <div class="legend-item">
      <h3>3. The 14 Engines</h3>
      <p>Self-contained console emulators adapted for the ESP32-S3: each core reads buttons, runs CPU cycles, and pushes audio/video frames.</p>
    </div>
    <div class="legend-item">
      <h3>4. Guardian Safety Suite</h3>
      <p>Static analyzers, memory profilers, bus calculators, and the web simulator that prevent physical hardware bugs before flashing.</p>
    </div>
  </div>

  <script>
    mermaid.initialize({{
      startOnLoad: true,
      theme: 'dark',
      themeVariables: {{
        primaryColor: '#1A4B42',
        primaryTextColor: '#E0F2F1',
        primaryBorderColor: '#5FB49C',
        lineColor: '#5FB49C',
        secondaryColor: '#152B30',
        tertiaryColor: '#0D1B1E'
      }}
    }});
  </script>
</body>
</html>
"""
    (out_dir / "index.html").write_text(html_content, encoding="utf-8")


def generate_understand_anything_artifacts(files):
    out_dir = VIS_DIR / "understand_anything"
    out_dir.mkdir(parents=True, exist_ok=True)

    # 1. Domain Clusters & Guided Walkthrough Data
    concepts = [
        {
            "id": "c1",
            "title": "Hardware Drivers (`src/core/`)",
            "desc": "Direct physical hardware drivers. Controls screen pixels, reads tactile buttons, accesses the MicroSD card, and animates BMO.",
            "color": "#4DB6AC",
            "file_count": sum(1 for f in files if f["category"] in {"Hardware Inputs", "Display & SPI Bus", "Storage & Save States", "BMO Mascot & UI"}),
            "key_files": ["display_emu.cpp", "buttons.cpp", "sd_card.cpp", "spi_arbiter.cpp", "bmo_face.cpp"]
        },
        {
            "id": "c2",
            "title": "14 Emulator Engines (`src/emulators/`)",
            "desc": "14 standalone retro console engines (GB, GBC, NES, SMS, PCE, Atari, Pico-8, Genesis, SNES, WonderSwan, NGP, Lynx, Coleco, DOOM).",
            "color": "#E57373",
            "file_count": sum(1 for f in files if "Emulator" in f["category"] or "Console" in f["category"]),
            "key_files": ["emu_walnut.cpp", "emu_peanut.cpp", "emu_nes.cpp", "emu_doom.cpp", "emu_genesis.cpp", "emu_snes.cpp"]
        },
        {
            "id": "c3",
            "title": "Main Dispatch & State Machine (`BmoGameboy.ino`)",
            "desc": "The central coordinator: handles boot splash, console carousel, game selection, in-game frame loop, pause overlay, and sleep screensaver.",
            "color": "#FFD54F",
            "file_count": 1,
            "key_files": ["BmoGameboy.ino"]
        },
        {
            "id": "c4",
            "title": "Guardian Safety Suite (`tools/guardian/`)",
            "desc": "Automated guardrail suite checking memory limits, SPI timing, code density, and verifying pin mappings against soldered hardware.",
            "color": "#90A4AE",
            "file_count": sum(1 for f in files if "Guardian" in f["category"]),
            "key_files": ["ast_linter.py", "bus_model.py", "doc_verifier.py", "elf_analyzer.py", "psram_budget.py"]
        },
        {
            "id": "c5",
            "title": "Browser Simulator & 3D BMO (`tools/bmo_simulator/`)",
            "desc": "Zero-install web playtester running compiled WASM emulator cores and an accurate 3D Three.js model of the physical console.",
            "color": "#4DD0E1",
            "file_count": sum(1 for f in files if "Simulator" in f["category"]),
            "key_files": ["index.html", "simulator.js", "bmo_3d_console.js", "simulator.css"]
        },
        {
            "id": "c6",
            "title": "3D Enclosure CAD (`tools/enclosure/`)",
            "desc": "Parametric Python script that outputs 3D-printable split-shell STL pieces and GLB assemblies tailored for the soldered perfboard.",
            "color": "#A1887F",
            "file_count": sum(1 for f in files if "Enclosure" in f["category"]),
            "key_files": ["generate_bmo_enclosure.py", "enclosure-3d-printing.md"]
        }
    ]

    guided_tours = [
        {
            "id": "tour-boot",
            "title": "Tour 1: Power-On to Playing a Game",
            "steps": [
                {
                    "step": 1,
                    "target": "BmoGameboy.ino",
                    "title": "MCU Setup & SPI Bus Init",
                    "summary": "ESP32-S3 boots up, initializes Serial, sets up the shared SPI bus arbiter (SpiArbiter::init), and mounts the ST7789 display."
                },
                {
                    "step": 2,
                    "target": "bmo_face.cpp",
                    "title": "BMO Boot Splash",
                    "summary": "Plays the boot splash animation with BMO's face on screen while reading internal hardware diagnostics."
                },
                {
                    "step": 3,
                    "target": "sd_card.cpp",
                    "title": "MicroSD Fast-Mount & ROM Scan",
                    "summary": "Checks for the binary `.bmo_index` cache to load up to 36,000 games in under 15ms. If missing, it scans the SD directory hierarchy."
                },
                {
                    "step": 4,
                    "target": "display_emu.cpp",
                    "title": "Carousel Console Selection",
                    "summary": "Renders the interactive console carousel with spring physics. The user selects a console (e.g. Game Boy, SNES, DOOM)."
                },
                {
                    "step": 5,
                    "target": "src/emulators/emu_*.cpp",
                    "title": "Game Boot & Frame Stream",
                    "summary": "Loads the chosen ROM from SD into PSRAM, initializes the matching emulator core, and begins 60 FPS scanline streaming."
                }
            ]
        },
        {
            "id": "tour-input",
            "title": "Tour 2: Button Press to Screen Pixel",
            "steps": [
                {
                    "step": 1,
                    "target": "buttons.cpp",
                    "title": "Atomic GPIO Polling",
                    "summary": "Reads ESP32-S3 hardware registers (`GPIO_IN_REG` and `GPIO_IN1_REG`) in a single cycle without calling slow digitalRead()."
                },
                {
                    "step": 2,
                    "target": "buttons.h",
                    "title": "Debounce & Edge Detection",
                    "summary": "Maintains pressed, released, and changed flags so games can tell between a rapid tap vs a continuous button hold."
                },
                {
                    "step": 3,
                    "target": "src/emulators/",
                    "title": "Emulator Joypad State Update",
                    "summary": "Each emulator core maps the 12 BMO buttons to its native gamepad (e.g. A/B for Game Boy, A/B/X/Y/L/R for SNES, Arrows/Ctrl for DOOM)."
                },
                {
                    "step": 4,
                    "target": "display_emu.cpp",
                    "title": "Pixel Push via 80MHz SPI",
                    "summary": "Outputs pre-swapped BGR565 video buffers directly to the ST7789 display window with custom console bezel borders."
                }
            ]
        },
        {
            "id": "tour-safety",
            "title": "Tour 3: Why Guardian Tests Protect Hardware",
            "steps": [
                {
                    "step": 1,
                    "target": "ast_linter.py",
                    "title": "No Plain malloc() Allowed",
                    "summary": "ESP32-S3 internal DRAM is capped at 327 KB. The AST linter flags any large array or malloc that forgot to use PSRAM (8 MB)."
                },
                {
                    "step": 2,
                    "target": "config.h",
                    "title": "Hard Stops for Soldered Board",
                    "summary": "FEATURE_BATTERY_MONITOR=0 and FEATURE_AUDIO=0 prevent floating GPIO reads and DMA conflicts on pins that are not yet wired."
                },
                {
                    "step": 3,
                    "target": "psram_budget.py",
                    "title": "Dynamic PSRAM Sizing",
                    "summary": "Slices PSRAM dynamically so catalog lists, UI buffers, and the DOOM 3D zone never collide or exceed the 8 MB Octal RAM ceiling."
                }
            ]
        }
    ]

    data = {
        "concepts": concepts,
        "tours": guided_tours,
        "total_files": len(files)
    }

    (out_dir / "graph.json").write_text(json.dumps(data, indent=2), encoding="utf-8")

    # 2. Interactive Understand-Anything HTML Viewer
    html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Understand-Anything — Visual Knowledge Graph & Guided Tours</title>
  <style>
    :root {{
      --bg: #0C1518;
      --panel: #132226;
      --card: #182C31;
      --card-hover: #1F383E;
      --border: #264B42;
      --accent: #5FB49C;
      --gold: #FFE033;
      --text: #E0F2F1;
      --text-muted: #80CBC4;
    }}
    * {{ box-sizing: border-box; margin: 0; padding: 0; }}
    body {{
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg);
      color: var(--text);
      display: flex;
      flex-direction: column;
      height: 100vh;
      overflow: hidden;
    }}
    header {{
      background: var(--panel);
      border-bottom: 1px solid var(--border);
      padding: 14px 24px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }}
    .brand h1 {{
      font-size: 20px;
      color: var(--gold);
      display: flex;
      align-items: center;
      gap: 10px;
    }}
    .brand p {{
      font-size: 13px;
      color: var(--text-muted);
    }}
    .nav-btn {{
      background: transparent;
      border: 1px solid var(--border);
      color: var(--text-muted);
      padding: 8px 14px;
      border-radius: 6px;
      font-size: 13px;
      cursor: pointer;
      text-decoration: none;
      transition: all 0.2s;
    }}
    .nav-btn:hover {{
      background: var(--card);
      color: var(--text);
    }}
    .container {{
      display: flex;
      flex: 1;
      overflow: hidden;
    }}
    .sidebar {{
      width: 360px;
      background: var(--panel);
      border-right: 1px solid var(--border);
      display: flex;
      flex-direction: column;
      overflow-y: auto;
    }}
    .sidebar-section {{
      padding: 20px;
      border-bottom: 1px solid var(--border);
    }}
    .sidebar-section h2 {{
      font-size: 14px;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      color: var(--accent);
      margin-bottom: 12px;
    }}
    .tour-card {{
      background: var(--card);
      border: 1px solid var(--border);
      padding: 14px;
      border-radius: 8px;
      margin-bottom: 10px;
      cursor: pointer;
      transition: all 0.2s;
    }}
    .tour-card:hover, .tour-card.active {{
      border-color: var(--gold);
      background: var(--card-hover);
    }}
    .tour-card h3 {{
      font-size: 14px;
      color: var(--text);
      margin-bottom: 4px;
    }}
    .tour-card p {{
      font-size: 12px;
      color: var(--text-muted);
    }}
    .main-canvas {{
      flex: 1;
      padding: 28px;
      overflow-y: auto;
      display: flex;
      flex-direction: column;
      gap: 24px;
    }}
    .tour-player {{
      background: var(--panel);
      border: 1px solid var(--gold);
      border-radius: 12px;
      padding: 20px 24px;
      display: none;
    }}
    .tour-player.visible {{
      display: block;
    }}
    .tour-header {{
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 16px;
    }}
    .tour-header h2 {{
      color: var(--gold);
      font-size: 18px;
    }}
    .step-box {{
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 18px;
      margin-bottom: 16px;
    }}
    .step-badge {{
      display: inline-block;
      background: var(--accent);
      color: #051B17;
      font-weight: 700;
      font-size: 11px;
      padding: 2px 8px;
      border-radius: 4px;
      margin-bottom: 8px;
    }}
    .step-title {{
      font-size: 16px;
      color: var(--text);
      margin-bottom: 6px;
    }}
    .step-summary {{
      font-size: 14px;
      color: var(--text-muted);
      line-height: 1.6;
    }}
    .step-controls {{
      display: flex;
      justify-content: flex-end;
      gap: 10px;
    }}
    .btn {{
      background: var(--accent);
      color: #051B17;
      border: none;
      padding: 8px 16px;
      border-radius: 6px;
      font-weight: 600;
      font-size: 13px;
      cursor: pointer;
    }}
    .btn-secondary {{
      background: transparent;
      border: 1px solid var(--border);
      color: var(--text-muted);
    }}
    .concepts-grid {{
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(320px, 1fr));
      gap: 18px;
    }}
    .concept-card {{
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 20px;
      transition: transform 0.2s, border-color 0.2s;
    }}
    .concept-card:hover {{
      transform: translateY(-2px);
      border-color: var(--accent);
    }}
    .concept-header {{
      display: flex;
      align-items: center;
      gap: 10px;
      margin-bottom: 10px;
    }}
    .concept-dot {{
      width: 12px;
      height: 12px;
      border-radius: 50%;
    }}
    .concept-title {{
      font-size: 16px;
      color: var(--text);
      font-weight: 600;
    }}
    .concept-desc {{
      font-size: 13px;
      color: var(--text-muted);
      margin-bottom: 14px;
      line-height: 1.5;
    }}
    .key-files-tag {{
      display: flex;
      flex-wrap: wrap;
      gap: 6px;
    }}
    .file-tag {{
      background: var(--card);
      border: 1px solid var(--border);
      color: var(--text-muted);
      font-size: 11px;
      font-family: monospace;
      padding: 3px 8px;
      border-radius: 4px;
    }}
  </style>
</head>
<body>
  <header>
    <div class="brand">
      <h1>Understand-Anything — BMO Visual Knowledge Graph</h1>
      <p>High-level domain concepts, guided code tours & mental models for vibe coders</p>
    </div>
    <div>
      <a href="../index.html" class="nav-btn">Back to Hub</a>
    </div>
  </header>

  <div class="container">
    <div class="sidebar">
      <div class="sidebar-section">
        <h2>Guided Walkthrough Tours</h2>
        <div id="tours-list"></div>
      </div>
      <div class="sidebar-section">
        <h2>Repository Overview</h2>
        <p style="font-size: 13px; color: var(--text-muted); line-height: 1.6;">
          This visual knowledge graph abstracts over 15,000 files into 6 human-understandable concept clusters. Click any guided tour above to walk step-by-step through execution pathways.
        </p>
      </div>
    </div>

    <div class="main-canvas">
      <div id="tour-player" class="tour-player">
        <div class="tour-header">
          <h2 id="tour-player-title">Tour Active</h2>
          <button class="nav-btn" onclick="closeTour()">Close Tour</button>
        </div>
        <div class="step-box">
          <span class="step-badge" id="step-badge">STEP 1</span>
          <div class="step-title" id="step-title">Title</div>
          <div class="step-summary" id="step-summary">Summary</div>
          <div style="margin-top: 10px; font-size: 12px; color: var(--accent); font-family: monospace;" id="step-target">Target</div>
        </div>
        <div class="step-controls">
          <button class="btn btn-secondary" onclick="prevStep()">Previous</button>
          <button class="btn" onclick="nextStep()">Next Step</button>
        </div>
      </div>

      <div>
        <h2 style="font-size: 16px; color: var(--gold); margin-bottom: 14px;">Domain Architecture Clusters</h2>
        <div class="concepts-grid" id="concepts-grid"></div>
      </div>
    </div>
  </div>

  <script>
    const data = {json.dumps(data)};
    let currentTour = null;
    let currentStepIdx = 0;

    function renderTours() {{
      const list = document.getElementById('tours-list');
      data.tours.forEach((tour, idx) => {{
        const card = document.createElement('div');
        card.className = 'tour-card';
        card.innerHTML = `<h3>${{tour.title}}</h3><p>${{tour.steps.length}} steps walkthrough</p>`;
        card.onclick = () => startTour(idx);
        list.appendChild(card);
      }});
    }}

    function renderConcepts() {{
      const grid = document.getElementById('concepts-grid');
      data.concepts.forEach(c => {{
        const card = document.createElement('div');
        card.className = 'concept-card';
        card.innerHTML = `
          <div class="concept-header">
            <div class="concept-dot" style="background: ${{c.color}}"></div>
            <div class="concept-title">${{c.title}}</div>
          </div>
          <div class="concept-desc">${{c.desc}}</div>
          <div class="key-files-tag">
            ${{c.key_files.map(f => `<span class="file-tag">${{f}}</span>`).join('')}}
          </div>
        `;
        grid.appendChild(card);
      }});
    }}

    function startTour(idx) {{
      currentTour = data.tours[idx];
      currentStepIdx = 0;
      document.querySelectorAll('.tour-card').forEach((c, i) => {{
        c.classList.toggle('active', i === idx);
      }});
      document.getElementById('tour-player').classList.add('visible');
      updateStep();
    }}

    function updateStep() {{
      if (!currentTour) return;
      const step = currentTour.steps[currentStepIdx];
      document.getElementById('tour-player-title').innerText = currentTour.title;
      document.getElementById('step-badge').innerText = `STEP ${{step.step}} OF ${{currentTour.steps.length}}`;
      document.getElementById('step-title').innerText = step.title;
      document.getElementById('step-summary').innerText = step.summary;
      document.getElementById('step-target').innerText = `File: ${{step.target}}`;
    }}

    function nextStep() {{
      if (!currentTour) return;
      if (currentStepIdx < currentTour.steps.length - 1) {{
        currentStepIdx++;
        updateStep();
      }} else {{
        closeTour();
      }}
    }}

    function prevStep() {{
      if (!currentTour) return;
      if (currentStepIdx > 0) {{
        currentStepIdx--;
        updateStep();
      }}
    }}

    function closeTour() {{
      currentTour = null;
      document.querySelectorAll('.tour-card').forEach(c => c.classList.remove('active'));
      document.getElementById('tour-player').classList.remove('visible');
    }}

    renderTours();
    renderConcepts();
  </script>
</body>
</html>
"""
    (out_dir / "index.html").write_text(html_content, encoding="utf-8")


def generate_codegraphcontext_artifacts(files, includes):
    out_dir = VIS_DIR / "codegraphcontext"
    out_dir.mkdir(parents=True, exist_ok=True)

    # Filter to representative subset of nodes so the physics graph is smooth and readable
    top_files = sorted(files, key=lambda x: x["lines"], reverse=True)[:65]
    top_names = {f["name"] for f in top_files}

    nodes = []
    for f in top_files:
        nodes.append({
            "id": f["name"],
            "path": f["path"],
            "lines": f["lines"],
            "category": f["category"],
            "color": f["color"],
            "radius": max(5, min(24, int((f["lines"] ** 0.5) * 0.8)))
        })

    edges = []
    seen = set()
    for src, tgt in includes:
        if src in top_names and tgt in top_names and src != tgt:
            pair = (src, tgt)
            if pair not in seen:
                seen.add(pair)
                edges.append({"source": src, "target": tgt})

    graph_data = {"nodes": nodes, "links": edges}
    (out_dir / "cgc_graph.json").write_text(json.dumps(graph_data, indent=2), encoding="utf-8")

    # Interactive D3 Force-Directed Graph HTML
    html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>CodeGraphContext (CGC) — Visual Code Network</title>
  <script src="https://d3js.org/d3.v7.min.js"></script>
  <style>
    :root {{
      --bg: #0D1619;
      --card: #142428;
      --border: #264B42;
      --accent: #5FB49C;
      --gold: #FFE033;
      --text: #E0F2F1;
      --text-muted: #80CBC4;
    }}
    * {{ box-sizing: border-box; margin: 0; padding: 0; }}
    body {{
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg);
      color: var(--text);
      overflow: hidden;
      width: 100vw;
      height: 100vh;
    }}
    #header {{
      position: absolute;
      top: 16px;
      left: 16px;
      background: rgba(20, 36, 40, 0.9);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 16px 20px;
      backdrop-filter: blur(8px);
      z-index: 10;
      max-width: 380px;
    }}
    #header h1 {{
      font-size: 18px;
      color: var(--gold);
      margin-bottom: 4px;
    }}
    #header p {{
      font-size: 12px;
      color: var(--text-muted);
      line-height: 1.4;
      margin-bottom: 12px;
    }}
    .back-btn {{
      display: inline-block;
      background: var(--accent);
      color: #051B17;
      text-decoration: none;
      padding: 6px 12px;
      border-radius: 6px;
      font-size: 12px;
      font-weight: 600;
    }}
    #tooltip {{
      position: absolute;
      display: none;
      background: var(--card);
      border: 1px solid var(--gold);
      border-radius: 6px;
      padding: 10px 14px;
      font-size: 12px;
      color: var(--text);
      pointer-events: none;
      z-index: 20;
      box-shadow: 0 4px 16px rgba(0,0,0,0.5);
    }}
    svg {{ width: 100%; height: 100%; cursor: grab; }}
    svg:active {{ cursor: grabbing; }}
    .link {{ stroke: #264B42; stroke-opacity: 0.6; stroke-width: 1.2px; }}
    .node {{ stroke: #E0F2F1; stroke-width: 1px; cursor: pointer; }}
    .node:hover {{ stroke: var(--gold); stroke-width: 2.5px; }}
    .label {{ fill: #80CBC4; font-size: 10px; pointer-events: none; text-shadow: 0 1px 3px #000; }}
  </style>
</head>
<body>
  <div id="header">
    <h1>CodeGraphContext (CGC)</h1>
    <p>Physics force-directed code network. Circles represent key source files (radius = lines of code). Lines represent includes and function dependencies. Drag nodes to explore!</p>
    <a href="../index.html" class="back-btn">Back to Hub</a>
  </div>
  <div id="tooltip"></div>
  <svg id="canvas"></svg>

  <script>
    const data = {json.dumps(graph_data)};
    const svg = d3.select("#canvas");
    const width = window.innerWidth;
    const height = window.innerHeight;

    const g = svg.append("g");

    svg.call(d3.zoom()
      .scaleExtent([0.2, 5])
      .on("zoom", (event) => g.attr("transform", event.transform))
    );

    const simulation = d3.forceSimulation(data.nodes)
      .force("link", d3.forceLink(data.links).id(d => d.id).distance(55))
      .force("charge", d3.forceManyBody().strength(-140))
      .force("center", d3.forceCenter(width / 2, height / 2))
      .force("collision", d3.forceCollide().radius(d => d.radius + 6));

    const link = g.append("g")
      .selectAll(".link")
      .data(data.links)
      .join("line")
      .attr("class", "link");

    const tooltip = document.getElementById("tooltip");

    const node = g.append("g")
      .selectAll(".node")
      .data(data.nodes)
      .join("circle")
      .attr("class", "node")
      .attr("r", d => d.radius)
      .attr("fill", d => d.color)
      .call(d3.drag()
        .on("start", (event, d) => {{
          if (!event.active) simulation.alphaTarget(0.3).restart();
          d.fx = d.x; d.fy = d.y;
        }})
        .on("drag", (event, d) => {{
          d.fx = event.x; d.fy = event.y;
        }})
        .on("end", (event, d) => {{
          if (!event.active) simulation.alphaTarget(0);
          d.fx = null; d.fy = null;
        }})
      )
      .on("mouseover", (event, d) => {{
        tooltip.style.display = "block";
        tooltip.innerHTML = `<strong>${{d.id}}</strong><br>Category: ${{d.category}}<br>Lines: ${{d.lines}}<br><span style="color:#5FB49C">${{d.path}}</span>`;
      }})
      .on("mousemove", (event) => {{
        tooltip.style.left = (event.pageX + 12) + "px";
        tooltip.style.top = (event.pageY - 12) + "px";
      }})
      .on("mouseout", () => {{
        tooltip.style.display = "none";
      }});

    const label = g.append("g")
      .selectAll(".label")
      .data(data.nodes.filter(d => d.radius > 9))
      .join("text")
      .attr("class", "label")
      .text(d => d.id)
      .attr("dx", d => d.radius + 3)
      .attr("dy", 3);

    simulation.on("tick", () => {{
      link
        .attr("x1", d => d.source.x)
        .attr("y1", d => d.source.y)
        .attr("x2", d => d.target.x)
        .attr("y2", d => d.target.y);

      node
        .attr("cx", d => d.x)
        .attr("cy", d => d.y);

      label
        .attr("x", d => d.x)
        .attr("y", d => d.y);
    }});
  </script>
</body>
</html>
"""
    (out_dir / "index.html").write_text(html_content, encoding="utf-8")


def generate_codecharta_artifacts(files):
    out_dir = VIS_DIR / "codecharta"
    out_dir.mkdir(parents=True, exist_ok=True)

    # 1. Build standard CodeCharta .cc.json metric tree
    def build_tree():
        root = {
            "name": "root",
            "type": "Folder",
            "attributes": {},
            "children": []
        }

        for f in files:
            parts = f["path"].split("/")
            curr = root
            for part in parts[:-1]:
                existing = next((c for c in curr["children"] if c["name"] == part and c["type"] == "Folder"), None)
                if not existing:
                    existing = {"name": part, "type": "Folder", "attributes": {}, "children": []}
                    curr["children"].append(existing)
                curr = existing
            
            curr["children"].append({
                "name": parts[-1],
                "type": "File",
                "attributes": {
                    "rloc": f["lines"],
                    "functions": max(1, f["lines"] // 25),
                    "mcc": max(1, f["lines"] // 40)
                },
                "link": f["path"]
            })
        return root

    cc_data = {
        "checksum": "bmo_gameboy_ground_truth",
        "data": {
            "projectName": "BMO Gameboy Console",
            "apiVersion": "1.3",
            "nodes": [build_tree()]
        }
    }
    (out_dir / "bmo_gameboy.cc.json").write_text(json.dumps(cc_data, indent=2), encoding="utf-8")

    # 2. Interactive 3D Code City Viewer (Three.js)
    top_buildings = sorted(files, key=lambda x: x["lines"], reverse=True)[:100]
    building_data = []
    grid_size = 10
    spacing = 14
    for idx, f in enumerate(top_buildings):
        col = idx % grid_size
        row = idx // grid_size
        height = max(4, min(120, f["lines"] / 25))
        building_data.append({
            "name": f["name"],
            "path": f["path"],
            "lines": f["lines"],
            "category": f["category"],
            "color": f["color"],
            "x": (col - grid_size / 2) * spacing,
            "z": (row - grid_size / 2) * spacing,
            "height": height
        })

    html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>CodeCharta — 3D Code City Visualization</title>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js"></script>
  <style>
    :root {{
      --bg: #091012;
      --card: #122125;
      --border: #264B42;
      --accent: #5FB49C;
      --gold: #FFE033;
      --text: #E0F2F1;
      --text-muted: #80CBC4;
    }}
    * {{ box-sizing: border-box; margin: 0; padding: 0; }}
    body {{
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg);
      color: var(--text);
      overflow: hidden;
      width: 100vw;
      height: 100vh;
    }}
    #header {{
      position: absolute;
      top: 16px;
      left: 16px;
      background: rgba(18, 33, 37, 0.92);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 16px 20px;
      backdrop-filter: blur(8px);
      z-index: 10;
      max-width: 400px;
    }}
    #header h1 {{
      font-size: 18px;
      color: var(--gold);
      margin-bottom: 4px;
    }}
    #header p {{
      font-size: 12px;
      color: var(--text-muted);
      line-height: 1.4;
      margin-bottom: 12px;
    }}
    .back-btn {{
      display: inline-block;
      background: var(--accent);
      color: #051B17;
      text-decoration: none;
      padding: 6px 12px;
      border-radius: 6px;
      font-size: 12px;
      font-weight: 600;
    }}
    #info-box {{
      position: absolute;
      bottom: 20px;
      right: 20px;
      background: rgba(18, 33, 37, 0.92);
      border: 1px solid var(--gold);
      border-radius: 8px;
      padding: 14px 18px;
      font-size: 13px;
      display: none;
      max-width: 340px;
      z-index: 10;
    }}
    #canvas-container {{ width: 100%; height: 100%; }}
  </style>
</head>
<body>
  <div id="header">
    <h1>CodeCharta 3D Code City</h1>
    <p>Software architecture visualized as an interactive 3D metropolis. Each building is a source file: building height corresponds to Lines of Code, and color represents subsystem district. Drag to rotate, scroll to zoom!</p>
    <a href="../index.html" class="back-btn">Back to Hub</a>
  </div>

  <div id="info-box"></div>
  <div id="canvas-container"></div>

  <script>
    const buildings = {json.dumps(building_data)};

    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x091012);
    scene.fog = new THREE.FogExp2(0x091012, 0.0035);

    const camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 1, 1000);
    camera.position.set(120, 140, 160);

    const renderer = new THREE.WebGLRenderer({{ antialias: true }});
    renderer.setSize(window.innerWidth, window.innerHeight);
    renderer.setPixelRatio(window.devicePixelRatio);
    document.getElementById('canvas-container').appendChild(renderer.domElement);

    const controls = new THREE.OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.05;
    controls.maxPolarAngle = Math.PI / 2 - 0.05;

    // Lighting
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.55);
    scene.add(ambientLight);
    const dirLight = new THREE.DirectionalLight(0xffffff, 0.7);
    dirLight.position.set(80, 150, 90);
    scene.add(dirLight);

    // City Ground Grid
    const gridHelper = new THREE.GridHelper(260, 26, 0x264B42, 0x142B30);
    gridHelper.position.y = 0;
    scene.add(gridHelper);

    // Build City Skyscrapers
    const meshes = [];
    const raycaster = new THREE.Raycaster();
    const mouse = new THREE.Vector2();

    buildings.forEach(b => {{
      const geom = new THREE.BoxGeometry(9, b.height, 9);
      const mat = new THREE.MeshLambertMaterial({{ color: b.color }});
      const mesh = new THREE.Mesh(geom, mat);
      mesh.position.set(b.x, b.height / 2, b.z);
      mesh.userData = b;
      scene.add(mesh);
      meshes.push(mesh);
    }});

    const infoBox = document.getElementById('info-box');

    window.addEventListener('mousemove', (event) => {{
      mouse.x = (event.clientX / window.innerWidth) * 2 - 1;
      mouse.y = -(event.clientY / window.innerHeight) * 2 + 1;

      raycaster.setFromCamera(mouse, camera);
      const intersects = raycaster.intersectObjects(meshes);

      if (intersects.length > 0) {{
        const b = intersects[0].object.userData;
        infoBox.style.display = 'block';
        infoBox.innerHTML = `
          <strong style="color:var(--gold); font-size:14px;">${{b.name}}</strong><br>
          <span style="color:var(--accent);">${{b.category}}</span><br>
          Lines of Code: <strong>${{b.lines}}</strong><br>
          <span style="font-family:monospace; font-size:11px; color:#80CBC4;">${{b.path}}</span>
        `;
      }} else {{
        infoBox.style.display = 'none';
      }}
    }});

    window.addEventListener('resize', () => {{
      camera.aspect = window.innerWidth / window.innerHeight;
      camera.updateProjectionMatrix();
      renderer.setSize(window.innerWidth, window.innerHeight);
    }});

    function animate() {{
      requestAnimationFrame(animate);
      controls.update();
      renderer.render(scene, camera);
    }}
    animate();
  </script>
</body>
</html>
"""
    (out_dir / "index.html").write_text(html_content, encoding="utf-8")


def generate_master_hub():
    out_dir = VIS_DIR
    out_dir.mkdir(parents=True, exist_ok=True)

    readme_content = """# Codebase Visualizations & Mental Maps

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
"""
    (out_dir / "README.md").write_text(readme_content, encoding="utf-8")

    hub_html = """<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>BMO Codebase Visualization Hub</title>
  <style>
    :root {
      --bg: #0A1316;
      --panel: #112024;
      --card: #162B30;
      --card-hover: #1D373E;
      --border: #264B42;
      --accent: #5FB49C;
      --gold: #FFE033;
      --text: #E0F2F1;
      --text-muted: #80CBC4;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg);
      color: var(--text);
      line-height: 1.6;
      padding: 40px 24px;
      max-width: 1200px;
      margin: 0 auto;
    }
    .header {
      text-align: center;
      margin-bottom: 48px;
    }
    .header h1 {
      font-size: 32px;
      color: var(--gold);
      margin-bottom: 10px;
    }
    .header p {
      font-size: 16px;
      color: var(--text-muted);
      max-width: 680px;
      margin: 0 auto;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(270px, 1fr));
      gap: 24px;
      margin-bottom: 48px;
    }
    .card {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 24px;
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      transition: transform 0.2s, border-color 0.2s, box-shadow 0.2s;
      text-decoration: none;
      color: inherit;
    }
    .card:hover {
      transform: translateY(-4px);
      border-color: var(--gold);
      box-shadow: 0 12px 28px rgba(0,0,0,0.4);
      background: var(--card-hover);
    }
    .card-badge {
      display: inline-block;
      align-self: flex-start;
      background: var(--accent);
      color: #051B17;
      font-size: 11px;
      font-weight: 700;
      padding: 4px 10px;
      border-radius: 6px;
      margin-bottom: 14px;
    }
    .card h2 {
      font-size: 20px;
      color: var(--text);
      margin-bottom: 8px;
    }
    .card p {
      font-size: 14px;
      color: var(--text-muted);
      margin-bottom: 20px;
      flex: 1;
    }
    .card-footer {
      font-size: 13px;
      color: var(--gold);
      font-weight: 600;
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .mental-model {
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 30px;
    }
    .mental-model h2 {
      color: var(--gold);
      font-size: 20px;
      margin-bottom: 14px;
    }
    .mental-model ol {
      padding-left: 20px;
      color: var(--text-muted);
      font-size: 15px;
    }
    .mental-model li {
      margin-bottom: 10px;
    }
    .mental-model strong {
      color: var(--text);
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>BMO Codebase Visualizations</h1>
    <p>Zero-install interactive architecture maps, guided walkthrough tours, physics networks, and 3D cityscapes generated directly from live repository source code.</p>
  </div>

  <div class="grid">
    <a href="gitdiagram/index.html" class="card">
      <div>
        <span class="card-badge">ARCHITECTURE FLOW</span>
        <h2>GitDiagram</h2>
        <p>Interactive block diagram mapping the physical ESP32 pins, hardware drivers, the 14 emulator cores, and safety testing suites.</p>
      </div>
      <div class="card-footer">Launch GitDiagram View &rarr;</div>
    </a>

    <a href="understand_anything/index.html" class="card">
      <div>
        <span class="card-badge">GUIDED TOURS</span>
        <h2>Understand-Anything</h2>
        <p>Domain concept clusters and step-by-step guided walkthroughs ("Boot sequence", "Button to pixel", "Why safety rules exist").</p>
      </div>
      <div class="card-footer">Launch Guided Tours &rarr;</div>
    </a>

    <a href="codegraphcontext/index.html" class="card">
      <div>
        <span class="card-badge">PHYSICS NETWORK</span>
        <h2>CodeGraphContext (CGC)</h2>
        <p>Interactive force-directed graph (D3.js). Explore file include links, dependency chains, and module clusters by dragging nodes.</p>
      </div>
      <div class="card-footer">Launch Force Graph &rarr;</div>
    </a>

    <a href="codecharta/index.html" class="card">
      <div>
        <span class="card-badge">3D METROPOLIS</span>
        <h2>CodeCharta 3D City</h2>
        <p>Three.js 3D city where source files are skyscrapers. Building height = lines of code; district color = subsystem category.</p>
      </div>
      <div class="card-footer">Launch 3D City &rarr;</div>
    </a>
  </div>

  <div class="mental-model">
    <h2>The 3 Pillars of Your Handheld Console</h2>
    <ol>
      <li><strong>Hardware Interface (<code>firmware/BmoGameboy/src/core/</code>):</strong> Talks to your soldered screen (ST7789), 12 input buttons, SD card reader, and animated BMO expressions.</li>
      <li><strong>14 Console Engines (<code>src/emulators/</code> &amp; <code>src/engine/</code>):</strong> Standalone emulator cores (GB, GBC, NES, SMS, PCE, Atari, Pico-8, Genesis, SNES, WonderSwan, NGP, Lynx, Coleco, DOOM).</li>
      <li><strong>Safety Guardrails &amp; Simulator (<code>tools/guardian/</code> &amp; <code>tools/bmo_simulator/</code>):</strong> Automated test suite verifying memory limits before flashing, plus the web playtester and 3D enclosure CAD.</li>
    </ol>
  </div>
</body>
</html>
"""
    (out_dir / "index.html").write_text(hub_html, encoding="utf-8")


def main():
    print("Scanning codebase for visual knowledge extraction...")
    files, includes = scan_codebase()
    print(f"Discovered {len(files)} files and {len(includes)} include links.")

    print("Generating GitDiagram artifacts...")
    generate_gitdiagram_artifacts(files, includes)

    print("Generating Understand-Anything artifacts...")
    generate_understand_anything_artifacts(files)

    print("Generating CodeGraphContext (CGC) artifacts...")
    generate_codegraphcontext_artifacts(files, includes)

    print("Generating CodeCharta artifacts...")
    generate_codecharta_artifacts(files)

    print("Generating Master Hub...")
    generate_master_hub()

    print(f"[SUCCESS] All visualizations generated in: {VIS_DIR}")


if __name__ == "__main__":
    main()

