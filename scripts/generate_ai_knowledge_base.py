"""
scripts/generate_ai_knowledge_base.py — Automated AI Agent Knowledge Base & Graph Generator
Compiles an exhaustive, multi-dimensional machine-readable knowledge graph, AST index,
decision tree, and context manifest across all files, symbols, hardware pins, and performance baselines.
"""

import json
import re
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Dict

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))
FIRMWARE_DIR = REPO_ROOT / "firmware" / "BmoGameboy"
RULES_DIR = REPO_ROOT / ".agents" / "rules"

# Import Guardian Core for ground-truth telemetry
from tools.guardian.core.ast_linter import FirmwareAstLinter
from tools.guardian.core.bus_model import HardwareBusModel
from tools.guardian.core.elf_analyzer import ElfAnalyzer
from tools.guardian.core.host_bench import HostBenchmarkSuite


def parse_config_h(config_path: Path) -> Dict[str, int]:
    """Parses ground-truth #define integer constants directly from src/core/config.h."""
    defines: Dict[str, int] = {}
    if not config_path.exists():
        return defines
    content = config_path.read_text(encoding="utf-8", errors="ignore")
    for line in content.splitlines():
        clean = line.strip()
        m = re.match(r"^#define\s+([A-Z0-9_]+)\s+(\d+)\b", clean)
        if m:
            defines[m.group(1)] = int(m.group(2))
    return defines


def detect_engine_status(vendor_rel_path: str) -> str:
    """Inspects vendor engine source to determine 'production' vs 'stub' without stale dicts."""
    p = FIRMWARE_DIR / vendor_rel_path
    if not p.exists():
        return "stub"
    head = p.read_text(encoding="utf-8", errors="ignore")[:512]
    return "stub" if "STUB_ENGINE" in head else "production"


