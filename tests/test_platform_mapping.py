import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
INO = (REPO_ROOT / "firmware" / "BmoGameboy" / "BmoGameboy.ino").read_text(encoding="utf-8")
CORE_REGISTRY = (REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "cores" / "core_registry.h").read_text(encoding="utf-8")
COMPAT_DOC = (REPO_ROOT / "docs" / "compatibility.md").read_text(encoding="utf-8")
SD_CPP = (REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "storage" / "sd_card.cpp").read_text(encoding="utf-8")
DISPLAY_CPP = (REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "ui" / "display_emu.cpp").read_text(encoding="utf-8")
FRAME_PACING = (REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "ui" / "frame_pacing.h").read_text(encoding="utf-8")


PLATFORMS = [
    ("ROM_GB", 1, 1, "PeanutEmu", "emu_peanut.cpp", [".gb"], (240, 216), (40, 12), 16742, "gb"),
    ("ROM_GBC", 2, 0, "WalnutEmu", "emu_walnut.cpp", [".gbc"], (240, 216), (40, 12), 16742, "gbc"),
    ("ROM_NES", 3, 2, "NesEmu", "emu_nes.cpp", [".nes"], (256, 240), (32, 0), 16667, "nes"),
    ("ROM_WAD", 4, 3, "DoomEmu", "emu_doom.cpp", [".wad"], (320, 200), (0, 20), 28571, "doom"),
    ("ROM_SMS", 5, 4, "SmsEmu", "emu_sms.cpp", [".sms"], (256, 192), (32, 24), 16667, "sms"),
    ("ROM_GG", 6, 5, "SmsEmu", "emu_sms.cpp", [".gg"], (160, 144), (80, 48), 16667, "gg"),
    ("ROM_PCE", 7, 6, "PceEmu", "emu_pce.cpp", [".pce"], (256, 240), (32, 0), 16667, "pce"),
    ("ROM_ATARI", 8, 7, "AtariEmu", "emu_atari.cpp", [".a26"], (160, 192), (80, 24), 16667, "a26"),
    ("ROM_PICO8", 9, 8, "PicoEmu", "emu_pico.cpp", [".p8"], (128, 128), (96, 56), 33333, "p8"),
    ("ROM_GENESIS", 10, 9, "GenesisEmu", "emu_genesis.cpp", [".gen", ".md", ".smd"], (320, 224), (0, 8), 16667, "gen"),
    ("ROM_SNES", 11, 10, "SNESEmu", "emu_snes.cpp", [".sfc", ".smc"], (256, 224), (32, 8), 16667, "snes"),
    ("ROM_WSWAN", 12, 11, "WSwanEmu", "emu_wswan.cpp", [".ws", ".wsc"], (224, 144), (48, 48), 16667, "ws"),
    ("ROM_NGP", 13, 12, "NGPEmu", "emu_ngp.cpp", [".ngp", ".ngc"], (160, 152), (80, 44), 16667, "ngp"),
    ("ROM_LYNX", 14, 13, "LynxEmu", "emu_lynx.cpp", [".lnx"], (160, 102), (80, 69), 16667, "lnx"),
    ("ROM_COLEM", 15, 14, "ColemEmu", "emu_colem.cpp", [".col", ".sg"], (256, 192), (32, 24), 16667, "col"),
]


def dispatch_map(function_body, action):
    result = {}
    condition = re.compile(
        r"(?:if|else if)\s*\(\s*selectedEmulatorIndex\s*==\s*(\d+)"
        r"(?:\s*\|\|\s*selectedEmulatorIndex\s*==\s*(\d+))?\s*\)\s*\{([^}]*)\}",
        re.S,
    )
    for first, second, body in condition.findall(function_body):
        calls = re.findall(r"([A-Za-z0-9]+)::([A-Za-z0-9_]+)\(", body)
        matches = [name for name, method in calls if method == action]
        if not matches:
            continue
        for index in (first, second):
            if index:
                result[int(index)] = matches[-1]
    return result


