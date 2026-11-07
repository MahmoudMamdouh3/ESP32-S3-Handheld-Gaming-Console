import json
import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
INO = (REPO_ROOT / "firmware" / "BmoGameboy" / "BmoGameboy.ino").read_text(encoding="utf-8")
SDD9 = (REPO_ROOT / "docs" / "software-design-document" / "section-09-part-01.md").read_text(encoding="utf-8")
SDD7 = (REPO_ROOT / "docs" / "software-design-document" / "section-07.md").read_text(encoding="utf-8")
FRAME_PACING = (REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "core" / "frame_pacing.h").read_text(encoding="utf-8")
MANIFEST = json.loads((REPO_ROOT / "AGENT_MANIFEST.json").read_text(encoding="utf-8"))


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

        launch_body = INO.split("// Boot appropriate emulator core", 1)[1].split("if (!success)", 1)[0]
        launch = {}
        for rom_type, wrapper, index in re.findall(
            r"selectedRom->type\s*==\s*(ROM_[A-Z0-9_]+)\s*\)\s*\{.*?"
            r"success\s*=\s*([A-Za-z0-9]+)::(?:begin|init)\(.*?\);\s*"
            r"selectedEmulatorIndex\s*=\s*(\d+)", launch_body, re.S
        ):
            launch[rom_type] = (wrapper, int(index))
        self.assertEqual(len(launch), 15)

        destroy_body = INO.split("static void destroyActiveEmulator()", 1)[1].split("if (currentRomBuffer)", 1)[0]
        frame_body = INO.split("if (selectedEmulatorIndex == 3) {\n      DoomEmu::runFrame();", 1)[1].split("unsigned long elapsed = micros() - frameStart", 1)[0]
        frame_body = "if (selectedEmulatorIndex == 3) { DoomEmu::runFrame(); }" + frame_body
        destroy_map = dispatch_map(destroy_body, "destroy")
        frame_map = dispatch_map(frame_body, "runFrame")
        frame_map.update(dispatch_map(frame_body, "update"))
        wrapper_to_file = {row[3]: row[4] for row in PLATFORMS}

        for row in PLATFORMS:
            rom_type, carousel_index, wrapper_index, wrapper, wrapper_file = row[:5]
            self.assertEqual(launch[rom_type], (wrapper, wrapper_index))
            self.assertEqual(destroy_map[wrapper_index], wrapper)
            self.assertEqual(frame_map[wrapper_index], wrapper)
            self.assertEqual(wrapper_to_file[wrapper], wrapper_file)

    def test_manifest_viewports_pacing_and_sdd_mapping_match(self):
        viewport_rows = re.findall(
            r"^\|\s*(\d+)\s*\|[^|]*\|\s*(\d+)×(\d+)\s*\|\s*(\d+),(\d+)\s*\|",
            SDD7.split("## 7.3 Configured frame viewports", 1)[1].split("## 7.4", 1)[0], re.M
        )
        viewports = {int(index): ((int(w), int(h)), (int(x), int(y))) for index, w, h, x, y in viewport_rows}
        manifest_platforms = MANIFEST["platforms"]
        sdd_rows = [line for line in SDD9.splitlines() if line.startswith("| `ROM_")]
        self.assertEqual(len(sdd_rows), 15)
        self.assertEqual(len(MANIFEST["emulators"]), 14)
        self.assertEqual(len({entry["wrapper"] for entry in MANIFEST["emulators"]}), 14)

        for row, sdd_line in zip(PLATFORMS, sdd_rows):
            rom_type, carousel_index, wrapper_index, wrapper, wrapper_file, extensions, viewport, offset, period, key = row
            self.assertEqual(viewports[wrapper_index], (viewport, offset))
            self.assertEqual(manifest_platforms[key]["ext"], extensions)
            self.assertIn(f"| `{rom_type}` | {carousel_index} | {wrapper_index} |", sdd_line)
            self.assertIn(wrapper_file, sdd_line)
            self.assertIn(", ".join(f"`{ext}`" for ext in extensions), sdd_line)
            self.assertIn(f"{viewport[0]}×{viewport[1]} @({offset[0]},{offset[1]})", sdd_line)
            self.assertIn(f"{period:,} µs", sdd_line)
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