def build_knowledge_base():
    print("=" * 70)
    print(" BMO AI KNOWLEDGE BASE & AST INDEX COMPILER")
    print("=" * 70)

    now_iso = datetime.now().isoformat()

    # 1. Gather Ground-Truth Telemetry from Guardian
    print("\n[1/5] Gathering Hardware Bus, ELF, and Microbenchmark Baselines...")
    bus_model = HardwareBusModel()
    bus_metrics = bus_model.calculate_all()

    linter = FirmwareAstLinter(FIRMWARE_DIR)
    file_stats = linter.get_repo_line_stats()

    elf_analyzer = ElfAnalyzer(REPO_ROOT)
    elf_res = elf_analyzer.analyze()

    bench_suite = HostBenchmarkSuite()
    bench_metrics = bench_suite.run_all()

    # 2. Extract AST Symbols & Hardware Pin Accesses from Files
    print(f"\n[2/5] Parsing Symbols & AST Signatures across {len(file_stats)} files...")

    file_nodes: Dict[str, Any] = {}
    symbol_registry: Dict[str, Any] = {}

    for fs in file_stats:
        rel_str = fs.rel_path
        p = fs.file_path

        symbols_in_file = []
        pins_accessed = []
        includes = []

        try:
            if fs.category == "BAKED_ROMS" and p.stat().st_size > 262144:
                with p.open("r", encoding="utf-8", errors="ignore") as f:
                    head_lines = [f.readline() for _ in range(10)]
                for idx, line in enumerate(head_lines, 1):
                    clean = line.strip()
                    if clean.startswith("#include"):
                        inc = clean.replace("#include", "").strip().strip('<>"')
                        includes.append(inc)
                    m_arr = re.search(r"const\s+(?:uint8_t|unsigned\s+char)\s+([a-zA-Z0-9_]+)\s*\[", clean)
                    if m_arr:
                        sym_name = m_arr.group(1)
                        symbols_in_file.append(sym_name)
                        symbol_registry[sym_name] = {
                            "file": rel_str,
                            "line": idx,
                            "signature": clean.rstrip("{").strip(),
                            "is_iram": False,
                        }
            else:
                content = p.read_text(encoding="utf-8", errors="ignore")
                lines = content.splitlines()
                for idx, line in enumerate(lines, 1):
                    clean = line.strip()
                    if clean.startswith("#include"):
                        inc = clean.replace("#include", "").strip().strip('<>"')
                        includes.append(inc)
                    for pin_match in re.findall(
                        r"\b(PIN_[A-Z0-9_]+|TFT_[A-Z0-9_]+|SD_[A-Z0-9_]+|BTN_[A-Z0-9_]+|BUTTON_[A-Z0-9_]+|I2S_[A-Z0-9_]+|I2C_[A-Z0-9_]+|BATTERY_[A-Z0-9_]+|FEATURE_[A-Z0-9_]+|GPIO\d+)\b",
                        clean,
                    ):
                        if pin_match not in pins_accessed:
                            pins_accessed.append(pin_match)
                    if fs.category in {"FIRMWARE_CORE", "EMULATOR_WRAPPERS", "ENGINE_GBC", "FIRMWARE_TOP", "VENDOR_ENGINES"}:
                        fn_match = re.match(
                            r"^(?:static\s+)?(?:inline\s+)?(?:void|bool|int|uint\d+_t|int\d+_t|size_t|const\s+[a-zA-Z0-9_]+[\*\&]?|[a-zA-Z0-9_]+_t\*?)\s+([a-zA-Z0-9_]+::[a-zA-Z0-9_]+|[a-zA-Z0-9_]+)\s*\([^;]*\)\s*(?:const)?\s*\{?",
                            clean,
                        )
                        if (
                            fn_match
                            and not clean.startswith("if")
                            and not clean.startswith("for")
                            and not clean.startswith("while")
                            and not clean.startswith("return")
                        ):
                            sym_name = fn_match.group(1)
                            symbols_in_file.append(sym_name)
                            symbol_registry[sym_name] = {
                                "file": rel_str,
                                "line": idx,
                                "signature": clean.rstrip("{").strip(),
                                "is_iram": "IRAM_ATTR" in clean or (idx > 1 and "IRAM_ATTR" in lines[idx - 2]),
                            }
        except Exception:
            pass

        file_nodes[rel_str] = {
            "path": rel_str,
            "category": fs.category,
            "total_lines": fs.total_lines,
            "sloc": fs.sloc,
            "comments": fs.comment_lines,
            "blanks": fs.blank_lines,
            "density_percent": round(fs.code_density_percent, 1),
            "symbols": symbols_in_file[:20],
            "pins_accessed": pins_accessed,
            "includes": includes[:15],
        }

    # 3. Parse Governance Rules
    print("\n[3/5] Indexing Governance Rules and Intent Pathways...")
    rules_index = {}
    for rule_file in sorted(RULES_DIR.glob("*.md")):
        if rule_file.name == "README.md":
            continue
        try:
            txt = rule_file.read_text(encoding="utf-8", errors="ignore")
            title_match = re.search(r"^#\s+(.+)$", txt, re.MULTILINE)
            title = title_match.group(1) if title_match else rule_file.stem
            rules_index[rule_file.name] = {
                "file": f".agents/rules/{rule_file.name}",
                "title": title,
                "lines": len(txt.splitlines()),
            }
        except Exception:
            pass

    # 4. Construct Multi-Console Platform Matrix dynamically from vendor sources
    cfg = parse_config_h(FIRMWARE_DIR / "src" / "core" / "config.h")
    pce_status = "active" if detect_engine_status("src/vendor/pce/pce.c") == "production" else "stub"
    gen_status = "active" if detect_engine_status("src/vendor/genesis/genesis.c") == "production" else "stub"
    ws_status = "active" if detect_engine_status("src/vendor/wswan/wswan.c") == "production" else "stub"
    a26_status = "active" if detect_engine_status("src/vendor/stella/atari.c") == "production" else "stub"
    p8_status = "active" if detect_engine_status("src/vendor/pico/pico.c") == "production" else "stub"
    snes_status = "active" if detect_engine_status("src/vendor/snes/snes.c") == "production" else "stub"
    ngp_status = "active" if detect_engine_status("src/vendor/ngp/ngp.c") == "production" else "stub"
    lnx_status = "active" if detect_engine_status("src/vendor/lynx/lynx.c") == "production" else "stub"
    col_status = "active" if detect_engine_status("src/vendor/colem/colem.c") == "production" else "stub"

    platform_matrix = {
        "gb": {"name": "Game Boy DMG", "core": "Peanut-GB", "tier": 1, "status": "active", "res": "240x216 (3:2)", "ext": [".gb"], "fps": 59.73},
        "gbc": {"name": "Game Boy Color", "core": "Walnut-CGB", "tier": 1, "status": "active", "res": "240x216 (3:2)", "ext": [".gbc"], "fps": 59.73},
        "nes": {"name": "Nintendo NES", "core": "Agnes", "tier": 1, "status": "active", "res": "256x240", "ext": [".nes"], "fps": 60.10},
        "sms": {"name": "Sega Master System", "core": "SMS Z80+VDP Core", "tier": 1, "status": "active", "res": "256x192", "ext": [".sms"], "fps": 59.92},
        "gg": {"name": "Sega Game Gear", "core": "SMS Z80+VDP Core", "tier": 1, "status": "active", "res": "160x144", "ext": [".gg"], "fps": 59.92},
        "doom": {"name": "Classic DOOM", "core": "DoomGeneric", "tier": 1, "status": "active", "res": "320x200", "ext": [".wad"], "fps": 35.00},
        "pce": {"name": "PC Engine / TG-16", "core": "PCE HuC6280+VDC Core", "tier": 1, "status": pce_status, "res": "256x240", "ext": [".pce"], "fps": 59.82},
        "a26": {"name": "Atari 2600", "core": "Stella Core", "tier": 1, "status": a26_status, "res": "160x192", "ext": [".a26", ".bin"], "fps": 60.00},
        "p8": {"name": "PICO-8", "core": "PICO-8 Core", "tier": 1, "status": p8_status, "res": "128x128", "ext": [".p8", ".png"], "fps": 30.00},
        "gen": {"name": "Sega Genesis", "core": "Genesis 68000+VDP Core", "tier": 2, "status": gen_status, "res": "320x224", "ext": [".gen", ".md", ".smd"], "fps": 59.92},
        "snes": {"name": "Super Nintendo", "core": "SNES Core", "tier": 2, "status": snes_status, "res": "256x224", "ext": [".sfc", ".smc"], "fps": 60.10},
        "ws": {"name": "Bandai WonderSwan", "core": "WonderSwan V30MZ+VDC Core", "tier": 2, "status": ws_status, "res": "224x144", "ext": [".ws", ".wsc"], "fps": 75.47},
        "ngp": {"name": "Neo Geo Pocket", "core": "NGP Core", "tier": 2, "status": ngp_status, "res": "160x152", "ext": [".ngp", ".ngc"], "fps": 59.73},
        "lnx": {"name": "Atari Lynx", "core": "Lynx Core", "tier": 2, "status": lnx_status, "res": "160x102", "ext": [".lnx"], "fps": 75.00},
        "col": {"name": "ColecoVision", "core": "Coleco Core", "tier": 2, "status": col_status, "res": "256x192", "ext": [".col", ".sg"], "fps": 60.00},
    }

    # 5. Build Knowledge Graph Data Structure (Hardware Pins Sourced Directly From config.h)
    knowledge_graph = {
        "schema_version": "2.2.0",
        "generated_at": now_iso,
        "target_mcu": {
            "chip": "ESP32-S3-N16R8",
            "cpu_arch": "Xtensa dual-core 32-bit LX7 @ 240MHz",
            "flash": "16MB Quad SPI (QIO 80MHz)",
            "psram": "8MB Octal SPI (OPI 80MHz)",
            "internal_sram_dram_bytes": 327680,
            "iram_used_bytes": elf_res.iram_used_bytes,
            "dram_used_bytes": elf_res.dram_used_bytes,
            "flash_used_bytes": elf_res.flash_used_bytes,
        },
        "hard_stops": [
            {"id": "HARD_STOP_1", "rule": "Never read GPIO1 or set FEATURE_BATTERY_MONITOR != 0 (no divider soldered, prevents bootloop)."},
            {"id": "HARD_STOP_2", "rule": "Never set FEATURE_AUDIO != 0 (no I2S DAC soldered, prevents DMA hang)."},
            {"id": "HARD_STOP_3", "rule": "Never set FlashMode to OPI (requires FlashMode=qio, PSRAM=opi) or touch PSRAM pins GPIO 33-37 / strapping pins GPIO 0, 3, 45, 46."},
        ],
        "hardware_pins": {
            "display": {
                "tft_mosi": cfg.get("TFT_MOSI", 11),
                "tft_sck": cfg.get("TFT_SCK", 12),
                "tft_cs": cfg.get("TFT_CS", 10),
                "tft_dc": cfg.get("TFT_DC", 8),
                "tft_rst": cfg.get("TFT_RST", 9),
                "bus": "Shared 80MHz FSPI",
            },
            "sd_card": {
                "sd_mosi": cfg.get("TFT_MOSI", 11),
                "sd_miso": cfg.get("SD_MISO", 15),
                "sd_sck": cfg.get("TFT_SCK", 12),
                "sd_cs": cfg.get("SD_CS", 13),
                "clock_hz": 25000000,
            },
            "buttons": {
                "btn_up": cfg.get("BTN_UP", 4),
                "btn_down": cfg.get("BTN_DOWN", 5),
                "btn_left": cfg.get("BTN_LEFT", 6),
                "btn_right": cfg.get("BTN_RIGHT", 7),
                "btn_a": cfg.get("BTN_A", 16),
                "btn_b": cfg.get("BTN_B", 17),
                "btn_start": cfg.get("BTN_START", 18),
                "btn_select": cfg.get("BTN_SELECT", 21),
                "mode": "INPUT_PULLUP (Active LOW)",
            },
            "dormant_reserved": {
                "battery_adc_pin": cfg.get("BATTERY_ADC_PIN", 1),
                "i2s_bclk": cfg.get("I2S_BCLK", 38),
                "i2s_lrc": cfg.get("I2S_LRC", 39),
                "i2s_din": cfg.get("I2S_DIN", 40),
                "i2c_sda": cfg.get("I2C_SDA", 43),
                "i2c_scl": cfg.get("I2C_SCL", 44),
                "feature_sd_card": cfg.get("FEATURE_SD_CARD", 1),
                "feature_battery_monitor": cfg.get("FEATURE_BATTERY_MONITOR", 0),
                "feature_audio": cfg.get("FEATURE_AUDIO", 0),
            },
        },
        "platforms_matrix": platform_matrix,
        "bus_physics_model": [
            {
                "key": b.resolution_key,
                "name": b.resolution.name,
                "frame_bytes": b.resolution.frame_bytes,
                "spi_transfer_ms": round(b.spi_transfer_ms, 2),
                "target_fps": round(b.resolution.target_fps, 2),
                "sequential_cpu_budget_ms": round(b.sequential_cpu_budget_ms, 2),
                "sequential_cpu_budget_percent": round(b.sequential_cpu_budget_percent, 1),
                "spi_bus_utilization_percent": round(b.spi_bus_utilization_percent, 1),
                "dma_required": b.dma_required_for_60fps,
                "status": b.status,
            }
            for b in bus_metrics
        ],
        "microbenchmark_baselines": [
            {
                "name": bm.name,
                "iterations": bm.iterations,
                "avg_latency_us": round(bm.avg_latency_us, 3),
                "throughput_mops": round(bm.throughput_mops, 2),
                "description": bm.description,
            }
            for bm in bench_metrics
        ],
        "governance_rules": rules_index,
        "symbol_registry": symbol_registry,
        "files_index": file_nodes,
    }

    # 6. Build Decision Tree (Zero-Shot Agent Action Matrix)
    decision_tree = {
        "schema_version": "2.1.0",
        "description": "Zero-shot task intent routing table for autonomous AI coding agents.",
        "tasks": {
            "modify_gpio_or_pin": {
                "intent_keywords": ["pin", "gpio", "button", "hardware", "switch", "wiring", "remap", "turbo"],
                "mandatory_rules": ["01_hardware.md", "00_hard_stops.md"],
                "primary_files": ["firmware/BmoGameboy/src/core/config.h", "firmware/BmoGameboy/src/core/buttons.h", "firmware/BmoGameboy/src/core/buttons.cpp"],
                "guardrails": ["Never touch GPIO1 (battery)", "Never enable I2S audio GPIOs", "Never assign peripherals to strapping pins 0, 3, 45, 46 or PSRAM 33-37"],
                "verification_command": "python -m tools.guardian verify && python scripts/validate_repo.py",
            },
            "modify_display_rendering_or_spi": {
                "intent_keywords": ["display", "spi", "screen", "st7789", "scaling", "color", "bgr565", "palette", "framebuffer", "bezel"],
                "mandatory_rules": ["28_display_and_spi_contract.md", "15_performance_budgets.md", "39_performance_and_benchmark_framework.md"],
                "primary_files": ["firmware/BmoGameboy/src/core/display_emu.h", "firmware/BmoGameboy/src/core/display_emu.cpp"],
                "guardrails": ["Wire format is BGR565 byte-swapped", "Acquire SpiArbiter::lock() around SPI bus transactions"],
                "verification_command": "python -m tools.guardian bus-calc && python scripts/validate_repo.py",
            },
            "add_or_modify_emulator_core": {
                "intent_keywords": ["emulator", "core", "tier", "nes", "genesis", "snes", "doom", "peanut", "walnut", "sms", "atari", "lynx", "wswan", "ngp", "colem", "pico", "pce"],
                "mandatory_rules": ["12_extensibility_contract.md", "26_emulator_exit_contract.md", "32_modular_core_template.md"],
                "primary_files": ["firmware/BmoGameboy/src/emulators/", "firmware/BmoGameboy/BmoGameboy.ino"],
                "guardrails": ["Must implement destroy() to free PSRAM and wire into teardownCurrentEmulator()", "Must implement saveState/loadState/saveBatteryRam/loadBatteryRam", "Must include #pragma GCC optimize(\"O3,unroll-loops\")"],
                "verification_command": "python -m tools.guardian verify && python scripts/validate_repo.py",
            },
            "bake_rom_into_flash": {
                "intent_keywords": ["bake", "rom", "header", "flash", "app0", "aladdin", "mario", "zelda"],
                "mandatory_rules": ["29_adding_a_baked_rom.md", "37_rom_governance_and_flash_budget.md"],
                "primary_files": ["firmware/BmoGameboy/src/assets/roms/", "firmware/BmoGameboy/src/core/sd_card.cpp"],
                "guardrails": ["Check Flash headroom (stay under 8MB app0)", "Protect .rodata pointer in freeRom()"],
                "verification_command": "python scripts/test_runner.py && python scripts/validate_repo.py",
            },
            "bmo_mascot_face": {
                "intent_keywords": ["bmo", "face", "mascot", "sdf", "animation", "blink", "expression"],
                "mandatory_rules": ["35_bmo_face_contract.md", "08_ui_style_guide.md"],
                "primary_files": ["firmware/BmoGameboy/src/core/bmo_face.h", "firmware/BmoGameboy/src/core/bmo_face.cpp"],
                "guardrails": ["Preserve bounding-box spatial culling (PERF-12)", "Cache faceBuf in PSRAM"],
                "verification_command": "python -m tools.guardian bench-host && python -m tools.guardian verify",
            },
            "benchmark_and_performance_audit": {
                "intent_keywords": ["benchmark", "performance", "audit", "profile", "latency", "framerate", "fps", "memory"],
                "mandatory_rules": ["39_performance_and_benchmark_framework.md", "04_known_issues.md"],
                "primary_files": ["tools/guardian/", "docs/performance_report.md"],
                "guardrails": ["No throwaway scripts (Rule 2.1)", "Use tools.guardian framework"],
                "verification_command": "python -m tools.guardian audit && python -m tools.guardian report -o docs/performance_report.md",
            },
            "session_handoff_and_logging": {
                "intent_keywords": ["handoff", "changelog", "commit", "known issues", "log", "summary", "done"],
                "mandatory_rules": ["33_agent_handoff_and_optimization_cycle.md", "05_git_workflow.md", "06_verification_standards.md"],
                "primary_files": [".agents/rules/04_known_issues.md", "CHANGELOG.md"],
                "guardrails": ["Always use two-part commit messages: -m 'subject' -m '- bullets'", "Run validate_repo.py first", "No marketing hyperbole"],
                "verification_command": "python -m tools.guardian diff-audit && python scripts/validate_repo.py",
            },
            "critical_pushback_and_fact_checking": {
                "intent_keywords": ["idea", "question", "vibe", "change", "argue", "pushback", "suggest", "opinion", "why", "help"],
                "mandatory_rules": ["41_engineering_communication_and_critical_pushback.md", "00_hard_stops.md", "01_hardware.md"],
                "primary_files": [".agents/rules/41_engineering_communication_and_critical_pushback.md"],
                "guardrails": ["Fact-check against hardware limits before executing", "Provide scientific pushback and alternatives", "No marketing hyperbole"],
                "verification_command": "python scripts/validate_repo.py",
            },
            "save_states_and_persistence": {
                "intent_keywords": ["save", "save state", "slot", "restore", "pause menu", "battery ram", "persistence", "fram", "crc32", "screenshot"],
                "mandatory_rules": ["19_security_and_data_integrity.md", "28_display_and_spi_contract.md", "15_performance_budgets.md"],
                "primary_files": ["firmware/BmoGameboy/src/core/save_manager.h", "firmware/BmoGameboy/src/core/save_manager.cpp"],
                "guardrails": ["Must write to .tmp before atomic rename", "Must verify CRC32 and coreId on restore", "Must acquire SpiArbiter::lock()"],
                "verification_command": "python -m tools.guardian verify && python scripts/validate_repo.py",
            },
            "binary_rom_index_and_box_art": {
                "intent_keywords": ["rom index", "bmo_index", "cache", "box art", "cover art", "bmp", "raw", "catalog", "sort", "folder", "history"],
                "mandatory_rules": ["19_security_and_data_integrity.md", "15_performance_budgets.md", "28_display_and_spi_contract.md"],
                "primary_files": ["firmware/BmoGameboy/src/core/rom_index.h", "firmware/BmoGameboy/src/core/box_art.h", "firmware/BmoGameboy/src/core/box_art.cpp", "firmware/BmoGameboy/src/core/sd_card.h", "firmware/BmoGameboy/src/core/sd_card.cpp"],
                "guardrails": ["Allocate box art buffer strictly in PSRAM (8KB)", "Validate BMP 64x64 bounds", "Verify CRC32 of .bmo_index"],
                "verification_command": "python -m tools.guardian verify && python scripts/validate_repo.py",
            },
        },
    }

    # 7. Write Artifacts to Disk
    print("\n[4/5] Writing Output Artifacts...")

    kg_path = REPO_ROOT / "AGENT_KNOWLEDGE_GRAPH.json"
    kg_path.write_text(json.dumps(knowledge_graph, indent=2), encoding="utf-8")
    print(f"  * Generated: {kg_path} ({len(knowledge_graph['files_index'])} files, {len(symbol_registry)} symbols)")

    dt_path = REPO_ROOT / "AGENT_DECISION_TREE.json"
    dt_path.write_text(json.dumps(decision_tree, indent=2), encoding="utf-8")
    print(f"  * Generated: {dt_path} ({len(decision_tree['tasks'])} intent pathways)")

    manifest_path = REPO_ROOT / "AGENT_MANIFEST.json"
    emulators_list = [
        {"name": "Game Boy DMG", "wrapper": "src/emulators/emu_peanut.cpp", "engine_status": "production", "extensions": [".gb"]},
        {"name": "Game Boy Color", "wrapper": "src/emulators/emu_walnut.cpp", "engine_status": "production", "extensions": [".gbc"]},
        {"name": "Nintendo Entertainment System", "wrapper": "src/emulators/emu_nes.cpp", "engine_status": "production", "extensions": [".nes"]},
        {"name": "Classic DOOM", "wrapper": "src/emulators/emu_doom.cpp", "engine_status": "production", "extensions": [".wad"]},
        {"name": "Sega Master System", "wrapper": "src/emulators/emu_sms.cpp", "engine_status": "production", "extensions": [".sms", ".gg"]},
        {"name": "PC Engine", "wrapper": "src/emulators/emu_pce.cpp", "engine_status": detect_engine_status("src/vendor/pce/pce.c"), "extensions": [".pce"]},
        {"name": "Atari 2600", "wrapper": "src/emulators/emu_atari.cpp", "engine_status": detect_engine_status("src/vendor/stella/atari.c"), "extensions": [".a26"]},
        {"name": "PICO-8", "wrapper": "src/emulators/emu_pico.cpp", "engine_status": detect_engine_status("src/vendor/pico/pico.c"), "extensions": [".p8"]},
        {"name": "Sega Genesis", "wrapper": "src/emulators/emu_genesis.cpp", "engine_status": detect_engine_status("src/vendor/genesis/genesis.c"), "extensions": [".gen", ".md", ".smd"]},
        {"name": "Super Nintendo", "wrapper": "src/emulators/emu_snes.cpp", "engine_status": detect_engine_status("src/vendor/snes/snes.c"), "extensions": [".sfc", ".smc"]},
        {"name": "Bandai WonderSwan", "wrapper": "src/emulators/emu_wswan.cpp", "engine_status": detect_engine_status("src/vendor/wswan/wswan.c"), "extensions": [".ws", ".wsc"]},
        {"name": "Neo Geo Pocket", "wrapper": "src/emulators/emu_ngp.cpp", "engine_status": detect_engine_status("src/vendor/ngp/ngp.c"), "extensions": [".ngp", ".ngc"]},
        {"name": "Atari Lynx", "wrapper": "src/emulators/emu_lynx.cpp", "engine_status": detect_engine_status("src/vendor/lynx/lynx.c"), "extensions": [".lnx"]},
        {"name": "ColecoVision", "wrapper": "src/emulators/emu_colem.cpp", "engine_status": detect_engine_status("src/vendor/colem/colem.c"), "extensions": [".col", ".sg"]},
    ]
    manifest_data = {
        "manifest_version": "2.1.0",
        "last_updated": now_iso,
        "total_files": len(file_stats),
        "total_lines": sum(f.total_lines for f in file_stats),
        "total_sloc": sum(f.sloc for f in file_stats),
        "target_mcu": "ESP32-S3-N16R8 (16MB Flash, 8MB PSRAM)",
        "active_consoles": 15,
        "rules_count": len(rules_index),
        "iram_budget_used_bytes": elf_res.iram_used_bytes,
        "flash_budget_used_bytes": elf_res.flash_used_bytes,
        "dram_budget_used_bytes": elf_res.dram_used_bytes,
        "emulators": emulators_list,
        "platforms": platform_matrix,
    }
    manifest_path.write_text(json.dumps(manifest_data, indent=2), encoding="utf-8")
    print(f"  * Synchronized: {manifest_path}")

    ctx_path = RULES_DIR / "CONTEXT_INDEX.json"
    ctx_data = {
        "schema_version": "2.1.0",
        "last_updated": now_iso,
        "task_decision_table": {k: {"rules": v["mandatory_rules"], "files": v["primary_files"]} for k, v in decision_tree["tasks"].items()},
        "rules": rules_index,
    }
    ctx_path.write_text(json.dumps(ctx_data, indent=2), encoding="utf-8")
    print(f"  * Synchronized: {ctx_path}")

    print("\n[5/5] AI Knowledge Base compilation COMPLETE.")
    print("=" * 70)


if __name__ == "__main__":
    build_knowledge_base()