class PlatformMappingTests(unittest.TestCase):
    def test_all_routes_and_documented_indices_agree(self):
        console_body = re.search(r"static const RomType CONSOLES\[\]\s*=\s*\{(.*?)\};", INO, re.S).group(1)
        consoles = re.findall(r"ROM_[A-Z0-9_]+", console_body)
        self.assertEqual(consoles, ["ROM_FAVORITES"] + [row[0] for row in PLATFORMS])
        self.assertEqual((len(consoles), len(PLATFORMS)), (16, 15))

        # Verify BmoGameboy.ino dispatches launch, frame stepping, palette cycling, and destroy through CoreRegistry.
        self.assertIn("CoreRegistry::findByRomType(selectedRom->type)", INO)
        self.assertIn("activeCore->start(selectedRom, romData, romSize)", INO)
        destroy_body = INO.split("static void destroyActiveEmulator()", 1)[1].split("if (currentRomBuffer)", 1)[0]
        self.assertIn("CoreRegistry::findByIndex(selectedEmulatorIndex)", destroy_body)
        self.assertIn("core->destroy()", destroy_body)
        self.assertIn("if (activeCore && activeCore->stepFrame)", INO)
        self.assertIn("activeCore->stepFrame()", INO)
        self.assertIn("activeCore->supportsDmgPalette", INO)

        table_match = re.search(r"kCores\[\]\s*=\s*\{(.*?)\};", CORE_REGISTRY, re.S)
        self.assertIsNotNone(table_match, "CoreRegistry::detail::kCores[] missing in core_registry.h")
        registry = {}
        for idx, rom_type, dmg, start_fn, step_fn, destroy_fn in re.findall(
            r"\{\s*(\d+)\s*,\s*(ROM_[A-Z0-9_]+)\s*,\s*(true|false)\s*,\s*"
            r"([A-Za-z0-9_:]+)\s*,\s*([A-Za-z0-9_:]+)\s*,\s*([A-Za-z0-9_:]+)\s*,",
            table_match.group(1),
        ):
            registry[rom_type] = (int(idx), dmg == "true", start_fn, step_fn, destroy_fn)
        self.assertEqual(len(registry), 15)

        wrapper_to_file = {row[3]: row[4] for row in PLATFORMS}
        for row in PLATFORMS:
            rom_type, carousel_index, wrapper_index, wrapper, wrapper_file = row[:5]
            reg_idx, dmg_pal, start_fn, step_fn, destroy_fn = registry[rom_type]
            self.assertEqual(reg_idx, wrapper_index)
            self.assertEqual(destroy_fn, f"{wrapper}::destroy")
            self.assertEqual(dmg_pal, rom_type in ("ROM_GB", "ROM_GBC"))
            start_impl = re.search(rf"\b{re.escape(start_fn)}\s*\([^\{{]*\{{(.*?)\}}", CORE_REGISTRY, re.S)
            self.assertIsNotNone(start_impl, f"Missing start helper {start_fn} in core_registry.h")
            self.assertIn(f"{wrapper}::", start_impl.group(1))
            if not step_fn.startswith(f"{wrapper}::"):
                step_impl = re.search(rf"\b{re.escape(step_fn)}\s*\([^\{{]*\{{(.*?)\}}", CORE_REGISTRY, re.S)
                self.assertIsNotNone(step_impl, f"Missing step helper {step_fn} in core_registry.h")
                self.assertIn(f"{wrapper}::", step_impl.group(1))
            self.assertEqual(wrapper_to_file[wrapper], wrapper_file)

    def test_compatibility_table_viewports_pacing_and_extensions_match(self):
        doc_rows = [line for line in COMPAT_DOC.splitlines() if line.startswith("| `ROM_")]
        self.assertEqual(len(doc_rows), 15)

        for row, doc_line in zip(PLATFORMS, doc_rows):
            rom_type, carousel_index, wrapper_index, wrapper, wrapper_file, extensions, viewport, offset, period, key = row
            self.assertIn(f"| `{rom_type}` | {carousel_index} | {wrapper_index} |", doc_line)
            self.assertIn(wrapper_file, doc_line)
            self.assertIn(", ".join(f"`{ext}`" for ext in extensions), doc_line)
            self.assertIn(f"{viewport[0]}×{viewport[1]} @({offset[0]},{offset[1]})", doc_line)
            self.assertIn(f"{period:,} µs", doc_line)
            for ext in extensions:
                self.assertIn(f'"{ext}"', SD_CPP)
            if key in ("gb", "gbc"):
                self.assertIn("16742U", FRAME_PACING)
            elif key == "doom":
                self.assertIn("28571U", FRAME_PACING)
            elif key == "p8":
                self.assertIn("33333U", FRAME_PACING)
            else:
                self.assertIn("default: return 16667U", FRAME_PACING)


if __name__ == "__main__":
    unittest.main()
